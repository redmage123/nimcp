/**
 * @file nimcp_speech_kernels.cu
 * @brief GPU Speech Processing CUDA Kernels
 *
 * WHAT: CUDA kernels for speech feature extraction
 * WHY:  GPU acceleration for speech recognition preprocessing
 * HOW:  Custom kernels for formant extraction, pitch detection, VAD
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "SPEECH_GPU"

#define BLOCK_SIZE 256

//=============================================================================
// Pitch Detection (Autocorrelation Method)
//=============================================================================

__global__ void kernel_autocorrelation(
    const float* signal, float* autocorr, int signal_len, int max_lag)
{
    int lag = blockIdx.x * blockDim.x + threadIdx.x;
    if (lag >= max_lag) return;

    float sum = 0.0f;
    for (int i = 0; i < signal_len - lag; i++) {
        sum += signal[i] * signal[i + lag];
    }
    autocorr[lag] = sum;
}

__global__ void kernel_find_pitch_peak(
    const float* autocorr, float* pitch_period, float* confidence,
    int n_frames, int frame_len, int min_lag, int max_lag)
{
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= n_frames) return;

    const float* ac = autocorr + frame * frame_len;

    // Find peak in valid pitch range
    float max_val = 0.0f;
    int max_idx = min_lag;

    for (int lag = min_lag; lag < max_lag && lag < frame_len; lag++) {
        if (ac[lag] > max_val) {
            max_val = ac[lag];
            max_idx = lag;
        }
    }

    pitch_period[frame] = (float)max_idx;
    confidence[frame] = ac[0] > 0.0f ? max_val / ac[0] : 0.0f;
}

bool nimcp_gpu_pitch_detect(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t* pitch,
    nimcp_gpu_tensor_t* confidence,
    float sample_rate, float min_f0, float max_f0)
{
    if (!ctx || !audio || !pitch) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int signal_len = audio->dims[audio->ndim - 1];
    int max_lag = (int)(sample_rate / min_f0);
    int min_lag = (int)(sample_rate / max_f0);

    // Compute autocorrelation
    float* d_autocorr;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_autocorr, max_lag * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_autocorrelation<<<(max_lag + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        (const float*)audio->data, d_autocorr, signal_len, max_lag);

    // Find pitch peaks
    kernel_find_pitch_peak<<<1, 1>>>(
        d_autocorr, (float*)pitch->data,
        confidence ? (float*)confidence->data : NULL,
        1, max_lag, min_lag, max_lag);

    cudaFree(d_autocorr);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Voice Activity Detection (Energy-based)
//=============================================================================

__global__ void kernel_frame_energy(
    const float* audio, float* energy, int n_frames, int frame_len, int hop_len)
{
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= n_frames) return;

    float sum = 0.0f;
    int start = frame * hop_len;
    for (int i = 0; i < frame_len && (start + i) < (n_frames * hop_len + frame_len); i++) {
        float val = audio[start + i];
        sum += val * val;
    }
    energy[frame] = sum / frame_len;
}

__global__ void kernel_vad_threshold(
    const float* energy, float* vad, int n_frames, float threshold)
{
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= n_frames) return;

    vad[frame] = energy[frame] > threshold ? 1.0f : 0.0f;
}

bool nimcp_gpu_vad(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t* vad,
    int frame_len, int hop_len, float threshold)
{
    if (!ctx || !audio || !vad) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int audio_len = audio->dims[audio->ndim - 1];
    int n_frames = (audio_len - frame_len) / hop_len + 1;

    // Compute frame energy
    float* d_energy;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_energy, n_frames * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_frame_energy<<<(n_frames + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        (const float*)audio->data, d_energy, n_frames, frame_len, hop_len);

    // Apply threshold
    kernel_vad_threshold<<<(n_frames + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        d_energy, (float*)vad->data, n_frames, threshold);

    cudaFree(d_energy);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Levinson-Durbin Algorithm for LPC
//=============================================================================

/**
 * @brief Single-frame Levinson-Durbin recursion
 *
 * WHAT: Solve Toeplitz system for LPC coefficients
 * WHY:  Efficient O(n^2) algorithm for LPC analysis
 * HOW:  Forward-backward recursion with reflection coefficients
 *
 * The algorithm solves: R * a = r where R is the autocorrelation Toeplitz matrix
 *
 * @param autocorr Autocorrelation coefficients R(0)...R(order)
 * @param lpc_coeffs Output LPC coefficients a(1)...a(order)
 * @param reflection_coeffs Output reflection coefficients k(1)...k(order)
 * @param error Output prediction error (final)
 * @param order LPC order
 */
__global__ void kernel_levinson_durbin_single(
    const float* __restrict__ autocorr,
    float* __restrict__ lpc_coeffs,
    float* __restrict__ reflection_coeffs,
    float* __restrict__ error,
    int order)
{
    // This is a sequential algorithm, run on single thread
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    // Temporary storage for previous iteration
    float a_prev[32];  // Support up to order 32
    float a_curr[32];

    if (order > 32) order = 32;

    // Initialize error with R(0)
    float E = autocorr[0];
    if (E <= 0.0f) {
        // Invalid autocorrelation, return zeros
        for (int i = 0; i < order; i++) {
            lpc_coeffs[i] = 0.0f;
            reflection_coeffs[i] = 0.0f;
        }
        *error = 0.0f;
        return;
    }

    // Levinson-Durbin recursion
    for (int i = 0; i < order; i++) {
        // Compute reflection coefficient k(i)
        float lambda = autocorr[i + 1];
        for (int j = 0; j < i; j++) {
            lambda -= a_prev[j] * autocorr[i - j];
        }
        float k = lambda / E;

        // Store reflection coefficient
        reflection_coeffs[i] = k;

        // Update LPC coefficients
        a_curr[i] = k;
        for (int j = 0; j < i; j++) {
            a_curr[j] = a_prev[j] - k * a_prev[i - 1 - j];
        }

        // Update error
        E = E * (1.0f - k * k);
        if (E <= 0.0f) {
            E = 1e-10f;  // Prevent negative/zero error
        }

        // Copy current to previous for next iteration
        for (int j = 0; j <= i; j++) {
            a_prev[j] = a_curr[j];
        }
    }

    // Copy final LPC coefficients to output
    for (int i = 0; i < order; i++) {
        lpc_coeffs[i] = a_curr[i];
    }
    *error = E;
}

/**
 * @brief Batched Levinson-Durbin for multiple frames
 *
 * @param autocorr_batch Autocorrelation [batch_size, order+1]
 * @param lpc_batch Output LPC coefficients [batch_size, order]
 * @param reflection_batch Output reflection coefficients [batch_size, order]
 * @param error_batch Output prediction errors [batch_size]
 * @param order LPC order
 * @param batch_size Number of frames
 */
__global__ void kernel_levinson_durbin_batch(
    const float* __restrict__ autocorr_batch,
    float* __restrict__ lpc_batch,
    float* __restrict__ reflection_batch,
    float* __restrict__ error_batch,
    int order,
    int batch_size)
{
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= batch_size) return;

    // Temporary storage
    float a_prev[32];
    float a_curr[32];

    if (order > 32) order = 32;

    const float* autocorr = autocorr_batch + frame * (order + 1);
    float* lpc = lpc_batch + frame * order;
    float* reflection = reflection_batch + frame * order;

    float E = autocorr[0];
    if (E <= 0.0f) {
        for (int i = 0; i < order; i++) {
            lpc[i] = 0.0f;
            reflection[i] = 0.0f;
        }
        error_batch[frame] = 0.0f;
        return;
    }

    for (int i = 0; i < order; i++) {
        float lambda = autocorr[i + 1];
        for (int j = 0; j < i; j++) {
            lambda -= a_prev[j] * autocorr[i - j];
        }
        float k = lambda / E;
        reflection[i] = k;

        a_curr[i] = k;
        for (int j = 0; j < i; j++) {
            a_curr[j] = a_prev[j] - k * a_prev[i - 1 - j];
        }

        E = E * (1.0f - k * k);
        if (E <= 0.0f) E = 1e-10f;

        for (int j = 0; j <= i; j++) {
            a_prev[j] = a_curr[j];
        }
    }

    for (int i = 0; i < order; i++) {
        lpc[i] = a_curr[i];
    }
    error_batch[frame] = E;
}

//=============================================================================
// LPC Filtering Kernels
//=============================================================================

/**
 * @brief LPC synthesis filter (IIR)
 *
 * y[n] = x[n] + sum_{k=1}^{order} a[k] * y[n-k]
 */
__global__ void kernel_lpc_filter(
    const float* __restrict__ input,
    float* __restrict__ output,
    const float* __restrict__ lpc_coeffs,
    int order,
    int num_samples)
{
    // This is inherently sequential due to IIR nature
    // Process one sample at a time
    for (int n = 0; n < num_samples; n++) {
        float sum = input[n];
        for (int k = 0; k < order && (n - k - 1) >= 0; k++) {
            sum += lpc_coeffs[k] * output[n - k - 1];
        }
        output[n] = sum;
    }
}

/**
 * @brief LPC inverse filter (prediction error filter / residual extraction)
 *
 * e[n] = x[n] - sum_{k=1}^{order} a[k] * x[n-k]
 */
__global__ void kernel_lpc_inverse_filter(
    const float* __restrict__ input,
    float* __restrict__ residual,
    const float* __restrict__ lpc_coeffs,
    int order,
    int num_samples)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    if (n >= num_samples) return;

    float sum = input[n];
    for (int k = 0; k < order && (n - k - 1) >= 0; k++) {
        sum -= lpc_coeffs[k] * input[n - k - 1];
    }
    residual[n] = sum;
}

//=============================================================================
// LPC Root Finding for Formants
//=============================================================================

/**
 * @brief Build companion matrix from LPC coefficients for root finding
 *
 * For polynomial a[0] + a[1]*z^-1 + ... + a[n]*z^-n
 * Companion matrix has eigenvalues equal to polynomial roots
 *
 * @param lpc_coeffs LPC coefficients
 * @param companion Output companion matrix [order x order]
 * @param order LPC order
 */
__global__ void kernel_build_companion_matrix_single(
    const float* __restrict__ lpc_coeffs,
    float* __restrict__ companion,
    int order)
{
    int idx = threadIdx.x;
    if (idx >= order) return;

    // Initialize row
    for (int j = 0; j < order; j++) {
        float val = 0.0f;

        // First row: -a[1], -a[2], ..., -a[n] (negative of LPC coeffs)
        if (idx == 0) {
            val = -lpc_coeffs[j];
        }
        // Subdiagonal: 1's
        else if (j == idx - 1) {
            val = 1.0f;
        }

        companion[idx * order + j] = val;
    }
}

/**
 * @brief Durand-Kerner method for polynomial root finding
 *
 * Iterative method to find all roots of a polynomial simultaneously.
 * Works well for LPC polynomials which typically have well-separated roots.
 *
 * @param lpc_coeffs LPC polynomial coefficients (a[0]=1 implicit)
 * @param roots_real Output real parts of roots
 * @param roots_imag Output imaginary parts of roots
 * @param order Polynomial order
 * @param max_iters Maximum iterations
 */
__global__ void kernel_durand_kerner_roots(
    const float* __restrict__ lpc_coeffs,
    float* __restrict__ roots_real,
    float* __restrict__ roots_imag,
    int order,
    int max_iters)
{
    // Run on single thread (order is small, typically 10-16)
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    // Initialize roots on unit circle (slightly inside to ensure convergence)
    float radius = 0.95f;
    for (int i = 0; i < order; i++) {
        float angle = NIMCP_TWO_PI_F * (float)i / (float)order + 0.1f;
        roots_real[i] = radius * cosf(angle);
        roots_imag[i] = radius * sinf(angle);
    }

    // Durand-Kerner iteration
    for (int iter = 0; iter < max_iters; iter++) {
        float max_delta = 0.0f;

        for (int i = 0; i < order; i++) {
            // Evaluate polynomial at z_i
            // P(z) = z^n + a[0]*z^(n-1) + a[1]*z^(n-2) + ... + a[n-1]
            float z_re = roots_real[i];
            float z_im = roots_imag[i];

            // Compute z^n
            float p_re = 1.0f;
            float p_im = 0.0f;
            float zpower_re = 1.0f;
            float zpower_im = 0.0f;

            // P(z) = z^n + sum_{k=0}^{n-1} a[k] * z^(n-1-k)
            // Start with z^n term
            for (int j = 0; j < order; j++) {
                float temp = zpower_re * z_re - zpower_im * z_im;
                zpower_im = zpower_re * z_im + zpower_im * z_re;
                zpower_re = temp;
            }
            p_re = zpower_re;
            p_im = zpower_im;

            // Add polynomial terms
            zpower_re = 1.0f;
            zpower_im = 0.0f;
            for (int k = order - 1; k >= 0; k--) {
                // Multiply by z
                float temp = zpower_re * z_re - zpower_im * z_im;
                zpower_im = zpower_re * z_im + zpower_im * z_re;
                zpower_re = temp;

                p_re += lpc_coeffs[order - 1 - k] * zpower_re;
                p_im += lpc_coeffs[order - 1 - k] * zpower_im;
            }

            // Compute product of (z_i - z_j) for j != i
            float prod_re = 1.0f;
            float prod_im = 0.0f;
            for (int j = 0; j < order; j++) {
                if (j != i) {
                    float diff_re = z_re - roots_real[j];
                    float diff_im = z_im - roots_imag[j];
                    float temp = prod_re * diff_re - prod_im * diff_im;
                    prod_im = prod_re * diff_im + prod_im * diff_re;
                    prod_re = temp;
                }
            }

            // Compute delta = P(z_i) / product
            float denom = prod_re * prod_re + prod_im * prod_im;
            if (denom < 1e-20f) denom = 1e-20f;

            float delta_re = (p_re * prod_re + p_im * prod_im) / denom;
            float delta_im = (p_im * prod_re - p_re * prod_im) / denom;

            // Update root
            roots_real[i] -= delta_re;
            roots_imag[i] -= delta_im;

            float delta_mag = sqrtf(delta_re * delta_re + delta_im * delta_im);
            if (delta_mag > max_delta) max_delta = delta_mag;
        }

        // Check convergence
        if (max_delta < 1e-8f) break;
    }
}

/**
 * @brief Convert complex roots to formant frequencies and bandwidths
 *
 * Formants are roots inside the unit circle with positive imaginary part.
 * Frequency = angle * sample_rate / (2*pi)
 * Bandwidth = -log(magnitude) * sample_rate / pi
 *
 * @param root_real Real parts of roots
 * @param root_imag Imaginary parts of roots
 * @param frequencies Output formant frequencies [n_formants]
 * @param bandwidths Output formant bandwidths [n_formants] (can be NULL)
 * @param order Number of roots
 * @param sample_rate Audio sample rate (Hz)
 * @param n_formants Maximum formants to extract
 */
__global__ void kernel_roots_to_formants_single(
    const float* __restrict__ root_real,
    const float* __restrict__ root_imag,
    float* __restrict__ frequencies,
    float* __restrict__ bandwidths,
    int order,
    float sample_rate,
    int n_formants)
{
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    // Local storage for sorting
    float formant_freqs[16];
    float formant_bws[16];
    int n_found = 0;

    const float PI = NIMCP_PI_F;

    // Extract formants from roots with positive imaginary part inside unit circle
    for (int i = 0; i < order && n_found < 16; i++) {
        float re = root_real[i];
        float im = root_imag[i];

        // Only consider roots with positive imaginary part (upper half-plane)
        if (im <= 0.0f) continue;

        // Compute magnitude and angle
        float mag = sqrtf(re * re + im * im);

        // Only consider roots inside or near unit circle
        if (mag > 1.05f) continue;

        float angle = atan2f(im, re);
        float freq = angle * sample_rate / (2.0f * PI);

        // Frequency must be positive and less than Nyquist
        if (freq < 50.0f || freq > sample_rate / 2.0f) continue;

        // Compute bandwidth from distance to unit circle
        float bw = -logf(mag) * sample_rate / PI;
        if (bw < 0.0f) bw = 0.0f;
        if (bw > 1000.0f) continue;  // Skip unreasonably wide formants

        formant_freqs[n_found] = freq;
        formant_bws[n_found] = bw;
        n_found++;
    }

    // Sort formants by frequency (simple bubble sort - order is small)
    for (int i = 0; i < n_found - 1; i++) {
        for (int j = 0; j < n_found - 1 - i; j++) {
            if (formant_freqs[j] > formant_freqs[j + 1]) {
                float temp = formant_freqs[j];
                formant_freqs[j] = formant_freqs[j + 1];
                formant_freqs[j + 1] = temp;
                temp = formant_bws[j];
                formant_bws[j] = formant_bws[j + 1];
                formant_bws[j + 1] = temp;
            }
        }
    }

    // Copy to output (first n_formants)
    for (int i = 0; i < n_formants; i++) {
        if (i < n_found) {
            frequencies[i] = formant_freqs[i];
            if (bandwidths) bandwidths[i] = formant_bws[i];
        } else {
            frequencies[i] = 0.0f;
            if (bandwidths) bandwidths[i] = 0.0f;
        }
    }
}

//=============================================================================
// Autocorrelation via FFT (more efficient for large frames)
//=============================================================================

/**
 * @brief Compute autocorrelation directly (for small frames)
 *
 * R(k) = sum_{n=0}^{N-1-k} x[n] * x[n+k]
 */
__global__ void kernel_autocorrelation_direct(
    const float* __restrict__ frame,
    float* __restrict__ autocorr,
    int frame_size,
    int max_lag)
{
    int lag = blockIdx.x * blockDim.x + threadIdx.x;
    if (lag > max_lag) return;

    float sum = 0.0f;
    for (int n = 0; n < frame_size - lag; n++) {
        sum += frame[n] * frame[n + lag];
    }
    autocorr[lag] = sum;
}

//=============================================================================
// LPC Context Management
//=============================================================================

typedef struct nimcp_lpc_ctx {
    int order;              // LPC order (typically 10-16 for speech)
    int frame_size;
    void* gpu_context;

    // Workspace (device pointers)
    float* d_autocorr;      // Autocorrelation coefficients
    float* d_lpc_coeffs;    // LPC coefficients
    float* d_reflection;    // Reflection coefficients
    float* d_prediction_error;
} nimcp_lpc_ctx_t;

nimcp_lpc_ctx_t* nimcp_lpc_create(void* gpu_ctx, int order, int frame_size)
{
    if (!gpu_ctx || order <= 0 || frame_size <= 0) return NULL;

    nimcp_lpc_ctx_t* ctx = (nimcp_lpc_ctx_t*)nimcp_malloc(sizeof(nimcp_lpc_ctx_t));
    if (!ctx) return NULL;

    ctx->order = order;
    ctx->frame_size = frame_size;
    ctx->gpu_context = gpu_ctx;

    // Allocate GPU memory
    if (cudaMalloc(&ctx->d_autocorr, (order + 1) * sizeof(float)) != cudaSuccess) {
        nimcp_free(ctx);
        return NULL;
    }
    if (cudaMalloc(&ctx->d_lpc_coeffs, order * sizeof(float)) != cudaSuccess) {
        cudaFree(ctx->d_autocorr);
        nimcp_free(ctx);
        return NULL;
    }
    if (cudaMalloc(&ctx->d_reflection, order * sizeof(float)) != cudaSuccess) {
        cudaFree(ctx->d_autocorr);
        cudaFree(ctx->d_lpc_coeffs);
        nimcp_free(ctx);
        return NULL;
    }
    if (cudaMalloc(&ctx->d_prediction_error, sizeof(float)) != cudaSuccess) {
        cudaFree(ctx->d_autocorr);
        cudaFree(ctx->d_lpc_coeffs);
        cudaFree(ctx->d_reflection);
        nimcp_free(ctx);
        return NULL;
    }

    return ctx;
}

void nimcp_lpc_destroy(nimcp_lpc_ctx_t* ctx)
{
    if (!ctx) return;

    cudaFree(ctx->d_autocorr);
    cudaFree(ctx->d_lpc_coeffs);
    cudaFree(ctx->d_reflection);
    cudaFree(ctx->d_prediction_error);
    nimcp_free(ctx);
}

int nimcp_lpc_autocorrelation(nimcp_lpc_ctx_t* ctx, const float* d_frame, float* d_autocorr)
{
    if (!ctx || !d_frame || !d_autocorr) return -1;

    int num_blocks = (ctx->order + 1 + BLOCK_SIZE - 1) / BLOCK_SIZE;
    kernel_autocorrelation_direct<<<num_blocks, BLOCK_SIZE>>>(
        d_frame, d_autocorr, ctx->frame_size, ctx->order);

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

int nimcp_lpc_levinson_durbin(nimcp_lpc_ctx_t* ctx, const float* d_autocorr,
                               float* d_lpc_coeffs, float* d_reflection_coeffs, float* d_error)
{
    if (!ctx || !d_autocorr || !d_lpc_coeffs) return -1;

    kernel_levinson_durbin_single<<<1, 1>>>(
        d_autocorr,
        d_lpc_coeffs,
        d_reflection_coeffs ? d_reflection_coeffs : ctx->d_reflection,
        d_error ? d_error : ctx->d_prediction_error,
        ctx->order);

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

int nimcp_lpc_analyze_frame(nimcp_lpc_ctx_t* ctx, const float* d_frame,
                            float* d_lpc_coeffs, float* d_residual)
{
    if (!ctx || !d_frame || !d_lpc_coeffs) return -1;

    // Compute autocorrelation
    int ret = nimcp_lpc_autocorrelation(ctx, d_frame, ctx->d_autocorr);
    if (ret != 0) return ret;

    // Levinson-Durbin
    ret = nimcp_lpc_levinson_durbin(ctx, ctx->d_autocorr, d_lpc_coeffs, NULL, NULL);
    if (ret != 0) return ret;

    // Optionally compute residual
    if (d_residual) {
        int num_blocks = (ctx->frame_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        kernel_lpc_inverse_filter<<<num_blocks, BLOCK_SIZE>>>(
            d_frame, d_residual, d_lpc_coeffs, ctx->order, ctx->frame_size);
    }

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

//=============================================================================
// Formant Context Management
//=============================================================================

typedef struct nimcp_formant_result {
    float* frequencies;     // Formant frequencies (F1, F2, F3, ...)
    float* bandwidths;      // Formant bandwidths
    int num_formants;
    int num_frames;
} nimcp_formant_result_t;

int nimcp_lpc_find_formants(nimcp_lpc_ctx_t* ctx, const float* d_lpc_coeffs,
                            float sample_rate, nimcp_formant_result_t* result)
{
    if (!ctx || !d_lpc_coeffs || !result) return -1;

    int order = ctx->order;
    int n_formants = result->num_formants;

    // Allocate root finding buffers
    float *d_roots_real, *d_roots_imag;
    if (cudaMalloc(&d_roots_real, order * sizeof(float)) != cudaSuccess) return -1;
    if (cudaMalloc(&d_roots_imag, order * sizeof(float)) != cudaSuccess) {
        cudaFree(d_roots_real);
        return -1;
    }

    // Find roots using Durand-Kerner
    kernel_durand_kerner_roots<<<1, 1>>>(d_lpc_coeffs, d_roots_real, d_roots_imag, order, 100);

    // Allocate device output
    float *d_freqs, *d_bws;
    if (cudaMalloc(&d_freqs, n_formants * sizeof(float)) != cudaSuccess) {
        cudaFree(d_roots_real);
        cudaFree(d_roots_imag);
        return -1;
    }
    if (cudaMalloc(&d_bws, n_formants * sizeof(float)) != cudaSuccess) {
        cudaFree(d_roots_real);
        cudaFree(d_roots_imag);
        cudaFree(d_freqs);
        return -1;
    }

    // Convert to formants
    kernel_roots_to_formants_single<<<1, 1>>>(
        d_roots_real, d_roots_imag, d_freqs, d_bws,
        order, sample_rate, n_formants);

    // Copy to host
    cudaMemcpy(result->frequencies, d_freqs, n_formants * sizeof(float), cudaMemcpyDeviceToHost);
    if (result->bandwidths) {
        cudaMemcpy(result->bandwidths, d_bws, n_formants * sizeof(float), cudaMemcpyDeviceToHost);
    }

    // Cleanup
    cudaFree(d_roots_real);
    cudaFree(d_roots_imag);
    cudaFree(d_freqs);
    cudaFree(d_bws);

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

//=============================================================================
// Formant Extraction (LPC-based)
//=============================================================================

__global__ void kernel_preemphasis(
    const float* input, float* output, int n, float coeff)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    if (idx == 0) {
        output[idx] = input[idx];
    } else {
        output[idx] = input[idx] - coeff * input[idx - 1];
    }
}

__global__ void kernel_lpc_autocorr(
    const float* signal, float* autocorr, int signal_len, int order)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k > order) return;

    float sum = 0.0f;
    for (int i = 0; i < signal_len - k; i++) {
        sum += signal[i] * signal[i + k];
    }
    autocorr[k] = sum;
}

bool nimcp_gpu_formant_extract(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t* formants,
    int n_formants, int lpc_order, float sample_rate)
{
    if (!ctx || !audio || !formants) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int signal_len = audio->dims[audio->ndim - 1];

    // Apply pre-emphasis
    float* d_preemph;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_preemph, signal_len * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_preemphasis<<<(signal_len + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        (const float*)audio->data, d_preemph, signal_len, 0.97f);

    // Compute LPC autocorrelation
    float* d_autocorr;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_autocorr, (lpc_order + 1) * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_lpc_autocorr<<<1, lpc_order + 1>>>(d_preemph, d_autocorr, signal_len, lpc_order);

    // Levinson-Durbin is implemented below - we now use the full LPC formant pipeline
    float* d_lpc;
    float* d_reflection;
    float* d_error;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_lpc, lpc_order * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_reflection, lpc_order * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_error, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    // Run Levinson-Durbin (single frame for now)
    kernel_levinson_durbin_single<<<1, 1>>>(
        d_autocorr, d_lpc, d_reflection, d_error, lpc_order);

    // Extract formants from LPC coefficients using root finding
    float* d_roots_real;
    float* d_roots_imag;
    float* d_freqs;
    float* d_bandwidths;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_roots_real, lpc_order * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_roots_imag, lpc_order * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_freqs, n_formants * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_bandwidths, n_formants * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    // Build companion matrix and find roots via Durand-Kerner method
    float* d_companion;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_companion, lpc_order * lpc_order * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    kernel_build_companion_matrix_single<<<1, lpc_order>>>(d_lpc, d_companion, lpc_order);
    kernel_durand_kerner_roots<<<1, lpc_order>>>(d_lpc, d_roots_real, d_roots_imag, lpc_order, 100);

    // Convert roots to formant frequencies and bandwidths
    kernel_roots_to_formants_single<<<1, lpc_order>>>(
        d_roots_real, d_roots_imag, (float*)formants->data, NULL,
        lpc_order, sample_rate, n_formants);

    // Cleanup
    cudaFree(d_preemph);
    cudaFree(d_autocorr);
    cudaFree(d_lpc);
    cudaFree(d_reflection);
    cudaFree(d_error);
    cudaFree(d_roots_real);
    cudaFree(d_roots_imag);
    cudaFree(d_freqs);
    cudaFree(d_bandwidths);
    cudaFree(d_companion);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Phoneme Feature Extraction
//=============================================================================

__global__ void kernel_delta_features(
    const float* features, float* delta, int n_frames, int n_features, int context)
{
    int frame = blockIdx.y;
    int feat = blockIdx.x * blockDim.x + threadIdx.x;

    if (feat >= n_features || frame >= n_frames) return;

    float sum_num = 0.0f;
    float sum_denom = 0.0f;

    for (int t = 1; t <= context; t++) {
        int prev = (frame - t >= 0) ? frame - t : 0;
        int next = (frame + t < n_frames) ? frame + t : n_frames - 1;

        sum_num += t * (features[next * n_features + feat] - features[prev * n_features + feat]);
        sum_denom += t * t;
    }

    delta[frame * n_features + feat] = sum_num / (2.0f * sum_denom);
}

bool nimcp_gpu_delta_features(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* features,
    nimcp_gpu_tensor_t* delta,
    int context)
{
    if (!ctx || !features || !delta) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int n_frames = features->dims[0];
    int n_features = features->dims[1];

    dim3 block(BLOCK_SIZE);
    dim3 grid((n_features + BLOCK_SIZE - 1) / BLOCK_SIZE, n_frames);

    kernel_delta_features<<<grid, block>>>(
        (const float*)features->data, (float*)delta->data,
        n_frames, n_features, context);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "utils/logging/nimcp_logging.h"
#define LOG_MODULE "SPEECH_GPU"

bool nimcp_gpu_pitch_detect(void* ctx, void* audio, void* pitch, void* conf, float sr, float min, float max)
{
    LOG_WARN("CUDA not available - speech processing requires GPU");
    return false;
}

bool nimcp_gpu_vad(void* ctx, void* audio, void* vad, int fl, int hl, float th) { return false; }
bool nimcp_gpu_formant_extract(void* ctx, void* audio, void* form, int n, int o, float sr) { return false; }
bool nimcp_gpu_delta_features(void* ctx, void* feat, void* delta, int ctx_size) { return false; }

#endif // NIMCP_ENABLE_CUDA
