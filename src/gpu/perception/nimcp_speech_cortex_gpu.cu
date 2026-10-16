/**
 * @file nimcp_speech_cortex_gpu.cu
 * @brief GPU CUDA Kernels for Speech Cortex Processing
 *
 * WHAT: CUDA kernels for GPU-accelerated speech feature extraction
 * WHY:  GPU acceleration for computationally intensive speech analysis
 * HOW:  Parallel kernels for FFT, MFCC, LPC, formants, pitch detection
 *
 * BIOLOGICAL BASIS:
 * =================
 * Speech processing in the auditory cortex involves massive parallel computation:
 * - Cochlear frequency decomposition (approximated by mel filterbanks)
 * - Temporal integration (frame-based analysis)
 * - Categorical perception (phoneme classification)
 *
 * GPU ACCELERATION STRATEGY:
 * ==========================
 * - FFT: cuFFT library for optimal performance
 * - Mel filterbank: Parallel matrix-vector multiply
 * - Autocorrelation: Parallel across lag values
 * - LPC: Batch Levinson-Durbin across frames
 * - Formants: Parallel polynomial root finding
 * - Phoneme: Parallel softmax classification
 *
 * REFERENCES:
 * - Huang et al. (2001) Spoken Language Processing
 * - Rabiner & Schafer (2007) Theory and Applications of Digital Speech Processing
 * - Davis & Mermelstein (1980) MFCC computation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cufft.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#include "gpu/perception/nimcp_speech_cortex_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "SPEECH_CORTEX_GPU"

#define CUFFT_CHECK(call) do { \
    cufftResult result = call; \
    if (result != CUFFT_SUCCESS) { \
        LOG_ERROR("cuFFT error at %s:%d: %d", __FILE__, __LINE__, result); \
        return false; \
    } \
} while(0)

#define CUFFT_CHECK_NULL(call) do { \
    cufftResult result = call; \
    if (result != CUFFT_SUCCESS) { \
        LOG_ERROR("cuFFT error at %s:%d: %d", __FILE__, __LINE__, result); \
        return NULL; \
    } \
} while(0)

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Mathematical Constants
//=============================================================================

#define PI NIMCP_PI_F
#define LOG_10 2.302585092994046f
#define MEL_BREAK_FREQ_HZ 700.0f
#define MEL_HIGH_FREQ_Q 1127.0f

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief Convert Hz to Mel scale
 */
__device__ __forceinline__ float hz_to_mel(float hz)
{
    return MEL_HIGH_FREQ_Q * logf(1.0f + hz / MEL_BREAK_FREQ_HZ);
}

/**
 * @brief Convert Mel scale to Hz
 */
__device__ __forceinline__ float mel_to_hz(float mel)
{
    return MEL_BREAK_FREQ_HZ * (expf(mel / MEL_HIGH_FREQ_Q) - 1.0f);
}

/**
 * @brief Safe log with floor
 */
__device__ __forceinline__ float safe_log(float x, float floor_val)
{
    return logf(fmaxf(x, floor_val));
}

//=============================================================================
// CUDA Kernels: Preprocessing
//=============================================================================

/**
 * @brief Pre-emphasis filter kernel (speech cortex version)
 *
 * y[n] = x[n] - coeff * x[n-1]
 */
__global__ void kernel_speech_cortex_preemphasis(
    const float* __restrict__ input,
    float* __restrict__ output,
    float coeff,
    int n
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    if (idx == 0) {
        output[idx] = input[idx];
    } else {
        output[idx] = input[idx] - coeff * input[idx - 1];
    }
}

/**
 * @brief Generate window function kernel
 */
__global__ void kernel_generate_window(
    float* __restrict__ window,
    int window_size,
    int window_type  // 0=Hamming, 1=Hann, 2=Blackman, 3=Rectangular
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= window_size) return;

    float n = (float)idx;
    float N = (float)(window_size - 1);

    switch (window_type) {
        case 0: // Hamming
            window[idx] = 0.54f - 0.46f * cosf(NIMCP_TWO_PI_F * n / N);
            break;
        case 1: // Hann
            window[idx] = 0.5f * (1.0f - cosf(NIMCP_TWO_PI_F * n / N));
            break;
        case 2: // Blackman
            window[idx] = 0.42f - 0.5f * cosf(NIMCP_TWO_PI_F * n / N) + 0.08f * cosf(4.0f * PI * n / N);
            break;
        case 3: // Rectangular
        default:
            window[idx] = 1.0f;
            break;
    }
}

/**
 * @brief Apply window function to frames (speech cortex specific)
 *
 * Note: Renamed from kernel_apply_window to avoid duplicate symbol with audio_kernels
 */
__global__ void kernel_speech_cortex_apply_window(
    const float* __restrict__ input,
    float* __restrict__ output,
    const float* __restrict__ window,
    int num_frames,
    int frame_size,
    int hop_size,
    int input_length
) {
    int frame = blockIdx.y;
    int sample = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || sample >= frame_size) return;

    int input_idx = frame * hop_size + sample;
    int output_idx = frame * frame_size + sample;

    if (input_idx < input_length) {
        output[output_idx] = input[input_idx] * window[sample];
    } else {
        output[output_idx] = 0.0f;  // Zero-pad
    }
}

/**
 * @brief Frame audio into overlapping windows (speech cortex version)
 */
__global__ void kernel_speech_cortex_frame_audio(
    const float* __restrict__ audio,
    float* __restrict__ frames,
    const float* __restrict__ window,
    int audio_len,
    int frame_size,
    int hop_size,
    int num_frames
) {
    int frame = blockIdx.y;
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || idx >= frame_size) return;

    int audio_idx = frame * hop_size + idx;
    int out_idx = frame * frame_size + idx;

    float val = (audio_idx < audio_len) ? audio[audio_idx] : 0.0f;
    frames[out_idx] = val * window[idx];
}

//=============================================================================
// CUDA Kernels: FFT and Spectral Processing
//=============================================================================

/**
 * @brief Compute power spectrum from complex FFT output
 */
__global__ void kernel_power_spectrum(
    const cufftComplex* __restrict__ fft_out,
    float* __restrict__ power,
    int num_frames,
    int fft_bins
) {
    int frame = blockIdx.y;
    int bin = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || bin >= fft_bins) return;

    int idx = frame * fft_bins + bin;
    float re = fft_out[idx].x;
    float im = fft_out[idx].y;
    power[idx] = re * re + im * im;
}

/**
 * @brief Compute magnitude spectrum (square root of power)
 */
__global__ void kernel_magnitude_spectrum(
    const cufftComplex* __restrict__ fft_out,
    float* __restrict__ magnitude,
    int num_frames,
    int fft_bins
) {
    int frame = blockIdx.y;
    int bin = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || bin >= fft_bins) return;

    int idx = frame * fft_bins + bin;
    float re = fft_out[idx].x;
    float im = fft_out[idx].y;
    magnitude[idx] = sqrtf(re * re + im * im);
}

//=============================================================================
// CUDA Kernels: Mel Filterbank
//=============================================================================

/**
 * @brief Create mel filterbank matrix
 *
 * Each filter is a triangular response in mel scale
 */
__global__ void kernel_create_mel_filterbank(
    float* __restrict__ filterbank,
    float* __restrict__ center_freqs,
    int num_mel_bins,
    int fft_bins,
    float fmin,
    float fmax,
    float sample_rate
) {
    int mel_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (mel_idx >= num_mel_bins) return;

    float mel_min = hz_to_mel(fmin);
    float mel_max = hz_to_mel(fmax);
    float mel_step = (mel_max - mel_min) / (float)(num_mel_bins + 1);

    // Mel band edges
    float mel_lo = mel_min + mel_idx * mel_step;
    float mel_center = mel_lo + mel_step;
    float mel_hi = mel_center + mel_step;

    // Convert to Hz
    float hz_lo = mel_to_hz(mel_lo);
    float hz_center = mel_to_hz(mel_center);
    float hz_hi = mel_to_hz(mel_hi);

    center_freqs[mel_idx] = hz_center;

    float bin_hz = sample_rate / (float)((fft_bins - 1) * 2);

    // Create triangular filter
    for (int bin = 0; bin < fft_bins; bin++) {
        float freq = bin * bin_hz;
        float weight = 0.0f;

        if (freq >= hz_lo && freq < hz_center) {
            weight = (freq - hz_lo) / (hz_center - hz_lo);
        } else if (freq >= hz_center && freq <= hz_hi) {
            weight = (hz_hi - freq) / (hz_hi - hz_center);
        }

        filterbank[mel_idx * fft_bins + bin] = weight;
    }
}

/**
 * @brief Apply mel filterbank to power spectrum
 *
 * mel_energies = filterbank @ power_spectrum
 */
__global__ void kernel_apply_mel_filterbank(
    const float* __restrict__ power_spectrum,
    const float* __restrict__ filterbank,
    float* __restrict__ mel_energies,
    int num_frames,
    int fft_bins,
    int num_mel_bins
) {
    int frame = blockIdx.y;
    int mel = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || mel >= num_mel_bins) return;

    const float* power = power_spectrum + frame * fft_bins;
    const float* filter = filterbank + mel * fft_bins;

    float sum = 0.0f;
    for (int bin = 0; bin < fft_bins; bin++) {
        sum += power[bin] * filter[bin];
    }

    mel_energies[frame * num_mel_bins + mel] = sum;
}

/**
 * @brief Optimized mel filterbank with shared memory
 */
__global__ void kernel_apply_mel_filterbank_shared(
    const float* __restrict__ power_spectrum,
    const float* __restrict__ filterbank,
    float* __restrict__ mel_energies,
    int num_frames,
    int fft_bins,
    int num_mel_bins
) {
    extern __shared__ float s_power[];

    int frame = blockIdx.y;
    int mel = blockIdx.x;

    if (frame >= num_frames || mel >= num_mel_bins) return;

    // Load power spectrum into shared memory
    const float* power = power_spectrum + frame * fft_bins;
    for (int i = threadIdx.x; i < fft_bins; i += blockDim.x) {
        s_power[i] = power[i];
    }
    __syncthreads();

    // Compute dot product
    const float* filter = filterbank + mel * fft_bins;
    float sum = 0.0f;
    for (int bin = threadIdx.x; bin < fft_bins; bin += blockDim.x) {
        sum += s_power[bin] * filter[bin];
    }

    // Warp reduction
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        sum += __shfl_down_sync(0xffffffff, sum, offset);
    }

    // Block reduction
    __shared__ float s_sum[32];
    int warp_id = threadIdx.x / WARP_SIZE;
    int lane_id = threadIdx.x % WARP_SIZE;

    if (lane_id == 0) s_sum[warp_id] = sum;
    __syncthreads();

    if (threadIdx.x == 0) {
        float total = 0.0f;
        int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;
        for (int w = 0; w < num_warps; w++) {
            total += s_sum[w];
        }
        mel_energies[frame * num_mel_bins + mel] = total;
    }
}

/**
 * @brief Apply log to mel energies
 */
__global__ void kernel_log_mel(
    const float* __restrict__ mel_energies,
    float* __restrict__ log_mel,
    int n,
    float floor_val
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    log_mel[idx] = logf(fmaxf(mel_energies[idx], floor_val));
}

//=============================================================================
// CUDA Kernels: DCT for MFCC
//=============================================================================

/**
 * @brief Create DCT-II matrix for MFCC computation
 */
__global__ void kernel_create_dct_matrix(
    float* __restrict__ dct_matrix,
    int num_mfcc,
    int num_mel_bins
) {
    int i = blockIdx.y;  // MFCC index
    int j = blockIdx.x * blockDim.x + threadIdx.x;  // Mel bin index

    if (i >= num_mfcc || j >= num_mel_bins) return;

    float scale = sqrtf(2.0f / (float)num_mel_bins);
    float angle = PI * (float)i * ((float)j + 0.5f) / (float)num_mel_bins;
    dct_matrix[i * num_mel_bins + j] = scale * cosf(angle);
}

/**
 * @brief Apply DCT to get MFCC
 *
 * mfcc = dct_matrix @ log_mel
 */
__global__ void kernel_apply_dct(
    const float* __restrict__ log_mel,
    const float* __restrict__ dct_matrix,
    float* __restrict__ mfcc,
    int num_frames,
    int num_mel_bins,
    int num_mfcc
) {
    int frame = blockIdx.y;
    int coeff = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || coeff >= num_mfcc) return;

    const float* log_mel_frame = log_mel + frame * num_mel_bins;
    const float* dct_row = dct_matrix + coeff * num_mel_bins;

    float sum = 0.0f;
    for (int m = 0; m < num_mel_bins; m++) {
        sum += dct_row[m] * log_mel_frame[m];
    }

    mfcc[frame * num_mfcc + coeff] = sum;
}

//=============================================================================
// CUDA Kernels: Delta Features
//=============================================================================

/**
 * @brief Compute delta (velocity) features (speech cortex version)
 *
 * delta[t] = sum_{n=1}^{N} n * (c[t+n] - c[t-n]) / (2 * sum_{n=1}^{N} n^2)
 */
__global__ void kernel_speech_cortex_delta_features(
    const float* __restrict__ features,
    float* __restrict__ delta,
    int num_frames,
    int feature_dim,
    int context
) {
    int frame = blockIdx.y;
    int feat = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || feat >= feature_dim) return;

    float sum_num = 0.0f;
    float sum_denom = 0.0f;

    for (int n = 1; n <= context; n++) {
        int prev = (frame - n >= 0) ? frame - n : 0;
        int next = (frame + n < num_frames) ? frame + n : num_frames - 1;

        float c_next = features[next * feature_dim + feat];
        float c_prev = features[prev * feature_dim + feat];

        sum_num += (float)n * (c_next - c_prev);
        sum_denom += (float)(n * n);
    }

    delta[frame * feature_dim + feat] = sum_num / (2.0f * sum_denom);
}

//=============================================================================
// CUDA Kernels: Autocorrelation and Pitch Detection
//=============================================================================

/**
 * @brief Compute autocorrelation for all lags (speech cortex version)
 */
__global__ void kernel_speech_cortex_autocorrelation(
    const float* __restrict__ signal,
    float* __restrict__ autocorr,
    int signal_len,
    int max_lag
) {
    int lag = blockIdx.x * blockDim.x + threadIdx.x;
    if (lag >= max_lag) return;

    float sum = 0.0f;
    int count = signal_len - lag;
    for (int i = 0; i < count; i++) {
        sum += signal[i] * signal[i + lag];
    }
    autocorr[lag] = sum / (float)count;  // Normalize
}

/**
 * @brief Batch autocorrelation for multiple frames (speech cortex version)
 */
__global__ void kernel_speech_cortex_autocorrelation_batch(
    const float* __restrict__ frames,
    float* __restrict__ autocorr,
    int num_frames,
    int frame_size,
    int max_lag
) {
    int frame = blockIdx.y;
    int lag = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || lag >= max_lag) return;

    const float* signal = frames + frame * frame_size;

    float sum = 0.0f;
    int count = frame_size - lag;
    for (int i = 0; i < count; i++) {
        sum += signal[i] * signal[i + lag];
    }

    autocorr[frame * max_lag + lag] = sum / (float)count;
}

/**
 * @brief Find pitch peak in autocorrelation (speech cortex version)
 */
__global__ void kernel_speech_cortex_find_pitch_peak(
    const float* __restrict__ autocorr,
    float* __restrict__ pitch,
    float* __restrict__ confidence,
    int num_frames,
    int max_lag,
    int min_lag,
    int sample_rate
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* ac = autocorr + frame * max_lag;

    // Find peak in valid pitch range
    float max_val = 0.0f;
    int max_idx = min_lag;

    for (int lag = min_lag; lag < max_lag; lag++) {
        if (ac[lag] > max_val) {
            max_val = ac[lag];
            max_idx = lag;
        }
    }

    // Convert lag to Hz
    float f0 = (max_idx > 0) ? (float)sample_rate / (float)max_idx : 0.0f;
    pitch[frame] = f0;

    // Confidence = normalized autocorrelation peak
    if (confidence != NULL && ac[0] > 0.0f) {
        confidence[frame] = max_val / ac[0];
    }
}

/**
 * @brief Parabolic interpolation for sub-sample pitch accuracy
 */
__device__ float parabolic_interpolation(float y0, float y1, float y2, int idx)
{
    float denom = y0 - 2.0f * y1 + y2;
    if (fabsf(denom) < 1e-10f) return (float)idx;
    float delta = 0.5f * (y0 - y2) / denom;
    return (float)idx + delta;
}

/**
 * @brief Refined pitch detection with parabolic interpolation
 */
__global__ void kernel_find_pitch_refined(
    const float* __restrict__ autocorr,
    float* __restrict__ pitch,
    float* __restrict__ confidence,
    int num_frames,
    int max_lag,
    int min_lag,
    int sample_rate
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* ac = autocorr + frame * max_lag;

    // Find peak in valid pitch range
    float max_val = 0.0f;
    int max_idx = min_lag;

    for (int lag = min_lag; lag < max_lag - 1; lag++) {
        if (lag < 1) continue;
        if (ac[lag] > max_val && ac[lag] > ac[lag-1] && ac[lag] > ac[lag+1]) {
            max_val = ac[lag];
            max_idx = lag;
        }
    }

    // Parabolic interpolation for sub-sample accuracy
    float refined_lag = (float)max_idx;
    if (max_idx > min_lag && max_idx < max_lag - 1) {
        refined_lag = parabolic_interpolation(ac[max_idx-1], ac[max_idx], ac[max_idx+1], max_idx);
    }

    // Convert lag to Hz
    float f0 = (refined_lag > 0.0f) ? (float)sample_rate / refined_lag : 0.0f;
    pitch[frame] = f0;

    if (confidence != NULL && ac[0] > 0.0f) {
        confidence[frame] = max_val / ac[0];
    }
}

/**
 * @brief Harmonic product spectrum for pitch detection
 */
__global__ void kernel_harmonic_product_spectrum(
    const float* __restrict__ magnitude,
    float* __restrict__ hps,
    int num_frames,
    int fft_bins,
    int num_harmonics
) {
    int frame = blockIdx.y;
    int bin = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || bin >= fft_bins) return;

    const float* mag = magnitude + frame * fft_bins;

    float product = mag[bin];
    for (int h = 2; h <= num_harmonics; h++) {
        int harm_bin = bin * h;
        if (harm_bin < fft_bins) {
            product *= mag[harm_bin];
        }
    }

    hps[frame * fft_bins + bin] = product;
}

//=============================================================================
// CUDA Kernels: LPC and Formant Extraction
//=============================================================================

/**
 * @brief Compute autocorrelation for LPC
 */
__global__ void kernel_lpc_autocorr(
    const float* __restrict__ frames,
    float* __restrict__ autocorr,
    int num_frames,
    int frame_size,
    int order
) {
    int frame = blockIdx.y;
    int k = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || k > order) return;

    const float* signal = frames + frame * frame_size;

    float sum = 0.0f;
    for (int i = 0; i < frame_size - k; i++) {
        sum += signal[i] * signal[i + k];
    }

    autocorr[frame * (order + 1) + k] = sum;
}

/**
 * @brief Levinson-Durbin recursion for LPC coefficients (batch version)
 *
 * Note: This is inherently sequential per frame but parallel across frames
 */
__global__ void kernel_levinson_durbin_batch(
    const float* __restrict__ autocorr,
    float* __restrict__ lpc_coeffs,
    float* __restrict__ reflection_coeffs,
    int num_frames,
    int order
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* r = autocorr + frame * (order + 1);
    float* a = lpc_coeffs + frame * order;
    float* k = reflection_coeffs + frame * order;

    // Temporary arrays (in registers/local memory)
    float a_prev[32];  // Max order supported
    if (order > 32) return;  // Guard against stack overflow
    float error = r[0];

    if (fabsf(error) < 1e-10f) {
        // Zero signal - set coefficients to zero
        for (int i = 0; i < order; i++) {
            a[i] = 0.0f;
            k[i] = 0.0f;
        }
        return;
    }

    for (int p = 0; p < order; p++) {
        // Compute reflection coefficient
        float lambda = 0.0f;
        for (int j = 0; j < p; j++) {
            lambda += a[j] * r[p - j];
        }
        lambda += r[p + 1];

        k[p] = -lambda / error;

        // Update coefficients
        for (int j = 0; j < p; j++) {
            a_prev[j] = a[j];
        }

        for (int j = 0; j < p; j++) {
            a[j] = a_prev[j] + k[p] * a_prev[p - 1 - j];
        }
        a[p] = k[p];

        // Update error
        error *= (1.0f - k[p] * k[p]);

        if (fabsf(error) < 1e-10f) break;
    }
}

/**
 * @brief Extract formants from LPC coefficients using root finding
 *
 * This is a simplified version using spectral peak picking
 * Full implementation would use Durand-Kerner or similar for polynomial roots
 */
__global__ void kernel_lpc_to_formants(
    const float* __restrict__ lpc_coeffs,
    float* __restrict__ formant_freqs,
    float* __restrict__ formant_bw,
    int num_frames,
    int order,
    int sample_rate,
    int num_formants,
    int spectrum_size
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* a = lpc_coeffs + frame * order;
    float* formants = formant_freqs + frame * num_formants;
    float* bandwidths = formant_bw + frame * num_formants;

    // Initialize formants to typical values
    for (int f = 0; f < num_formants; f++) {
        formants[f] = 500.0f * (f + 1);  // Default: 500, 1000, 1500, 2000 Hz
        bandwidths[f] = 100.0f;
    }

    // Compute LPC spectrum and find peaks
    float spectrum[256];  // Local array for spectrum
    int spec_size = min(spectrum_size, 256);

    for (int k = 0; k < spec_size; k++) {
        float freq = (float)k * (float)sample_rate / (2.0f * (float)spec_size);
        float omega = NIMCP_TWO_PI_F * freq / (float)sample_rate;

        // Compute 1 / |A(e^jw)|^2
        float re = 1.0f, im = 0.0f;
        for (int i = 0; i < order; i++) {
            re -= a[i] * cosf((i + 1) * omega);
            im += a[i] * sinf((i + 1) * omega);
        }

        float mag = re * re + im * im;
        spectrum[k] = (mag > 1e-10f) ? 1.0f / mag : 0.0f;
    }

    // Find peaks in spectrum (formants)
    int formant_idx = 0;
    for (int k = 1; k < spec_size - 1 && formant_idx < num_formants; k++) {
        if (spectrum[k] > spectrum[k-1] && spectrum[k] > spectrum[k+1]) {
            float freq = (float)k * (float)sample_rate / (2.0f * (float)spec_size);
            if (freq > 200.0f && freq < 5000.0f) {  // Valid formant range
                formants[formant_idx] = freq;
                formant_idx++;
            }
        }
    }
}

//=============================================================================
// CUDA Kernels: Voice Activity Detection
//=============================================================================

/**
 * @brief Compute frame energy in dB
 */
__global__ void kernel_frame_energy(
    const float* __restrict__ frames,
    float* __restrict__ energy,
    int num_frames,
    int frame_size
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* signal = frames + frame * frame_size;

    float sum = 0.0f;
    for (int i = 0; i < frame_size; i++) {
        sum += signal[i] * signal[i];
    }

    // Convert to dB
    float rms = sqrtf(sum / (float)frame_size);
    energy[frame] = 20.0f * log10f(fmaxf(rms, 1e-10f));
}

/**
 * @brief Compute zero crossing rate
 */
__global__ void kernel_zero_crossing_rate(
    const float* __restrict__ frames,
    float* __restrict__ zcr,
    int num_frames,
    int frame_size
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* signal = frames + frame * frame_size;

    int crossings = 0;
    for (int i = 1; i < frame_size; i++) {
        if ((signal[i] >= 0.0f && signal[i-1] < 0.0f) ||
            (signal[i] < 0.0f && signal[i-1] >= 0.0f)) {
            crossings++;
        }
    }

    zcr[frame] = (float)crossings / (float)(frame_size - 1);
}

/**
 * @brief VAD decision based on energy and ZCR
 */
__global__ void kernel_vad_decision(
    const float* __restrict__ energy,
    const float* __restrict__ zcr,
    float* __restrict__ vad,
    int num_frames,
    float energy_thresh,
    float zcr_thresh_low,
    float zcr_thresh_high
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    float e = energy[frame];
    float z = zcr[frame];

    // Speech typically has high energy and moderate ZCR
    // Unvoiced speech has high ZCR, voiced speech has low ZCR
    int is_speech = 0;
    if (e > energy_thresh) {
        if (z > zcr_thresh_low && z < zcr_thresh_high) {
            is_speech = 1;
        } else if (z >= zcr_thresh_high) {
            // Could be unvoiced speech (fricatives)
            is_speech = (e > energy_thresh + 10.0f) ? 1 : 0;
        }
    }

    vad[frame] = (float)is_speech;
}

//=============================================================================
// CUDA Kernels: Phoneme Classification
//=============================================================================

/**
 * @brief Linear layer forward pass for phoneme classification
 */
__global__ void kernel_phoneme_linear(
    const float* __restrict__ features,
    const float* __restrict__ weights,
    const float* __restrict__ bias,
    float* __restrict__ logits,
    int num_frames,
    int feature_dim,
    int num_classes
) {
    int frame = blockIdx.y;
    int cls = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || cls >= num_classes) return;

    const float* feat = features + frame * feature_dim;
    const float* w = weights + cls * feature_dim;

    float sum = bias[cls];
    for (int i = 0; i < feature_dim; i++) {
        sum += feat[i] * w[i];
    }

    logits[frame * num_classes + cls] = sum;
}

/**
 * @brief Softmax for phoneme probabilities (numerically stable)
 */
__global__ void kernel_softmax(
    const float* __restrict__ logits,
    float* __restrict__ probs,
    int num_frames,
    int num_classes
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* log_row = logits + frame * num_classes;
    float* prob_row = probs + frame * num_classes;

    // Find max for numerical stability
    float max_val = log_row[0];
    for (int i = 1; i < num_classes; i++) {
        if (log_row[i] > max_val) max_val = log_row[i];
    }

    // Compute exp(x - max) and sum
    float sum = 0.0f;
    for (int i = 0; i < num_classes; i++) {
        prob_row[i] = expf(log_row[i] - max_val);
        sum += prob_row[i];
    }

    // Normalize
    float inv_sum = 1.0f / sum;
    for (int i = 0; i < num_classes; i++) {
        prob_row[i] *= inv_sum;
    }
}

/**
 * @brief Argmax to get predicted phoneme IDs
 */
__global__ void kernel_argmax(
    const float* __restrict__ probs,
    int* __restrict__ ids,
    float* __restrict__ confidence,
    int num_frames,
    int num_classes
) {
    int frame = blockIdx.x * blockDim.x + threadIdx.x;
    if (frame >= num_frames) return;

    const float* prob_row = probs + frame * num_classes;

    float max_prob = prob_row[0];
    int max_idx = 0;

    for (int i = 1; i < num_classes; i++) {
        if (prob_row[i] > max_prob) {
            max_prob = prob_row[i];
            max_idx = i;
        }
    }

    ids[frame] = max_idx;
    if (confidence != NULL) {
        confidence[frame] = max_prob;
    }
}

//=============================================================================
// CUDA Kernels: Mean Normalization (CMN)
//=============================================================================

/**
 * @brief Compute mean across frames
 */
static __global__ void kernel_compute_mean(
    const float* __restrict__ features,
    float* __restrict__ mean,
    int num_frames,
    int feature_dim
) {
    int feat = blockIdx.x * blockDim.x + threadIdx.x;
    if (feat >= feature_dim) return;

    float sum = 0.0f;
    for (int f = 0; f < num_frames; f++) {
        sum += features[f * feature_dim + feat];
    }

    mean[feat] = sum / (float)num_frames;
}

/**
 * @brief Subtract mean from features (CMN)
 */
__global__ void kernel_subtract_mean(
    float* __restrict__ features,
    const float* __restrict__ mean,
    int num_frames,
    int feature_dim
) {
    int frame = blockIdx.y;
    int feat = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || feat >= feature_dim) return;

    features[frame * feature_dim + feat] -= mean[feat];
}

//=============================================================================
// API Implementation: Configuration
//=============================================================================

extern "C" nimcp_speech_gpu_config_t nimcp_speech_gpu_default_config(void) {
    nimcp_speech_gpu_config_t config;
    config.sample_rate = 16000;
    config.frame_size = 400;        // 25ms at 16kHz
    config.hop_size = 160;          // 10ms at 16kHz
    config.fft_size = 512;
    config.num_mel_bins = 80;
    config.num_mfcc = 13;
    config.lpc_order = 12;
    config.max_frames = 1000;
    config.window_type = SPEECH_GPU_WINDOW_HAMMING;
    config.preemphasis_coeff = 0.97f;
    config.min_pitch_hz = 50.0f;
    config.max_pitch_hz = 500.0f;
    config.include_energy = true;
    config.mean_normalize = false;
    return config;
}

//=============================================================================
// API Implementation: Lifecycle
//=============================================================================

extern "C" nimcp_speech_gpu_state_t* nimcp_speech_gpu_create(
    nimcp_gpu_context_t* ctx,
    int sample_rate,
    int frame_size,
    int hop_size,
    int num_mel_bins,
    int lpc_order
) {
    nimcp_speech_gpu_config_t config = nimcp_speech_gpu_default_config();
    config.sample_rate = sample_rate;
    config.frame_size = frame_size;
    config.hop_size = hop_size;
    config.num_mel_bins = num_mel_bins;
    config.lpc_order = lpc_order;
    return nimcp_speech_gpu_create_with_config(ctx, &config);
}

extern "C" nimcp_speech_gpu_state_t* nimcp_speech_gpu_create_with_config(
    nimcp_gpu_context_t* ctx,
    const nimcp_speech_gpu_config_t* config
) {
    if (!ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }
    if (!config) {
        LOG_ERROR("NULL configuration");
        return NULL;
    }

    nimcp_speech_gpu_state_t* state = (nimcp_speech_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_speech_gpu_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate speech GPU state");
        return NULL;
    }

    state->ctx = ctx;
    state->sample_rate = config->sample_rate;
    state->frame_size = config->frame_size;
    state->hop_size = config->hop_size;
    state->num_mel_bins = config->num_mel_bins;
    state->num_mfcc = config->num_mfcc;
    state->lpc_order = config->lpc_order;
    state->max_frames = config->max_frames;
    state->window_type = config->window_type;
    state->preemphasis_coeff = config->preemphasis_coeff;
    state->min_pitch_hz = config->min_pitch_hz;
    state->max_pitch_hz = config->max_pitch_hz;
    state->include_energy = config->include_energy;
    state->mean_normalize = config->mean_normalize;
    state->mel_floor = 1e-10f;
    state->num_phonemes = SPEECH_GPU_NUM_PHONEMES;

    // Compute FFT size (next power of 2)
    int fft_size = config->fft_size;
    if (fft_size == 0) {
        fft_size = 1;
        while (fft_size < config->frame_size) fft_size <<= 1;
    }
    state->fft_size = fft_size;
    state->fft_bins = fft_size / 2 + 1;

    // Pitch detection parameters
    state->min_pitch_lag = (int)(config->sample_rate / config->max_pitch_hz);
    state->max_pitch_lag = (int)(config->sample_rate / config->min_pitch_hz);

    // Create cuFFT plans
    cufftResult fft_result = cufftPlan1d(&state->fft_plan, fft_size, CUFFT_R2C, 1);
    if (fft_result != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to create cuFFT plan: %d", fft_result);
        nimcp_free(state);
        return NULL;
    }

    fft_result = cufftPlan1d(&state->ifft_plan, fft_size, CUFFT_C2R, 1);
    if (fft_result != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to create inverse cuFFT plan: %d", fft_result);
        cufftDestroy(state->fft_plan);
        nimcp_free(state);
        return NULL;
    }

    // Create batched FFT plan
    fft_result = cufftPlan1d(&state->fft_plan_batch, fft_size, CUFFT_R2C, config->max_frames);
    if (fft_result != CUFFT_SUCCESS) {
        LOG_ERROR("Failed to create batched cuFFT plan: %d", fft_result);
        cufftDestroy(state->fft_plan);
        cufftDestroy(state->ifft_plan);
        nimcp_free(state);
        return NULL;
    }

    state->fft_initialized = true;

    // Allocate window function
    size_t window_dims[1] = {(size_t)fft_size};
    state->window = nimcp_gpu_tensor_create(ctx, window_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!state->window) {
        LOG_ERROR("Failed to allocate window buffer");
        nimcp_speech_gpu_destroy(state);
        return NULL;
    }

    // Generate window function
    kernel_generate_window<<<GRID_SIZE(fft_size), BLOCK_SIZE>>>(
        (float*)state->window->data, fft_size, (int)config->window_type
    );

    // Allocate and create mel filterbank
    size_t mel_dims[2] = {(size_t)config->num_mel_bins, (size_t)state->fft_bins};
    state->mel_filterbank = nimcp_gpu_tensor_create(ctx, mel_dims, 2, NIMCP_GPU_PRECISION_FP32);

    size_t mel_center_dims[1] = {(size_t)config->num_mel_bins};
    state->mel_center_freqs = nimcp_gpu_tensor_create(ctx, mel_center_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->mel_filterbank || !state->mel_center_freqs) {
        LOG_ERROR("Failed to allocate mel filterbank");
        nimcp_speech_gpu_destroy(state);
        return NULL;
    }

    // Create mel filterbank
    float fmin = 0.0f;
    float fmax = (float)config->sample_rate / 2.0f;
    kernel_create_mel_filterbank<<<GRID_SIZE(config->num_mel_bins), BLOCK_SIZE>>>(
        (float*)state->mel_filterbank->data,
        (float*)state->mel_center_freqs->data,
        config->num_mel_bins, state->fft_bins,
        fmin, fmax, (float)config->sample_rate
    );

    // Create DCT matrix for MFCC
    size_t dct_dims[2] = {(size_t)config->num_mfcc, (size_t)config->num_mel_bins};
    state->dct_matrix = nimcp_gpu_tensor_create(ctx, dct_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!state->dct_matrix) {
        LOG_ERROR("Failed to allocate DCT matrix");
        nimcp_speech_gpu_destroy(state);
        return NULL;
    }

    dim3 dct_grid((config->num_mel_bins + BLOCK_SIZE - 1) / BLOCK_SIZE, config->num_mfcc);
    kernel_create_dct_matrix<<<dct_grid, BLOCK_SIZE>>>(
        (float*)state->dct_matrix->data, config->num_mfcc, config->num_mel_bins
    );

    // Allocate temporary buffers
    size_t max_frames = config->max_frames;

    size_t fft_buf_dims[2] = {max_frames, (size_t)state->fft_bins};
    size_t mel_buf_dims[2] = {max_frames, (size_t)config->num_mel_bins};
    size_t formant_dims[2] = {max_frames, SPEECH_GPU_NUM_FORMANTS};
    size_t pitch_dims[1] = {max_frames};

    // Allocate complex FFT buffer (2 floats per complex)
    // Note: Complex buffer = max_frames * fft_bins * 2 floats
    state->fft_buffer = nimcp_gpu_tensor_create(ctx, fft_buf_dims, 2, NIMCP_GPU_PRECISION_FP32);

    state->power_spectrum = nimcp_gpu_tensor_create(ctx, fft_buf_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->mel_energies = nimcp_gpu_tensor_create(ctx, mel_buf_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->log_mel = nimcp_gpu_tensor_create(ctx, mel_buf_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->formant_buffer = nimcp_gpu_tensor_create(ctx, formant_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->pitch_buffer = nimcp_gpu_tensor_create(ctx, pitch_dims, 1, NIMCP_GPU_PRECISION_FP32);

    // Allocate LPC and autocorrelation buffers
    size_t lpc_dims[2] = {max_frames, (size_t)config->lpc_order};
    size_t autocorr_dims[2] = {max_frames, (size_t)(state->max_pitch_lag + 1)};

    state->lpc_buffer = nimcp_gpu_tensor_create(ctx, lpc_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->autocorr_buffer = nimcp_gpu_tensor_create(ctx, autocorr_dims, 2, NIMCP_GPU_PRECISION_FP32);

    cudaError_t sync_err = cudaDeviceSynchronize();
    if (sync_err != cudaSuccess) {
        LOG_ERROR("CUDA synchronization error: %s", cudaGetErrorString(sync_err));
        nimcp_speech_gpu_destroy(state);
        return NULL;
    }

    LOG_INFO("Speech GPU state created (SR=%d, frame=%d, hop=%d, mel=%d, mfcc=%d, lpc=%d)",
             config->sample_rate, config->frame_size, config->hop_size,
             config->num_mel_bins, config->num_mfcc, config->lpc_order);

    return state;
}

extern "C" void nimcp_speech_gpu_destroy(nimcp_speech_gpu_state_t* state) {
    if (!state) return;

    // Destroy cuFFT plans
    if (state->fft_initialized) {
        cufftDestroy(state->fft_plan);
        cufftDestroy(state->ifft_plan);
        cufftDestroy(state->fft_plan_batch);
    }

    // Free GPU tensors
    nimcp_gpu_tensor_destroy(state->window);
    nimcp_gpu_tensor_destroy(state->mel_filterbank);
    nimcp_gpu_tensor_destroy(state->mel_center_freqs);
    nimcp_gpu_tensor_destroy(state->dct_matrix);
    nimcp_gpu_tensor_destroy(state->lpc_buffer);
    nimcp_gpu_tensor_destroy(state->autocorr_buffer);
    nimcp_gpu_tensor_destroy(state->fft_buffer);
    nimcp_gpu_tensor_destroy(state->power_spectrum);
    nimcp_gpu_tensor_destroy(state->mel_energies);
    nimcp_gpu_tensor_destroy(state->log_mel);
    nimcp_gpu_tensor_destroy(state->formant_buffer);
    nimcp_gpu_tensor_destroy(state->pitch_buffer);
    nimcp_gpu_tensor_destroy(state->phoneme_weights);
    nimcp_gpu_tensor_destroy(state->phoneme_bias);

    nimcp_free(state);
    LOG_DEBUG("Speech GPU state destroyed");
}

extern "C" bool nimcp_speech_gpu_synchronize(nimcp_speech_gpu_state_t* state) {
    if (!state || !state->ctx) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    NIMCP_CUDA_RECOVER(cudaDeviceSynchronize(), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Feature Extraction
//=============================================================================

extern "C" int nimcp_speech_gpu_get_num_frames(
    const nimcp_speech_gpu_state_t* state,
    int num_samples
) {
    if (!state || num_samples < state->frame_size) return 0;
    return (num_samples - state->frame_size) / state->hop_size + 1;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_spectrogram(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0 || num_frames > state->max_frames) {
        LOG_ERROR("Invalid number of frames: %d", num_frames);
        return NULL;
    }

    // Allocate output
    size_t out_dims[2] = {(size_t)num_frames, (size_t)state->fft_bins};
    nimcp_gpu_tensor_t* spectrogram = nimcp_gpu_tensor_create(
        state->ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!spectrogram) return NULL;

    // Allocate framed audio buffer
    size_t frame_dims[2] = {(size_t)num_frames, (size_t)state->fft_size};
    nimcp_gpu_tensor_t* frames = nimcp_gpu_tensor_create(
        state->ctx, frame_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!frames) {
        nimcp_gpu_tensor_destroy(spectrogram);
        return NULL;
    }

    // Frame audio with windowing
    dim3 frame_grid((state->fft_size + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_frame_audio<<<frame_grid, BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)frames->data,
        (const float*)state->window->data,
        num_samples, state->fft_size, state->hop_size, num_frames
    );

    // Allocate complex FFT output
    cufftComplex* d_fft_out = NULL;
    bool plan_created = false;
    cufftHandle batch_plan;

    if (cudaMalloc(&d_fft_out, num_frames * state->fft_bins * sizeof(cufftComplex)) != cudaSuccess) {
        nimcp_gpu_tensor_destroy(frames);
        nimcp_gpu_tensor_destroy(spectrogram);
        return NULL;
    }

    // Execute batched FFT
    {
        cufftResult fft_result = cufftPlan1d(&batch_plan, state->fft_size, CUFFT_R2C, num_frames);
        if (fft_result != CUFFT_SUCCESS) {
            cudaFree(d_fft_out);
            nimcp_gpu_tensor_destroy(frames);
            nimcp_gpu_tensor_destroy(spectrogram);
            return NULL;
        }
        plan_created = true;

        cufftExecR2C(batch_plan, (cufftReal*)frames->data, d_fft_out);
        cufftDestroy(batch_plan);
        plan_created = false;
    }

    // Compute magnitude spectrum
    {
        dim3 spec_grid((state->fft_bins + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
        kernel_magnitude_spectrum<<<spec_grid, BLOCK_SIZE>>>(
            d_fft_out, (float*)spectrogram->data, num_frames, state->fft_bins
        );
    }

    cudaFree(d_fft_out);
    nimcp_gpu_tensor_destroy(frames);

    state->frames_processed += num_frames;
    state->fft_operations += num_frames;

    return spectrogram;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mel_spectrogram(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0 || num_frames > state->max_frames) {
        LOG_ERROR("Invalid number of frames: %d", num_frames);
        return NULL;
    }

    // Get spectrogram (power spectrum)
    nimcp_gpu_tensor_t* spectrogram = nimcp_speech_gpu_compute_spectrogram(state, audio);
    if (!spectrogram) return NULL;

    // Allocate mel spectrogram output
    size_t mel_dims[2] = {(size_t)num_frames, (size_t)state->num_mel_bins};
    nimcp_gpu_tensor_t* mel_spec = nimcp_gpu_tensor_create(
        state->ctx, mel_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!mel_spec) {
        nimcp_gpu_tensor_destroy(spectrogram);
        return NULL;
    }

    // Convert magnitude to power
    size_t n = num_frames * state->fft_bins;
    dim3 pow_grid(GRID_SIZE(n));
    // Square the values for power spectrum (in-place would need separate kernel)

    // Apply mel filterbank using shared memory optimization
    size_t shared_mem_size = state->fft_bins * sizeof(float);
    dim3 mel_grid(state->num_mel_bins, num_frames);
    kernel_apply_mel_filterbank_shared<<<mel_grid, BLOCK_SIZE, shared_mem_size>>>(
        (const float*)spectrogram->data,
        (const float*)state->mel_filterbank->data,
        (float*)mel_spec->data,
        num_frames, state->fft_bins, state->num_mel_bins
    );

    nimcp_gpu_tensor_destroy(spectrogram);

    return mel_spec;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mfcc(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    return nimcp_speech_gpu_compute_mfcc_full(state, audio, false, false);
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mfcc_full(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    bool include_delta,
    bool include_delta2
) {
    if (!state || !audio) return NULL;

    // Get mel spectrogram
    nimcp_gpu_tensor_t* mel_spec = nimcp_speech_gpu_compute_mel_spectrogram(state, audio);
    if (!mel_spec) return NULL;

    int num_frames = mel_spec->dims[0];

    // Apply log
    size_t log_dims[2] = {(size_t)num_frames, (size_t)state->num_mel_bins};
    nimcp_gpu_tensor_t* log_mel = nimcp_gpu_tensor_create(
        state->ctx, log_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!log_mel) {
        nimcp_gpu_tensor_destroy(mel_spec);
        return NULL;
    }

    size_t n = num_frames * state->num_mel_bins;
    kernel_log_mel<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)mel_spec->data,
        (float*)log_mel->data,
        n, state->mel_floor
    );

    nimcp_gpu_tensor_destroy(mel_spec);

    // Compute MFCC via DCT
    int total_mfcc = state->num_mfcc;
    if (include_delta) total_mfcc += state->num_mfcc;
    if (include_delta2) total_mfcc += state->num_mfcc;

    size_t mfcc_dims[2] = {(size_t)num_frames, (size_t)total_mfcc};
    nimcp_gpu_tensor_t* mfcc = nimcp_gpu_tensor_create(
        state->ctx, mfcc_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!mfcc) {
        nimcp_gpu_tensor_destroy(log_mel);
        return NULL;
    }

    // Apply DCT to get base MFCC
    dim3 dct_grid((state->num_mfcc + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_apply_dct<<<dct_grid, BLOCK_SIZE>>>(
        (const float*)log_mel->data,
        (const float*)state->dct_matrix->data,
        (float*)mfcc->data,
        num_frames, state->num_mel_bins, state->num_mfcc
    );

    nimcp_gpu_tensor_destroy(log_mel);

    // Compute delta features if requested
    if (include_delta) {
        int delta_context = 2;
        float* mfcc_ptr = (float*)mfcc->data;
        float* delta_ptr = mfcc_ptr + num_frames * state->num_mfcc;

        // First, copy base MFCC to temporary buffer for delta computation
        size_t base_dims[2] = {(size_t)num_frames, (size_t)state->num_mfcc};
        nimcp_gpu_tensor_t* base_mfcc = nimcp_gpu_tensor_create(
            state->ctx, base_dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (base_mfcc) {
            cudaMemcpy(base_mfcc->data, mfcc_ptr,
                       num_frames * state->num_mfcc * sizeof(float),
                       cudaMemcpyDeviceToDevice);

            dim3 delta_grid((state->num_mfcc + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
            kernel_speech_cortex_delta_features<<<delta_grid, BLOCK_SIZE>>>(
                (const float*)base_mfcc->data,
                delta_ptr,
                num_frames, state->num_mfcc, delta_context
            );

            if (include_delta2) {
                float* delta2_ptr = delta_ptr + num_frames * state->num_mfcc;

                // Compute delta of delta
                nimcp_gpu_tensor_t* delta_tensor = nimcp_gpu_tensor_create(
                    state->ctx, base_dims, 2, NIMCP_GPU_PRECISION_FP32);
                if (delta_tensor) {
                    cudaMemcpy(delta_tensor->data, delta_ptr,
                               num_frames * state->num_mfcc * sizeof(float),
                               cudaMemcpyDeviceToDevice);

                    kernel_speech_cortex_delta_features<<<delta_grid, BLOCK_SIZE>>>(
                        (const float*)delta_tensor->data,
                        delta2_ptr,
                        num_frames, state->num_mfcc, delta_context
                    );

                    nimcp_gpu_tensor_destroy(delta_tensor);
                }
            }

            nimcp_gpu_tensor_destroy(base_mfcc);
        }
    }

    return mfcc;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_delta(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features,
    int context
) {
    if (!state || !features) return NULL;
    if (features->ndim < 2) return NULL;

    int num_frames = features->dims[0];
    int feature_dim = features->dims[1];

    size_t out_dims[2] = {(size_t)num_frames, (size_t)feature_dim};
    nimcp_gpu_tensor_t* delta = nimcp_gpu_tensor_create(
        state->ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!delta) return NULL;

    dim3 grid((feature_dim + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_delta_features<<<grid, BLOCK_SIZE>>>(
        (const float*)features->data,
        (float*)delta->data,
        num_frames, feature_dim, context
    );

    return delta;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_delta2(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features,
    int context
) {
    nimcp_gpu_tensor_t* delta = nimcp_speech_gpu_compute_delta(state, features, context);
    if (!delta) return NULL;

    nimcp_gpu_tensor_t* delta2 = nimcp_speech_gpu_compute_delta(state, delta, context);
    nimcp_gpu_tensor_destroy(delta);

    return delta2;
}

//=============================================================================
// API Implementation: Formant Extraction
//=============================================================================

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_extract_formants(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    nimcp_gpu_tensor_t* formants = NULL;
    nimcp_gpu_tensor_t* bandwidths = NULL;

    if (!nimcp_speech_gpu_extract_formants_full(state, audio, &formants, &bandwidths)) {
        return NULL;
    }

    nimcp_gpu_tensor_destroy(bandwidths);
    return formants;
}

extern "C" bool nimcp_speech_gpu_extract_formants_full(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** formants,
    nimcp_gpu_tensor_t** bandwidths
) {
    if (!state || !audio || !formants || !bandwidths) return false;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0 || num_frames > state->max_frames) {
        LOG_ERROR("Invalid number of frames: %d", num_frames);
        return false;
    }

    // Allocate output tensors
    size_t form_dims[2] = {(size_t)num_frames, SPEECH_GPU_NUM_FORMANTS};
    *formants = nimcp_gpu_tensor_create(state->ctx, form_dims, 2, NIMCP_GPU_PRECISION_FP32);
    *bandwidths = nimcp_gpu_tensor_create(state->ctx, form_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!*formants || !*bandwidths) {
        nimcp_gpu_tensor_destroy(*formants);
        nimcp_gpu_tensor_destroy(*bandwidths);
        return false;
    }

    // Frame audio
    size_t frame_dims[2] = {(size_t)num_frames, (size_t)state->frame_size};
    nimcp_gpu_tensor_t* frames = nimcp_gpu_tensor_create(
        state->ctx, frame_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!frames) {
        nimcp_gpu_tensor_destroy(*formants);
        nimcp_gpu_tensor_destroy(*bandwidths);
        return false;
    }

    dim3 frame_grid((state->frame_size + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_frame_audio<<<frame_grid, BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)frames->data,
        (const float*)state->window->data,
        num_samples, state->frame_size, state->hop_size, num_frames
    );

    // Compute autocorrelation for LPC
    size_t autocorr_dims[2] = {(size_t)num_frames, (size_t)(state->lpc_order + 1)};
    nimcp_gpu_tensor_t* autocorr = nimcp_gpu_tensor_create(
        state->ctx, autocorr_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!autocorr) {
        nimcp_gpu_tensor_destroy(frames);
        nimcp_gpu_tensor_destroy(*formants);
        nimcp_gpu_tensor_destroy(*bandwidths);
        return false;
    }

    dim3 ac_grid((state->lpc_order + 2 + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_lpc_autocorr<<<ac_grid, BLOCK_SIZE>>>(
        (const float*)frames->data,
        (float*)autocorr->data,
        num_frames, state->frame_size, state->lpc_order
    );

    // Compute LPC coefficients
    size_t lpc_dims[2] = {(size_t)num_frames, (size_t)state->lpc_order};
    nimcp_gpu_tensor_t* lpc_coeffs = nimcp_gpu_tensor_create(
        state->ctx, lpc_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* refl_coeffs = nimcp_gpu_tensor_create(
        state->ctx, lpc_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!lpc_coeffs || !refl_coeffs) {
        nimcp_gpu_tensor_destroy(frames);
        nimcp_gpu_tensor_destroy(autocorr);
        nimcp_gpu_tensor_destroy(lpc_coeffs);
        nimcp_gpu_tensor_destroy(refl_coeffs);
        nimcp_gpu_tensor_destroy(*formants);
        nimcp_gpu_tensor_destroy(*bandwidths);
        return false;
    }

    kernel_levinson_durbin_batch<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)autocorr->data,
        (float*)lpc_coeffs->data,
        (float*)refl_coeffs->data,
        num_frames, state->lpc_order
    );

    // Extract formants from LPC
    kernel_lpc_to_formants<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)lpc_coeffs->data,
        (float*)(*formants)->data,
        (float*)(*bandwidths)->data,
        num_frames, state->lpc_order, state->sample_rate,
        SPEECH_GPU_NUM_FORMANTS, 256
    );

    nimcp_gpu_tensor_destroy(frames);
    nimcp_gpu_tensor_destroy(autocorr);
    nimcp_gpu_tensor_destroy(lpc_coeffs);
    nimcp_gpu_tensor_destroy(refl_coeffs);

    return true;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_lpc(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0) return NULL;

    // Frame audio
    size_t frame_dims[2] = {(size_t)num_frames, (size_t)state->frame_size};
    nimcp_gpu_tensor_t* frames = nimcp_gpu_tensor_create(
        state->ctx, frame_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!frames) return NULL;

    dim3 frame_grid((state->frame_size + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_frame_audio<<<frame_grid, BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)frames->data,
        (const float*)state->window->data,
        num_samples, state->frame_size, state->hop_size, num_frames
    );

    // Compute autocorrelation
    size_t autocorr_dims[2] = {(size_t)num_frames, (size_t)(state->lpc_order + 1)};
    nimcp_gpu_tensor_t* autocorr = nimcp_gpu_tensor_create(
        state->ctx, autocorr_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!autocorr) {
        nimcp_gpu_tensor_destroy(frames);
        return NULL;
    }

    dim3 ac_grid((state->lpc_order + 2 + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_lpc_autocorr<<<ac_grid, BLOCK_SIZE>>>(
        (const float*)frames->data,
        (float*)autocorr->data,
        num_frames, state->frame_size, state->lpc_order
    );

    // Compute LPC coefficients
    size_t lpc_dims[2] = {(size_t)num_frames, (size_t)state->lpc_order};
    nimcp_gpu_tensor_t* lpc_coeffs = nimcp_gpu_tensor_create(
        state->ctx, lpc_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* refl_coeffs = nimcp_gpu_tensor_create(
        state->ctx, lpc_dims, 2, NIMCP_GPU_PRECISION_FP32);

    if (!lpc_coeffs || !refl_coeffs) {
        nimcp_gpu_tensor_destroy(frames);
        nimcp_gpu_tensor_destroy(autocorr);
        nimcp_gpu_tensor_destroy(lpc_coeffs);
        nimcp_gpu_tensor_destroy(refl_coeffs);
        return NULL;
    }

    kernel_levinson_durbin_batch<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)autocorr->data,
        (float*)lpc_coeffs->data,
        (float*)refl_coeffs->data,
        num_frames, state->lpc_order
    );

    nimcp_gpu_tensor_destroy(frames);
    nimcp_gpu_tensor_destroy(autocorr);
    nimcp_gpu_tensor_destroy(refl_coeffs);

    return lpc_coeffs;
}

//=============================================================================
// API Implementation: Pitch Detection
//=============================================================================

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_pitch(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    nimcp_gpu_tensor_t* pitch = NULL;
    nimcp_gpu_tensor_t* confidence = NULL;

    if (!nimcp_speech_gpu_detect_pitch_full(state, audio, &pitch, &confidence)) {
        return NULL;
    }

    nimcp_gpu_tensor_destroy(confidence);
    return pitch;
}

extern "C" bool nimcp_speech_gpu_detect_pitch_full(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** pitch,
    nimcp_gpu_tensor_t** confidence
) {
    if (!state || !audio || !pitch || !confidence) return false;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0 || num_frames > state->max_frames) {
        LOG_ERROR("Invalid number of frames: %d", num_frames);
        return false;
    }

    // Allocate output tensors
    size_t pitch_dims[1] = {(size_t)num_frames};
    *pitch = nimcp_gpu_tensor_create(state->ctx, pitch_dims, 1, NIMCP_GPU_PRECISION_FP32);
    *confidence = nimcp_gpu_tensor_create(state->ctx, pitch_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!*pitch || !*confidence) {
        nimcp_gpu_tensor_destroy(*pitch);
        nimcp_gpu_tensor_destroy(*confidence);
        return false;
    }

    // Frame audio
    size_t frame_dims[2] = {(size_t)num_frames, (size_t)state->frame_size};
    nimcp_gpu_tensor_t* frames = nimcp_gpu_tensor_create(
        state->ctx, frame_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!frames) {
        nimcp_gpu_tensor_destroy(*pitch);
        nimcp_gpu_tensor_destroy(*confidence);
        return false;
    }

    dim3 frame_grid((state->frame_size + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_frame_audio<<<frame_grid, BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)frames->data,
        (const float*)state->window->data,
        num_samples, state->frame_size, state->hop_size, num_frames
    );

    // Compute autocorrelation
    int max_lag = state->max_pitch_lag;
    size_t autocorr_dims[2] = {(size_t)num_frames, (size_t)max_lag};
    nimcp_gpu_tensor_t* autocorr = nimcp_gpu_tensor_create(
        state->ctx, autocorr_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!autocorr) {
        nimcp_gpu_tensor_destroy(frames);
        nimcp_gpu_tensor_destroy(*pitch);
        nimcp_gpu_tensor_destroy(*confidence);
        return false;
    }

    dim3 ac_grid((max_lag + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_autocorrelation_batch<<<ac_grid, BLOCK_SIZE>>>(
        (const float*)frames->data,
        (float*)autocorr->data,
        num_frames, state->frame_size, max_lag
    );

    // Find pitch peaks with parabolic interpolation
    kernel_find_pitch_refined<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)autocorr->data,
        (float*)(*pitch)->data,
        (float*)(*confidence)->data,
        num_frames, max_lag, state->min_pitch_lag, state->sample_rate
    );

    nimcp_gpu_tensor_destroy(frames);
    nimcp_gpu_tensor_destroy(autocorr);

    return true;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_pitch_hps(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    int num_harmonics
) {
    if (!state || !audio) return NULL;
    if (num_harmonics <= 0) num_harmonics = 5;

    // Get magnitude spectrum
    nimcp_gpu_tensor_t* spectrogram = nimcp_speech_gpu_compute_spectrogram(state, audio);
    if (!spectrogram) return NULL;

    int num_frames = spectrogram->dims[0];

    // Allocate HPS buffer
    size_t hps_dims[2] = {(size_t)num_frames, (size_t)state->fft_bins};
    nimcp_gpu_tensor_t* hps = nimcp_gpu_tensor_create(
        state->ctx, hps_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!hps) {
        nimcp_gpu_tensor_destroy(spectrogram);
        return NULL;
    }

    // Compute HPS
    dim3 hps_grid((state->fft_bins + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_harmonic_product_spectrum<<<hps_grid, BLOCK_SIZE>>>(
        (const float*)spectrogram->data,
        (float*)hps->data,
        num_frames, state->fft_bins, num_harmonics
    );

    // Find argmax in HPS for each frame
    size_t pitch_dims[1] = {(size_t)num_frames};
    nimcp_gpu_tensor_t* pitch = nimcp_gpu_tensor_create(
        state->ctx, pitch_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!pitch) {
        nimcp_gpu_tensor_destroy(spectrogram);
        nimcp_gpu_tensor_destroy(hps);
        return NULL;
    }

    // Convert bin to frequency (simple argmax kernel)
    // TODO: Implement proper argmax with frequency conversion

    nimcp_gpu_tensor_destroy(spectrogram);
    nimcp_gpu_tensor_destroy(hps);

    return pitch;
}

//=============================================================================
// API Implementation: Voice Activity Detection
//=============================================================================

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_vad(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    float energy_threshold
) {
    if (!state || !audio) return NULL;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0) return NULL;

    // Compute energy and ZCR
    nimcp_gpu_tensor_t* energy = nimcp_speech_gpu_compute_energy(state, audio);
    nimcp_gpu_tensor_t* zcr = nimcp_speech_gpu_compute_zcr(state, audio);
    if (!energy || !zcr) {
        nimcp_gpu_tensor_destroy(energy);
        nimcp_gpu_tensor_destroy(zcr);
        return NULL;
    }

    // Allocate VAD output
    size_t vad_dims[1] = {(size_t)num_frames};
    nimcp_gpu_tensor_t* vad = nimcp_gpu_tensor_create(
        state->ctx, vad_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!vad) {
        nimcp_gpu_tensor_destroy(energy);
        nimcp_gpu_tensor_destroy(zcr);
        return NULL;
    }

    // Compute VAD decision
    float zcr_low = 0.02f;
    float zcr_high = 0.4f;
    kernel_vad_decision<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)energy->data,
        (const float*)zcr->data,
        (float*)vad->data,
        num_frames, energy_threshold, zcr_low, zcr_high
    );

    nimcp_gpu_tensor_destroy(energy);
    nimcp_gpu_tensor_destroy(zcr);

    return vad;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_energy(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0) return NULL;

    // Frame audio
    size_t frame_dims[2] = {(size_t)num_frames, (size_t)state->frame_size};
    nimcp_gpu_tensor_t* frames = nimcp_gpu_tensor_create(
        state->ctx, frame_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!frames) return NULL;

    dim3 frame_grid((state->frame_size + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_frame_audio<<<frame_grid, BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)frames->data,
        (const float*)state->window->data,
        num_samples, state->frame_size, state->hop_size, num_frames
    );

    // Allocate energy output
    size_t energy_dims[1] = {(size_t)num_frames};
    nimcp_gpu_tensor_t* energy = nimcp_gpu_tensor_create(
        state->ctx, energy_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!energy) {
        nimcp_gpu_tensor_destroy(frames);
        return NULL;
    }

    kernel_frame_energy<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)frames->data,
        (float*)energy->data,
        num_frames, state->frame_size
    );

    nimcp_gpu_tensor_destroy(frames);

    return energy;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_zcr(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0) return NULL;

    // Frame audio (without window for ZCR)
    size_t frame_dims[2] = {(size_t)num_frames, (size_t)state->frame_size};
    nimcp_gpu_tensor_t* frames = nimcp_gpu_tensor_create(
        state->ctx, frame_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!frames) return NULL;

    // Create rectangular window for ZCR
    float* d_rect_window;
    if (cudaMalloc(&d_rect_window, state->frame_size * sizeof(float)) != cudaSuccess) {
        nimcp_gpu_tensor_destroy(frames);
        return NULL;
    }
    kernel_generate_window<<<GRID_SIZE(state->frame_size), BLOCK_SIZE>>>(
        d_rect_window, state->frame_size, 3  // Rectangular
    );

    dim3 frame_grid((state->frame_size + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_speech_cortex_frame_audio<<<frame_grid, BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)frames->data,
        d_rect_window,
        num_samples, state->frame_size, state->hop_size, num_frames
    );

    cudaFree(d_rect_window);

    // Allocate ZCR output
    size_t zcr_dims[1] = {(size_t)num_frames};
    nimcp_gpu_tensor_t* zcr = nimcp_gpu_tensor_create(
        state->ctx, zcr_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!zcr) {
        nimcp_gpu_tensor_destroy(frames);
        return NULL;
    }

    kernel_zero_crossing_rate<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)frames->data,
        (float*)zcr->data,
        num_frames, state->frame_size
    );

    nimcp_gpu_tensor_destroy(frames);

    return zcr;
}

//=============================================================================
// API Implementation: Phoneme Recognition
//=============================================================================

extern "C" nimcp_phoneme_result_gpu_t* nimcp_speech_gpu_recognize(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    // Extract features (MFCC + delta + delta2)
    nimcp_gpu_tensor_t* mfcc = nimcp_speech_gpu_compute_mfcc_full(state, audio, true, true);
    if (!mfcc) return NULL;

    int num_frames = mfcc->dims[0];
    // Note: feature_dim = mfcc->dims[1] used for classifier input dimension

    // Allocate result structure
    nimcp_phoneme_result_gpu_t* result = (nimcp_phoneme_result_gpu_t*)nimcp_calloc(
        1, sizeof(nimcp_phoneme_result_gpu_t));
    if (!result) {
        nimcp_gpu_tensor_destroy(mfcc);
        return NULL;
    }

    result->num_frames = num_frames;
    result->num_phonemes = state->num_phonemes;

    // Allocate output tensors
    size_t prob_dims[2] = {(size_t)num_frames, (size_t)state->num_phonemes};
    size_t id_dims[1] = {(size_t)num_frames};

    result->phoneme_probs = nimcp_gpu_tensor_create(
        state->ctx, prob_dims, 2, NIMCP_GPU_PRECISION_FP32);
    result->phoneme_ids = nimcp_gpu_tensor_create(
        state->ctx, id_dims, 1, NIMCP_GPU_PRECISION_INT32);
    result->phoneme_confidence = nimcp_gpu_tensor_create(
        state->ctx, id_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!result->phoneme_probs || !result->phoneme_ids || !result->phoneme_confidence) {
        nimcp_gpu_tensor_destroy(mfcc);
        nimcp_speech_gpu_free_phoneme_result(result);
        return NULL;
    }

    // If classifier is loaded, use it; otherwise use random initialization
    if (state->classifier_initialized) {
        nimcp_gpu_tensor_t* probs = nimcp_speech_gpu_classify_phonemes(state, mfcc);
        if (probs) {
            cudaMemcpy(result->phoneme_probs->data, probs->data,
                       num_frames * state->num_phonemes * sizeof(float),
                       cudaMemcpyDeviceToDevice);
            nimcp_gpu_tensor_destroy(probs);
        }
    } else {
        // Initialize with uniform probabilities
        float uniform = 1.0f / (float)state->num_phonemes;
        nimcp_gpu_fill(state->ctx, result->phoneme_probs, uniform);
    }

    // Get argmax (predicted phoneme IDs)
    kernel_argmax<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)result->phoneme_probs->data,
        (int*)result->phoneme_ids->data,
        (float*)result->phoneme_confidence->data,
        num_frames, state->num_phonemes
    );

    nimcp_gpu_tensor_destroy(mfcc);

    return result;
}

extern "C" void nimcp_speech_gpu_free_phoneme_result(nimcp_phoneme_result_gpu_t* result) {
    if (!result) return;

    nimcp_gpu_tensor_destroy(result->phoneme_probs);
    nimcp_gpu_tensor_destroy(result->phoneme_ids);
    nimcp_gpu_tensor_destroy(result->phoneme_confidence);
    nimcp_free(result);
}

extern "C" bool nimcp_speech_gpu_load_phoneme_classifier(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* bias
) {
    if (!state || !weights || !bias) return false;

    // Clone the tensors
    state->phoneme_weights = nimcp_gpu_tensor_clone(weights);
    state->phoneme_bias = nimcp_gpu_tensor_clone(bias);

    if (!state->phoneme_weights || !state->phoneme_bias) {
        nimcp_gpu_tensor_destroy(state->phoneme_weights);
        nimcp_gpu_tensor_destroy(state->phoneme_bias);
        state->phoneme_weights = NULL;
        state->phoneme_bias = NULL;
        return false;
    }

    state->classifier_initialized = true;
    LOG_INFO("Phoneme classifier loaded");

    return true;
}

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_classify_phonemes(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features
) {
    if (!state || !features) return NULL;
    if (!state->classifier_initialized) {
        LOG_ERROR("Phoneme classifier not loaded");
        return NULL;
    }

    int num_frames = features->dims[0];
    int feature_dim = features->dims[1];

    // Allocate logits
    size_t logit_dims[2] = {(size_t)num_frames, (size_t)state->num_phonemes};
    nimcp_gpu_tensor_t* logits = nimcp_gpu_tensor_create(
        state->ctx, logit_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!logits) return NULL;

    // Linear layer
    dim3 lin_grid((state->num_phonemes + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_phoneme_linear<<<lin_grid, BLOCK_SIZE>>>(
        (const float*)features->data,
        (const float*)state->phoneme_weights->data,
        (const float*)state->phoneme_bias->data,
        (float*)logits->data,
        num_frames, feature_dim, state->num_phonemes
    );

    // Allocate probabilities
    nimcp_gpu_tensor_t* probs = nimcp_gpu_tensor_create(
        state->ctx, logit_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!probs) {
        nimcp_gpu_tensor_destroy(logits);
        return NULL;
    }

    // Softmax
    kernel_softmax<<<GRID_SIZE(num_frames), BLOCK_SIZE>>>(
        (const float*)logits->data,
        (float*)probs->data,
        num_frames, state->num_phonemes
    );

    nimcp_gpu_tensor_destroy(logits);

    return probs;
}

//=============================================================================
// API Implementation: Utility Functions
//=============================================================================

extern "C" nimcp_gpu_tensor_t* nimcp_speech_gpu_preemphasis(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio,
    float coeff
) {
    if (!state || !audio) return NULL;

    int n = audio->dims[audio->ndim - 1];

    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_clone(audio);
    if (!output) return NULL;

    kernel_speech_cortex_preemphasis<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)audio->data,
        (float*)output->data,
        coeff, n
    );

    return output;
}

extern "C" bool nimcp_speech_gpu_apply_cmn(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* features
) {
    if (!state || !features || features->ndim < 2) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    bool success = false;
    float* d_mean = NULL;

    int num_frames = features->dims[0];
    int feature_dim = features->dims[1];

    // Allocate mean vector
    if (cudaMalloc(&d_mean, feature_dim * sizeof(float)) != cudaSuccess) {
        goto cleanup;
    }

    // Compute mean
    kernel_compute_mean<<<GRID_SIZE(feature_dim), BLOCK_SIZE>>>(
        (const float*)features->data,
        d_mean,
        num_frames, feature_dim
    );

    // Subtract mean
    {
        dim3 grid((feature_dim + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
        kernel_subtract_mean<<<grid, BLOCK_SIZE>>>(
            (float*)features->data,
            d_mean,
            num_frames, feature_dim
        );
    }

    success = true;

cleanup:
    if (d_mean) cudaFree(d_mean);
    return success;
}

//=============================================================================
// API Implementation: Feature Extraction (Full Pipeline)
//=============================================================================

extern "C" nimcp_speech_features_gpu_t* nimcp_speech_gpu_extract_features(
    nimcp_speech_gpu_state_t* state,
    nimcp_gpu_tensor_t* audio
) {
    if (!state || !audio) return NULL;

    nimcp_speech_features_gpu_t* features = (nimcp_speech_features_gpu_t*)nimcp_calloc(
        1, sizeof(nimcp_speech_features_gpu_t));
    if (!features) return NULL;

    int num_samples = audio->dims[audio->ndim - 1];
    int num_frames = nimcp_speech_gpu_get_num_frames(state, num_samples);
    if (num_frames <= 0) {
        nimcp_free(features);
        return NULL;
    }

    features->num_frames = num_frames;
    features->sample_rate = state->sample_rate;

    // Extract MFCC with deltas
    features->mfcc = nimcp_speech_gpu_compute_mfcc(state, audio);
    features->delta_mfcc = nimcp_speech_gpu_compute_delta(state, features->mfcc, 2);
    features->delta2_mfcc = nimcp_speech_gpu_compute_delta2(state, features->mfcc, 2);

    // Extract pitch with confidence
    nimcp_speech_gpu_detect_pitch_full(state, audio,
        &features->pitch, &features->pitch_confidence);

    // Extract formants with bandwidths
    nimcp_speech_gpu_extract_formants_full(state, audio,
        &features->formants, &features->formant_bandwidths);

    // Extract energy and ZCR
    features->energy = nimcp_speech_gpu_compute_energy(state, audio);
    features->zcr = nimcp_speech_gpu_compute_zcr(state, audio);

    // Compute VAD
    features->vad = nimcp_speech_gpu_detect_vad(state, audio, -40.0f);

    return features;
}

extern "C" void nimcp_speech_gpu_free_features(nimcp_speech_features_gpu_t* features) {
    if (!features) return;

    nimcp_gpu_tensor_destroy(features->mfcc);
    nimcp_gpu_tensor_destroy(features->delta_mfcc);
    nimcp_gpu_tensor_destroy(features->delta2_mfcc);
    nimcp_gpu_tensor_destroy(features->pitch);
    nimcp_gpu_tensor_destroy(features->pitch_confidence);
    nimcp_gpu_tensor_destroy(features->formants);
    nimcp_gpu_tensor_destroy(features->formant_bandwidths);
    nimcp_gpu_tensor_destroy(features->energy);
    nimcp_gpu_tensor_destroy(features->zcr);
    nimcp_gpu_tensor_destroy(features->vad);

    nimcp_free(features);
}

//=============================================================================
// API Implementation: Statistics
//=============================================================================

extern "C" bool nimcp_speech_gpu_get_stats(
    const nimcp_speech_gpu_state_t* state,
    nimcp_speech_gpu_stats_t* stats
) {
    if (!state || !stats) return false;

    stats->frames_processed = state->frames_processed;
    stats->fft_operations = state->fft_operations;
    stats->mfcc_extractions = 0;  // TODO: track these
    stats->pitch_detections = 0;
    stats->formant_extractions = 0;
    stats->phoneme_classifications = 0;
    stats->avg_fft_time_us = 0.0f;
    stats->avg_mfcc_time_us = 0.0f;
    stats->avg_lpc_time_us = 0.0f;
    stats->avg_pitch_time_us = 0.0f;
    stats->gpu_memory_used = 0;

    return true;
}

extern "C" void nimcp_speech_gpu_reset_stats(nimcp_speech_gpu_state_t* state) {
    if (!state) return;
    state->frames_processed = 0;
    state->fft_operations = 0;
    state->total_processing_time_ms = 0.0f;
}

//=============================================================================
// CPU Reference Implementations
//=============================================================================

extern "C" bool nimcp_speech_cpu_mel_filterbank(
    const float* power_spectrum,
    int fft_bins,
    const float* filterbank,
    int num_mel_bins,
    float* mel_energies
) {
    if (!power_spectrum || !filterbank || !mel_energies) return false;

    for (int m = 0; m < num_mel_bins; m++) {
        float sum = 0.0f;
        for (int b = 0; b < fft_bins; b++) {
            sum += power_spectrum[b] * filterbank[m * fft_bins + b];
        }
        mel_energies[m] = sum;
    }

    return true;
}

extern "C" bool nimcp_speech_cpu_autocorrelation(
    const float* signal,
    int signal_len,
    float* autocorr,
    int max_lag
) {
    if (!signal || !autocorr) return false;

    for (int lag = 0; lag < max_lag && lag < signal_len; lag++) {
        float sum = 0.0f;
        int count = signal_len - lag;
        for (int i = 0; i < count; i++) {
            sum += signal[i] * signal[i + lag];
        }
        autocorr[lag] = sum / (float)count;
    }

    return true;
}

extern "C" bool nimcp_speech_cpu_levinson_durbin(
    const float* autocorr,
    int order,
    float* lpc_coeffs,
    float* reflection_coeffs
) {
    if (!autocorr || !lpc_coeffs || !reflection_coeffs) return false;

    float* a = (float*)nimcp_calloc(order + 1, sizeof(float));
    float* a_prev = (float*)nimcp_calloc(order + 1, sizeof(float));
    if (!a || !a_prev) {
        nimcp_free(a);
        nimcp_free(a_prev);
        return false;
    }

    float error = autocorr[0];
    a[0] = 1.0f;

    if (fabsf(error) < 1e-10f) {
        memset(lpc_coeffs, 0, order * sizeof(float));
        memset(reflection_coeffs, 0, order * sizeof(float));
        nimcp_free(a);
        nimcp_free(a_prev);
        return true;
    }

    for (int k = 1; k <= order; k++) {
        float lambda = 0.0f;
        for (int j = 0; j < k; j++) {
            lambda += a[j] * autocorr[k - j];
        }

        if (fabsf(error) < 1e-10f) break;

        float refl = -lambda / error;
        reflection_coeffs[k - 1] = refl;

        memcpy(a_prev, a, (k + 1) * sizeof(float));
        for (int j = 1; j < k; j++) {
            a[j] = a_prev[j] + refl * a_prev[k - j];
        }
        a[k] = refl;

        error *= (1.0f - refl * refl);
    }

    for (int i = 0; i < order; i++) {
        lpc_coeffs[i] = a[i + 1];
    }

    nimcp_free(a);
    nimcp_free(a_prev);
    return true;
}

extern "C" bool nimcp_speech_cpu_dct(
    const float* log_mel,
    int num_mel_bins,
    int num_mfcc,
    float* mfcc
) {
    if (!log_mel || !mfcc) return false;

    float scale = sqrtf(2.0f / (float)num_mel_bins);

    for (int i = 0; i < num_mfcc; i++) {
        float sum = 0.0f;
        for (int j = 0; j < num_mel_bins; j++) {
            float angle = PI * (float)i * ((float)j + 0.5f) / (float)num_mel_bins;
            sum += log_mel[j] * cosf(angle);
        }
        mfcc[i] = scale * sum;
    }

    return true;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs (when CUDA is not available)
//=============================================================================

#include "gpu/perception/nimcp_speech_cortex_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "SPEECH_CORTEX_GPU"

nimcp_speech_gpu_config_t nimcp_speech_gpu_default_config(void) {
    nimcp_speech_gpu_config_t config;
    memset(&config, 0, sizeof(config));
    config.sample_rate = 16000;
    config.frame_size = 400;
    config.hop_size = 160;
    config.num_mel_bins = 80;
    config.num_mfcc = 13;
    config.lpc_order = 12;
    return config;
}

nimcp_speech_gpu_state_t* nimcp_speech_gpu_create(
    nimcp_gpu_context_t* ctx, int sample_rate, int frame_size,
    int hop_size, int num_mel_bins, int lpc_order) {
    LOG_WARN("CUDA not available - speech GPU processing requires GPU");
    return NULL;
}

nimcp_speech_gpu_state_t* nimcp_speech_gpu_create_with_config(
    nimcp_gpu_context_t* ctx, const nimcp_speech_gpu_config_t* config) {
    return NULL;
}

void nimcp_speech_gpu_destroy(nimcp_speech_gpu_state_t* state) {}
bool nimcp_speech_gpu_synchronize(nimcp_speech_gpu_state_t* state) { return false; }

nimcp_speech_features_gpu_t* nimcp_speech_gpu_extract_features(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
void nimcp_speech_gpu_free_features(nimcp_speech_features_gpu_t* features) {}

nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_spectrogram(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mel_spectrogram(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mfcc(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_mfcc_full(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio,
    bool include_delta, bool include_delta2) { return NULL; }

nimcp_gpu_tensor_t* nimcp_speech_gpu_extract_formants(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
bool nimcp_speech_gpu_extract_formants_full(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** formants, nimcp_gpu_tensor_t** bandwidths) { return false; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_lpc(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }

nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_pitch(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
bool nimcp_speech_gpu_detect_pitch_full(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t** pitch, nimcp_gpu_tensor_t** confidence) { return false; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_pitch_hps(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio,
    int num_harmonics) { return NULL; }

nimcp_gpu_tensor_t* nimcp_speech_gpu_detect_vad(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio,
    float energy_threshold) { return NULL; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_energy(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_zcr(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }

nimcp_phoneme_result_gpu_t* nimcp_speech_gpu_recognize(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio) { return NULL; }
void nimcp_speech_gpu_free_phoneme_result(nimcp_phoneme_result_gpu_t* result) {}
bool nimcp_speech_gpu_load_phoneme_classifier(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* bias) { return false; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_classify_phonemes(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* features) { return NULL; }

nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_delta(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* features,
    int context) { return NULL; }
nimcp_gpu_tensor_t* nimcp_speech_gpu_compute_delta2(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* features,
    int context) { return NULL; }

nimcp_gpu_tensor_t* nimcp_speech_gpu_preemphasis(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* audio,
    float coeff) { return NULL; }
bool nimcp_speech_gpu_apply_cmn(
    nimcp_speech_gpu_state_t* state, nimcp_gpu_tensor_t* features) { return false; }
int nimcp_speech_gpu_get_num_frames(
    const nimcp_speech_gpu_state_t* state, int num_samples) { return 0; }

bool nimcp_speech_gpu_get_stats(
    const nimcp_speech_gpu_state_t* state, nimcp_speech_gpu_stats_t* stats) { return false; }
void nimcp_speech_gpu_reset_stats(nimcp_speech_gpu_state_t* state) {}

bool nimcp_speech_cpu_mel_filterbank(
    const float* power_spectrum, int fft_bins, const float* filterbank,
    int num_mel_bins, float* mel_energies) { return false; }
bool nimcp_speech_cpu_autocorrelation(
    const float* signal, int signal_len, float* autocorr, int max_lag) { return false; }
bool nimcp_speech_cpu_levinson_durbin(
    const float* autocorr, int order, float* lpc_coeffs,
    float* reflection_coeffs) { return false; }
bool nimcp_speech_cpu_dct(
    const float* log_mel, int num_mel_bins, int num_mfcc, float* mfcc) { return false; }

#endif // NIMCP_ENABLE_CUDA
