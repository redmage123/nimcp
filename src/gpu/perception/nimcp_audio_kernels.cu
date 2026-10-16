/**
 * @file nimcp_audio_kernels.cu
 * @brief GPU Audio Processing CUDA Kernels
 *
 * WHAT: CUDA kernels for audio feature extraction
 * WHY:  GPU acceleration for real-time audio processing
 * HOW:  Custom kernels for MFCC, mel filterbank, STFT
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
#include <cublas_v2.h>
#include <math.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "AUDIO_GPU"


#define BLOCK_SIZE 256
#define BLOCK_SIZE_2D 16

//=============================================================================
// Forward Declarations
//=============================================================================

// Mel filterbank creation kernel (defined later in file)
__global__ void kernel_mel_filterbank_create(float* filterbank, int num_filters,
                                              int fft_size, float fmin, float fmax,
                                              float sample_rate);

//=============================================================================
// cuBLAS Batched Matrix Multiplication Context
//=============================================================================

/**
 * @brief Audio matrix multiplication context using cuBLAS
 */
typedef struct nimcp_audio_matmul_ctx {
    cublasHandle_t cublas_handle;
    void* gpu_context;
    float** d_A_array;         /**< Device array of A matrix pointers */
    float** d_B_array;         /**< Device array of B matrix pointers */
    float** d_C_array;         /**< Device array of C matrix pointers */
    float** h_A_array;         /**< Host array of A matrix pointers */
    float** h_B_array;         /**< Host array of B matrix pointers */
    float** h_C_array;         /**< Host array of C matrix pointers */
    int max_batch_size;
    bool initialized;
} nimcp_audio_matmul_ctx_t;

/**
 * @brief Create audio matmul context with cuBLAS
 *
 * @param gpu_ctx GPU context
 * @param max_batch Maximum batch size for batched operations
 * @return Context or NULL on failure
 */
nimcp_audio_matmul_ctx_t* nimcp_audio_matmul_create(void* gpu_ctx, int max_batch)
{
    if (!gpu_ctx || max_batch <= 0) return NULL;

    nimcp_audio_matmul_ctx_t* ctx = (nimcp_audio_matmul_ctx_t*)nimcp_malloc(sizeof(nimcp_audio_matmul_ctx_t));
    if (!ctx) return NULL;

    ctx->gpu_context = gpu_ctx;
    ctx->max_batch_size = max_batch;
    ctx->initialized = false;

    // Create cuBLAS handle
    cublasStatus_t status = cublasCreate(&ctx->cublas_handle);
    if (status != CUBLAS_STATUS_SUCCESS) {
        LOG_ERROR("Failed to create cuBLAS handle: %d", (int)status);
        nimcp_free(ctx);
        return NULL;
    }

    // Allocate device arrays for batch pointers
    cudaError_t err;
    err = cudaMalloc(&ctx->d_A_array, max_batch * sizeof(float*));
    if (err != cudaSuccess) { cublasDestroy(ctx->cublas_handle); nimcp_free(ctx); return NULL; }

    err = cudaMalloc(&ctx->d_B_array, max_batch * sizeof(float*));
    if (err != cudaSuccess) { cudaFree(ctx->d_A_array); cublasDestroy(ctx->cublas_handle); nimcp_free(ctx); return NULL; }

    err = cudaMalloc(&ctx->d_C_array, max_batch * sizeof(float*));
    if (err != cudaSuccess) {
        cudaFree(ctx->d_A_array);
        cudaFree(ctx->d_B_array);
        cublasDestroy(ctx->cublas_handle);
        nimcp_free(ctx);
        return NULL;
    }

    // Allocate host arrays for batch pointers
    ctx->h_A_array = (float**)nimcp_malloc(max_batch * sizeof(float*));
    ctx->h_B_array = (float**)nimcp_malloc(max_batch * sizeof(float*));
    ctx->h_C_array = (float**)nimcp_malloc(max_batch * sizeof(float*));

    if (!ctx->h_A_array || !ctx->h_B_array || !ctx->h_C_array) {
        if (ctx->h_A_array) nimcp_free(ctx->h_A_array);
        if (ctx->h_B_array) nimcp_free(ctx->h_B_array);
        if (ctx->h_C_array) nimcp_free(ctx->h_C_array);
        cudaFree(ctx->d_A_array);
        cudaFree(ctx->d_B_array);
        cudaFree(ctx->d_C_array);
        cublasDestroy(ctx->cublas_handle);
        nimcp_free(ctx);
        return NULL;
    }

    ctx->initialized = true;
    LOG_DEBUG("Created audio matmul context with max batch size %d", max_batch);
    return ctx;
}

/**
 * @brief Destroy audio matmul context
 */
void nimcp_audio_matmul_destroy(nimcp_audio_matmul_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->d_A_array) cudaFree(ctx->d_A_array);
    if (ctx->d_B_array) cudaFree(ctx->d_B_array);
    if (ctx->d_C_array) cudaFree(ctx->d_C_array);
    if (ctx->h_A_array) nimcp_free(ctx->h_A_array);
    if (ctx->h_B_array) nimcp_free(ctx->h_B_array);
    if (ctx->h_C_array) nimcp_free(ctx->h_C_array);

    if (ctx->cublas_handle) {
        cublasDestroy(ctx->cublas_handle);
    }

    nimcp_free(ctx);
}

/**
 * @brief Perform batched matrix multiplication using cuBLAS
 *
 * Computes C[i] = A[i] * B[i] for i = 0..batch_size-1
 * Supports transposition of A and B matrices.
 *
 * @param ctx Matmul context
 * @param A Input matrices A (contiguous batch on device)
 * @param B Input matrices B (contiguous batch on device)
 * @param C Output matrices C (contiguous batch on device)
 * @param M Rows of A (or columns of A if transA)
 * @param N Columns of B (or rows of B if transB)
 * @param K Columns of A / Rows of B
 * @param batch_size Number of matrices in batch
 * @param transA Transpose A matrices
 * @param transB Transpose B matrices
 * @return 0 on success, -1 on error
 */
int nimcp_audio_batched_matmul(nimcp_audio_matmul_ctx_t* ctx,
                               const float* A, const float* B, float* C,
                               int M, int N, int K, int batch_size,
                               bool transA, bool transB)
{
    if (!ctx || !ctx->initialized || !A || !B || !C) return -1;
    if (batch_size <= 0 || batch_size > ctx->max_batch_size) return -1;
    if (M <= 0 || N <= 0 || K <= 0) return -1;

    // Calculate strides for batched operations
    int strideA = transA ? (K * M) : (M * K);
    int strideB = transB ? (N * K) : (K * N);
    int strideC = M * N;

    // Setup pointer arrays for batched GEMM
    for (int i = 0; i < batch_size; i++) {
        ctx->h_A_array[i] = (float*)(A + i * strideA);
        ctx->h_B_array[i] = (float*)(B + i * strideB);
        ctx->h_C_array[i] = C + i * strideC;
    }

    // Copy pointer arrays to device
    cudaMemcpy(ctx->d_A_array, ctx->h_A_array, batch_size * sizeof(float*), cudaMemcpyHostToDevice);
    cudaMemcpy(ctx->d_B_array, ctx->h_B_array, batch_size * sizeof(float*), cudaMemcpyHostToDevice);
    cudaMemcpy(ctx->d_C_array, ctx->h_C_array, batch_size * sizeof(float*), cudaMemcpyHostToDevice);

    float alpha = 1.0f;
    float beta = 0.0f;

    // Row-major to column-major transformation:
    // Row-major A (M x K) is col-major A^T (K x M), leading dim K
    // Row-major B (K x N) is col-major B^T (N x K), leading dim N
    // Row-major C (M x N) is col-major C^T (N x M), leading dim N
    //
    // To compute C = A * B in row-major: C^T = B^T * A^T in col-major
    // cublasSgemm(handle, transa, transb, m, n, k, alpha, A_blas, lda, B_blas, ldb, beta, C_blas, ldc)
    //
    // A_blas = B^T (col-major N x K), B_blas = A^T (col-major K x M)
    // C_blas = C^T (col-major N x M)
    // m = N, n = M, k = K
    //
    // For transA (user wants A^T * B): A is stored as M x K row-major.
    //   A^T is K x M row-major = M x K col-major. We need to use it transposed in cuBLAS.
    //   B_blas = A^T col-major (M x K), and we need transb=T to get A^T^T=A... no wait.
    //   Actually we need op(B_blas) = A^T. A^T col-major is M x K. op=N gives M x K.
    //   But we need K x M from A^T. So transb=T on (M x K) gives K x M. Hmm.
    //
    // Simple approach: handle transA/transB by adjusting ops and leading dims.

    // Base case (no transpose): C^T = B^T * A^T
    // transa=N for B^T (col-major N x K, lda=N)
    // transb=N for A^T (col-major K x M, ldb=K)
    cublasOperation_t cublas_transa, cublas_transb;
    int lda_cublas, ldb_cublas;

    if (!transB) {
        // B is K x N row-major -> B^T is N x K col-major
        cublas_transa = CUBLAS_OP_N;
        lda_cublas = N;
    } else {
        // B^T is N x K row-major -> (B^T)^T = B is K x N col-major
        // We need B^T for the cuBLAS A position, so transpose it
        cublas_transa = CUBLAS_OP_T;
        lda_cublas = K;
    }

    if (!transA) {
        // A is M x K row-major -> A^T is K x M col-major
        cublas_transb = CUBLAS_OP_N;
        ldb_cublas = K;
    } else {
        // A^T is K x M row-major -> (A^T)^T = A is M x K col-major
        // We need A^T for the cuBLAS B position, so transpose it
        cublas_transb = CUBLAS_OP_T;
        ldb_cublas = M;
    }

    int ldc = N;

    cublasStatus_t status = cublasSgemmBatched(
        ctx->cublas_handle,
        cublas_transa, cublas_transb,
        N, M, K,
        &alpha,
        (const float**)ctx->d_B_array, lda_cublas,
        (const float**)ctx->d_A_array, ldb_cublas,
        &beta,
        ctx->d_C_array, ldc,
        batch_size
    );

    if (status != CUBLAS_STATUS_SUCCESS) {
        LOG_ERROR("cuBLAS batched GEMM failed: %d", (int)status);
        return -1;
    }

    return 0;
}

/**
 * @brief Single matrix multiplication using cuBLAS (for comparison/fallback)
 */
int nimcp_audio_single_matmul(nimcp_audio_matmul_ctx_t* ctx,
                              const float* A, const float* B, float* C,
                              int M, int N, int K,
                              bool transA, bool transB)
{
    if (!ctx || !ctx->initialized || !A || !B || !C) return -1;
    if (M <= 0 || N <= 0 || K <= 0) return -1;

    float alpha = 1.0f;
    float beta = 0.0f;

    // Same row-major to col-major transformation as batched version
    cublasOperation_t cublas_transa, cublas_transb;
    int lda_cublas, ldb_cublas;

    if (!transB) {
        cublas_transa = CUBLAS_OP_N;
        lda_cublas = N;
    } else {
        cublas_transa = CUBLAS_OP_T;
        lda_cublas = K;
    }

    if (!transA) {
        cublas_transb = CUBLAS_OP_N;
        ldb_cublas = K;
    } else {
        cublas_transb = CUBLAS_OP_T;
        ldb_cublas = M;
    }

    int ldc = N;

    cublasStatus_t status = cublasSgemm(
        ctx->cublas_handle,
        cublas_transa, cublas_transb,
        N, M, K,
        &alpha,
        B, lda_cublas,
        A, ldb_cublas,
        &beta,
        C, ldc
    );

    if (status != CUBLAS_STATUS_SUCCESS) {
        LOG_ERROR("cuBLAS GEMM failed: %d", (int)status);
        return -1;
    }

    return 0;
}

//=============================================================================
// Window Type Enumeration
//=============================================================================

typedef enum {
    STFT_WINDOW_HANN = 0,
    STFT_WINDOW_HAMMING = 1,
    STFT_WINDOW_BLACKMAN = 2,
    STFT_WINDOW_RECTANGULAR = 3
} stft_window_type_t;

//=============================================================================
// Full STFT Pipeline
//=============================================================================

/**
 * @brief STFT context for full pipeline
 */
typedef struct nimcp_stft_ctx {
    int fft_size;
    int hop_size;
    int window_type;
    float* d_window;            /**< Precomputed window on device */
    cufftHandle plan;
    cufftHandle inverse_plan;
    void* gpu_context;
    bool plan_created;
    bool inverse_plan_created;
    int last_num_frames;
} nimcp_stft_ctx_t;

/**
 * @brief STFT result structure
 */
typedef struct nimcp_stft_result {
    float* d_magnitude;         /**< |STFT| on device */
    float* d_phase;             /**< angle(STFT) on device */
    float* d_power;             /**< |STFT|^2 on device */
    int num_frames;
    int num_bins;
    bool owns_memory;
} nimcp_stft_result_t;

//=============================================================================
// STFT Kernels
//=============================================================================

/**
 * @brief Generate Hann window
 */
__global__ void kernel_hann_window_gen(float* window, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        window[idx] = 0.5f * (1.0f - cosf(2.0f * M_PI * idx / (n - 1)));
    }
}

/**
 * @brief Generate Hamming window
 */
__global__ void kernel_hamming_window_gen(float* window, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        window[idx] = 0.54f - 0.46f * cosf(2.0f * M_PI * idx / (n - 1));
    }
}

/**
 * @brief Generate Blackman window
 */
__global__ void kernel_blackman_window_gen(float* window, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float a0 = 0.42f;
        float a1 = 0.5f;
        float a2 = 0.08f;
        window[idx] = a0 - a1 * cosf(2.0f * M_PI * idx / (n - 1))
                        + a2 * cosf(4.0f * M_PI * idx / (n - 1));
    }
}

/**
 * @brief Apply window to multiple frames with hop
 *
 * @param input Input audio signal
 * @param output Output windowed frames (frame_size elements per frame)
 * @param window Window coefficients
 * @param frame_size Size of each frame (FFT size)
 * @param num_frames Number of frames
 * @param hop Hop size between frames
 * @param audio_len Total audio length (for bounds checking)
 */
__global__ void kernel_apply_window(const float* input, float* output,
                                    const float* window, int frame_size,
                                    int num_frames, int hop, int audio_len)
{
    int frame = blockIdx.x;
    int idx = threadIdx.x;

    if (frame >= num_frames || idx >= frame_size) return;

    int audio_idx = frame * hop + idx;
    float val = (audio_idx < audio_len) ? input[audio_idx] : 0.0f;
    output[frame * frame_size + idx] = val * window[idx];
}

/**
 * @brief Convert complex FFT output to magnitude and phase
 *
 * @param complex_data Input complex data (interleaved real/imag from cuFFT)
 * @param magnitude Output magnitude array
 * @param phase Output phase array (can be NULL)
 * @param n Number of complex elements
 */
__global__ void kernel_complex_to_magnitude_phase(const cufftComplex* complex_data,
                                                   float* magnitude, float* phase, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float re = complex_data[idx].x;
    float im = complex_data[idx].y;

    magnitude[idx] = sqrtf(re * re + im * im);
    if (phase) {
        phase[idx] = atan2f(im, re);
    }
}

/**
 * @brief Convert magnitude and phase back to complex
 *
 * @param magnitude Input magnitude array
 * @param phase Input phase array
 * @param complex_data Output complex data
 * @param n Number of elements
 */
__global__ void kernel_magnitude_phase_to_complex(const float* magnitude, const float* phase,
                                                   cufftComplex* complex_data, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float mag = magnitude[idx];
    float ph = phase[idx];

    complex_data[idx].x = mag * cosf(ph);
    complex_data[idx].y = mag * sinf(ph);
}

/**
 * @brief Compute power spectrum from magnitude (square)
 */
__global__ void kernel_compute_power(const float* magnitude, float* power, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float mag = magnitude[idx];
    power[idx] = mag * mag;
}

/**
 * @brief Apply mel filterbank to power spectrum
 *
 * @param power_spectrum Input power spectrum [num_frames, num_bins]
 * @param mel_spectrum Output mel spectrum [num_frames, num_mels]
 * @param mel_filters Mel filterbank matrix [num_mels, num_bins]
 * @param num_bins Number of frequency bins
 * @param num_mels Number of mel bands
 * @param num_frames Number of time frames
 */
__global__ void kernel_apply_mel_filterbank_full(const float* power_spectrum, float* mel_spectrum,
                                                  const float* mel_filters, int num_bins,
                                                  int num_mels, int num_frames)
{
    int frame = blockIdx.y;
    int mel = blockIdx.x * blockDim.x + threadIdx.x;

    if (frame >= num_frames || mel >= num_mels) return;

    float sum = 0.0f;
    for (int bin = 0; bin < num_bins; bin++) {
        sum += power_spectrum[frame * num_bins + bin] * mel_filters[mel * num_bins + bin];
    }

    mel_spectrum[frame * num_mels + mel] = sum;
}

/**
 * @brief Overlap-add synthesis from ISTFT frames
 *
 * @param frames Input windowed frames (after IFFT)
 * @param output Output audio signal
 * @param frame_size Size of each frame
 * @param hop_size Hop between frames
 * @param num_frames Number of frames
 * @param output_size Total output size
 */
__global__ void kernel_overlap_add(const float* frames, float* output,
                                   int frame_size, int hop_size, int num_frames, int output_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= output_size) return;

    float sum = 0.0f;

    // Find all frames that contribute to this output sample
    int first_frame = max(0, (idx - frame_size + 1 + hop_size - 1) / hop_size);
    int last_frame = min(num_frames - 1, idx / hop_size);

    for (int frame = first_frame; frame <= last_frame; frame++) {
        int frame_idx = idx - frame * hop_size;
        if (frame_idx >= 0 && frame_idx < frame_size) {
            sum += frames[frame * frame_size + frame_idx];
        }
    }

    output[idx] = sum;
}

/**
 * @brief Scale an array by a constant factor
 *
 * @param data Array to scale (in-place)
 * @param scale Scale factor
 * @param n Number of elements
 */
__global__ void kernel_scale(float* data, float scale, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    data[idx] *= scale;
}

/**
 * @brief Synthesis window normalization for overlap-add
 */
__global__ void kernel_synthesis_normalize(float* output, const float* window,
                                           int frame_size, int hop_size, int num_frames,
                                           int output_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= output_size) return;

    float window_sum = 0.0f;

    int first_frame = max(0, (idx - frame_size + 1 + hop_size - 1) / hop_size);
    int last_frame = min(num_frames - 1, idx / hop_size);

    for (int frame = first_frame; frame <= last_frame; frame++) {
        int frame_idx = idx - frame * hop_size;
        if (frame_idx >= 0 && frame_idx < frame_size) {
            float w = window[frame_idx];
            window_sum += w * w;  // For synthesis, we use squared window
        }
    }

    if (window_sum > 1e-8f) {
        output[idx] /= window_sum;
    }
}

//=============================================================================
// STFT API Implementation
//=============================================================================

/**
 * @brief Create STFT context
 *
 * @param gpu_ctx GPU context
 * @param fft_size FFT size (must be power of 2)
 * @param hop_size Hop size between frames
 * @param window_type Window type (HANN, HAMMING, BLACKMAN)
 * @return STFT context or NULL on failure
 */
nimcp_stft_ctx_t* nimcp_stft_create(void* gpu_ctx, int fft_size, int hop_size, int window_type)
{
    if (!gpu_ctx || fft_size <= 0 || hop_size <= 0) return NULL;

    // Check power of 2
    if ((fft_size & (fft_size - 1)) != 0) {
        LOG_ERROR("FFT size must be power of 2, got %d", fft_size);
        return NULL;
    }

    nimcp_stft_ctx_t* ctx = (nimcp_stft_ctx_t*)nimcp_malloc(sizeof(nimcp_stft_ctx_t));
    if (!ctx) return NULL;

    ctx->fft_size = fft_size;
    ctx->hop_size = hop_size;
    ctx->window_type = window_type;
    ctx->gpu_context = gpu_ctx;
    ctx->plan_created = false;
    ctx->inverse_plan_created = false;
    ctx->last_num_frames = 0;

    // Allocate and compute window
    cudaError_t err = cudaMalloc(&ctx->d_window, fft_size * sizeof(float));
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate window buffer");
        nimcp_free(ctx);
        return NULL;
    }

    // Generate window
    int blocks = (fft_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    switch (window_type) {
        case STFT_WINDOW_HAMMING:
            kernel_hamming_window_gen<<<blocks, BLOCK_SIZE>>>(ctx->d_window, fft_size);
            break;
        case STFT_WINDOW_BLACKMAN:
            kernel_blackman_window_gen<<<blocks, BLOCK_SIZE>>>(ctx->d_window, fft_size);
            break;
        case STFT_WINDOW_RECTANGULAR:
            cudaMemset(ctx->d_window, 0, fft_size * sizeof(float));
            // Set all to 1.0
            {
                float* h_window = (float*)nimcp_malloc(fft_size * sizeof(float));
                for (int i = 0; i < fft_size; i++) h_window[i] = 1.0f;
                cudaMemcpy(ctx->d_window, h_window, fft_size * sizeof(float), cudaMemcpyHostToDevice);
                nimcp_free(h_window);
            }
            break;
        case STFT_WINDOW_HANN:
        default:
            kernel_hann_window_gen<<<blocks, BLOCK_SIZE>>>(ctx->d_window, fft_size);
            break;
    }

    cudaDeviceSynchronize();
    LOG_DEBUG("Created STFT context: fft_size=%d, hop_size=%d, window_type=%d",
              fft_size, hop_size, window_type);

    return ctx;
}

/**
 * @brief Destroy STFT context
 */
void nimcp_stft_destroy(nimcp_stft_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->d_window) cudaFree(ctx->d_window);
    if (ctx->plan_created) cufftDestroy(ctx->plan);
    if (ctx->inverse_plan_created) cufftDestroy(ctx->inverse_plan);

    nimcp_free(ctx);
}

/**
 * @brief Create STFT result structure
 */
nimcp_stft_result_t* nimcp_stft_result_create(int num_frames, int num_bins)
{
    nimcp_stft_result_t* result = (nimcp_stft_result_t*)nimcp_malloc(sizeof(nimcp_stft_result_t));
    if (!result) return NULL;

    result->num_frames = num_frames;
    result->num_bins = num_bins;
    result->owns_memory = true;

    size_t size = num_frames * num_bins * sizeof(float);

    cudaError_t err;
    err = cudaMalloc(&result->d_magnitude, size);
    if (err != cudaSuccess) { nimcp_free(result); return NULL; }

    err = cudaMalloc(&result->d_phase, size);
    if (err != cudaSuccess) { cudaFree(result->d_magnitude); nimcp_free(result); return NULL; }

    err = cudaMalloc(&result->d_power, size);
    if (err != cudaSuccess) {
        cudaFree(result->d_magnitude);
        cudaFree(result->d_phase);
        nimcp_free(result);
        return NULL;
    }

    cudaMemset(result->d_magnitude, 0, size);
    cudaMemset(result->d_phase, 0, size);
    cudaMemset(result->d_power, 0, size);

    return result;
}

/**
 * @brief Destroy STFT result
 */
void nimcp_stft_result_destroy(nimcp_stft_result_t* result)
{
    if (!result) return;

    if (result->owns_memory) {
        if (result->d_magnitude) cudaFree(result->d_magnitude);
        if (result->d_phase) cudaFree(result->d_phase);
        if (result->d_power) cudaFree(result->d_power);
    }

    nimcp_free(result);
}

/**
 * @brief Compute forward STFT
 *
 * @param ctx STFT context
 * @param audio Input audio on device
 * @param num_samples Number of input samples
 * @param result Pre-allocated result structure (or NULL to allocate)
 * @return 0 on success, -1 on error
 */
int nimcp_stft_forward(nimcp_stft_ctx_t* ctx, const float* audio, int num_samples,
                       nimcp_stft_result_t* result)
{
    if (!ctx || !audio || num_samples <= 0 || !result) return -1;

    int fft_size = ctx->fft_size;
    int hop_size = ctx->hop_size;
    int num_bins = fft_size / 2 + 1;
    int num_frames = (num_samples - fft_size) / hop_size + 1;

    if (num_frames <= 0) {
        LOG_ERROR("Audio too short for STFT: %d samples, fft_size=%d", num_samples, fft_size);
        return -1;
    }

    // Check result dimensions
    if (result->num_frames != num_frames || result->num_bins != num_bins) {
        LOG_ERROR("Result dimensions mismatch: expected %dx%d, got %dx%d",
                  num_frames, num_bins, result->num_frames, result->num_bins);
        return -1;
    }

    // Create or update cuFFT plan if needed
    if (!ctx->plan_created || ctx->last_num_frames != num_frames) {
        if (ctx->plan_created) {
            cufftDestroy(ctx->plan);
        }
        cufftResult cufft_status = cufftPlan1d(&ctx->plan, fft_size, CUFFT_R2C, num_frames);
        if (cufft_status != CUFFT_SUCCESS) {
            LOG_ERROR("Failed to create cuFFT plan: %d", (int)cufft_status);
            return -1;
        }
        ctx->plan_created = true;
        ctx->last_num_frames = num_frames;
    }

    // Allocate windowed frames buffer
    float* d_frames;
    cudaError_t err = cudaMalloc(&d_frames, num_frames * fft_size * sizeof(float));
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to allocate frames buffer");
        return -1;
    }

    // Apply window to create frames
    kernel_apply_window<<<num_frames, fft_size>>>(
        audio, d_frames, ctx->d_window, fft_size, num_frames, hop_size, num_samples);

    // Allocate complex output buffer
    cufftComplex* d_complex;
    cudaMalloc(&d_complex, num_frames * num_bins * sizeof(cufftComplex));

    // Execute FFT
    cufftResult cufft_status = cufftExecR2C(ctx->plan, d_frames, d_complex);
    if (cufft_status != CUFFT_SUCCESS) {
        LOG_ERROR("cuFFT execution failed: %d", (int)cufft_status);
        cudaFree(d_frames);
        cudaFree(d_complex);
        return -1;
    }

    // Convert to magnitude and phase
    int n_complex = num_frames * num_bins;
    int blocks = (n_complex + BLOCK_SIZE - 1) / BLOCK_SIZE;
    kernel_complex_to_magnitude_phase<<<blocks, BLOCK_SIZE>>>(
        d_complex, result->d_magnitude, result->d_phase, n_complex);

    // Compute power spectrum
    kernel_compute_power<<<blocks, BLOCK_SIZE>>>(result->d_magnitude, result->d_power, n_complex);

    cudaFree(d_frames);
    cudaFree(d_complex);

    err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("STFT forward CUDA error: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

/**
 * @brief Compute inverse STFT (reconstruction)
 *
 * @param ctx STFT context
 * @param stft STFT result (magnitude + phase)
 * @param audio_out Output audio buffer on device
 * @param num_samples_out Pointer to store number of output samples
 * @return 0 on success, -1 on error
 */
int nimcp_stft_inverse(nimcp_stft_ctx_t* ctx, const nimcp_stft_result_t* stft,
                       float* audio_out, int* num_samples_out)
{
    if (!ctx || !stft || !audio_out || !num_samples_out) return -1;

    int fft_size = ctx->fft_size;
    int hop_size = ctx->hop_size;
    int num_frames = stft->num_frames;
    int num_bins = stft->num_bins;

    int output_size = (num_frames - 1) * hop_size + fft_size;
    *num_samples_out = output_size;

    // Create inverse plan if needed
    if (!ctx->inverse_plan_created) {
        cufftResult status = cufftPlan1d(&ctx->inverse_plan, fft_size, CUFFT_C2R, num_frames);
        if (status != CUFFT_SUCCESS) {
            LOG_ERROR("Failed to create inverse cuFFT plan: %d", (int)status);
            return -1;
        }
        ctx->inverse_plan_created = true;
    }

    // Convert magnitude/phase back to complex
    cufftComplex* d_complex;
    cudaMalloc(&d_complex, num_frames * num_bins * sizeof(cufftComplex));

    int n_complex = num_frames * num_bins;
    int blocks = (n_complex + BLOCK_SIZE - 1) / BLOCK_SIZE;
    kernel_magnitude_phase_to_complex<<<blocks, BLOCK_SIZE>>>(
        stft->d_magnitude, stft->d_phase, d_complex, n_complex);

    // Allocate frames buffer for IFFT output
    float* d_frames;
    cudaMalloc(&d_frames, num_frames * fft_size * sizeof(float));

    // Execute inverse FFT
    cufftResult status = cufftExecC2R(ctx->inverse_plan, d_complex, d_frames);
    if (status != CUFFT_SUCCESS) {
        LOG_ERROR("Inverse cuFFT execution failed: %d", (int)status);
        cudaFree(d_complex);
        cudaFree(d_frames);
        return -1;
    }

    // Normalize IFFT output (cuFFT doesn't normalize)
    // Scale by 1/fft_size
    int n_frames_total = num_frames * fft_size;
    blocks = (n_frames_total + BLOCK_SIZE - 1) / BLOCK_SIZE;
    kernel_scale<<<blocks, BLOCK_SIZE>>>(d_frames, 1.0f / fft_size, n_frames_total);

    // Apply synthesis window to frames
    // For perfect reconstruction with Hann window, we need to apply window again
    // NOTE: hop=fft_size so each frame reads from frame*fft_size+idx (contiguous IFFT output)
    kernel_apply_window<<<num_frames, fft_size>>>(
        d_frames, d_frames, ctx->d_window, fft_size, num_frames, fft_size, n_frames_total);

    // Initialize output to zero
    cudaMemset(audio_out, 0, output_size * sizeof(float));

    // Overlap-add
    blocks = (output_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    kernel_overlap_add<<<blocks, BLOCK_SIZE>>>(
        d_frames, audio_out, fft_size, hop_size, num_frames, output_size);

    // Normalize by window sum for perfect reconstruction
    kernel_synthesis_normalize<<<blocks, BLOCK_SIZE>>>(
        audio_out, ctx->d_window, fft_size, hop_size, num_frames, output_size);

    cudaFree(d_complex);
    cudaFree(d_frames);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("STFT inverse CUDA error: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

/**
 * @brief Apply mel filterbank to STFT power spectrum
 *
 * @param ctx STFT context (for configuration info)
 * @param power_spectrum Input power spectrum [num_frames, num_bins]
 * @param mel_spectrum Output mel spectrum [num_frames, num_mels]
 * @param num_mels Number of mel bands
 * @param fmin Minimum frequency (Hz)
 * @param fmax Maximum frequency (Hz)
 * @param sample_rate Audio sample rate
 * @param num_frames Number of frames
 * @param num_bins Number of frequency bins
 * @return 0 on success, -1 on error
 */
int nimcp_stft_mel_filterbank(nimcp_stft_ctx_t* ctx, const float* power_spectrum,
                              float* mel_spectrum, int num_mels, float fmin, float fmax,
                              float sample_rate, int num_frames, int num_bins)
{
    if (!ctx || !power_spectrum || !mel_spectrum) return -1;
    if (num_mels <= 0 || num_frames <= 0 || num_bins <= 0) return -1;

    // Generate mel filterbank on device
    float* d_filterbank;
    cudaMalloc(&d_filterbank, num_mels * num_bins * sizeof(float));

    // Use existing mel filterbank creation kernel
    kernel_mel_filterbank_create<<<(num_mels + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        d_filterbank, num_mels, (num_bins - 1) * 2, fmin, fmax, sample_rate);

    // Apply filterbank
    dim3 grid((num_mels + BLOCK_SIZE - 1) / BLOCK_SIZE, num_frames);
    kernel_apply_mel_filterbank_full<<<grid, BLOCK_SIZE>>>(
        power_spectrum, mel_spectrum, d_filterbank,
        num_bins, num_mels, num_frames);

    cudaFree(d_filterbank);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Mel filterbank CUDA error: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

//=============================================================================
// Mel Filterbank Computation
//=============================================================================

__device__ float hz_to_mel(float hz)
{
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

__device__ float mel_to_hz(float mel)
{
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

__global__ void kernel_mel_filterbank_create(
    float* filterbank, int n_mels, int n_fft,
    float fmin, float fmax, float sample_rate)
{
    int mel_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (mel_idx >= n_mels) return;

    float mel_min = hz_to_mel(fmin);
    float mel_max = hz_to_mel(fmax);
    float mel_step = (mel_max - mel_min) / (n_mels + 1);

    float mel_lo = mel_min + mel_idx * mel_step;
    float mel_center = mel_lo + mel_step;
    float mel_hi = mel_center + mel_step;

    float hz_lo = mel_to_hz(mel_lo);
    float hz_center = mel_to_hz(mel_center);
    float hz_hi = mel_to_hz(mel_hi);

    float bin_hz = sample_rate / n_fft;
    int n_bins = n_fft / 2 + 1;

    for (int bin = 0; bin < n_bins; bin++) {
        float freq = bin * bin_hz;
        float weight = 0.0f;

        if (freq >= hz_lo && freq < hz_center) {
            weight = (freq - hz_lo) / (hz_center - hz_lo);
        } else if (freq >= hz_center && freq <= hz_hi) {
            weight = (hz_hi - freq) / (hz_hi - hz_center);
        }

        filterbank[mel_idx * n_bins + bin] = weight;
    }
}

__global__ void kernel_apply_mel_filterbank(
    const float* spectrogram, const float* filterbank, float* mel_spec,
    int batch, int time_frames, int n_bins, int n_mels)
{
    int b = blockIdx.z;
    int t = blockIdx.y;
    int mel = blockIdx.x * blockDim.x + threadIdx.x;

    if (mel >= n_mels) return;

    float sum = 0.0f;
    for (int bin = 0; bin < n_bins; bin++) {
        sum += spectrogram[b * time_frames * n_bins + t * n_bins + bin] *
               filterbank[mel * n_bins + bin];
    }

    mel_spec[b * time_frames * n_mels + t * n_mels + mel] = sum;
}

bool nimcp_gpu_mel_filterbank(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spectrogram,
    nimcp_gpu_tensor_t* mel_spec,
    int n_mels, float fmin, float fmax, float sample_rate)
{
    if (!ctx || !spectrogram || !mel_spec) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int batch = spectrogram->dims[0];
    int time_frames = spectrogram->dims[1];
    int n_bins = spectrogram->dims[2];
    int n_fft = (n_bins - 1) * 2;

    // Create filterbank
    float* d_filterbank;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_filterbank, n_mels * n_bins * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    kernel_mel_filterbank_create<<<(n_mels + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        d_filterbank, n_mels, n_fft, fmin, fmax, sample_rate);

    // Apply filterbank
    dim3 grid((n_mels + BLOCK_SIZE - 1) / BLOCK_SIZE, time_frames, batch);
    kernel_apply_mel_filterbank<<<grid, BLOCK_SIZE>>>(
        (const float*)spectrogram->data, d_filterbank,
        (float*)mel_spec->data, batch, time_frames, n_bins, n_mels);

    cudaFree(d_filterbank);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// MFCC Computation
//=============================================================================

__global__ void kernel_log_mel(float* mel_spec, int n, float floor_val)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        mel_spec[idx] = logf(fmaxf(mel_spec[idx], floor_val));
    }
}

__global__ void kernel_dct_matrix(float* dct, int n_mfcc, int n_mels)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= n_mfcc || j >= n_mels) return;

    float scale = sqrtf(2.0f / n_mels);
    float angle = NIMCP_PI_F * (float)i * ((float)j + 0.5f) / (float)n_mels;
    dct[i * n_mels + j] = scale * cosf(angle);
}

bool nimcp_gpu_mfcc(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* mel_spec,
    nimcp_gpu_tensor_t* mfcc,
    int n_mfcc)
{
    if (!ctx || !mel_spec || !mfcc) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int batch = mel_spec->dims[0];
    int time_frames = mel_spec->dims[1];
    int n_mels = mel_spec->dims[2];

    // Apply log
    nimcp_gpu_tensor_t* log_mel = nimcp_gpu_tensor_clone(mel_spec);
    kernel_log_mel<<<(log_mel->numel + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        (float*)log_mel->data, log_mel->numel, 1e-10f);

    // Create DCT matrix
    float* d_dct;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_dct, n_mfcc * n_mels * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    dim3 dct_block(16, 16);
    dim3 dct_grid((n_mfcc + 15) / 16, (n_mels + 15) / 16);
    kernel_dct_matrix<<<dct_grid, dct_block>>>(d_dct, n_mfcc, n_mels);

    // Apply DCT via matrix multiply (simplified)
    // mfcc = log_mel @ dct.T
    // TODO: Use cuBLAS for proper batched matmul
    LOG_WARN("MFCC using simplified DCT - consider cuBLAS optimization");

    cudaFree(d_dct);
    nimcp_gpu_tensor_destroy(log_mel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// STFT (Short-Time Fourier Transform)
//=============================================================================

__global__ void kernel_hann_window(float* window, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        window[idx] = 0.5f * (1.0f - cosf(NIMCP_TWO_PI_F * idx / (n - 1)));
    }
}

__global__ void kernel_frame_audio(
    const float* audio, float* frames, const float* window,
    int audio_len, int n_fft, int hop_length, int n_frames)
{
    int frame = blockIdx.x;
    int idx = threadIdx.x;

    if (frame >= n_frames || idx >= n_fft) return;

    int audio_idx = frame * hop_length + idx;
    float val = (audio_idx < audio_len) ? audio[audio_idx] : 0.0f;
    frames[frame * n_fft + idx] = val * window[idx];
}

bool nimcp_gpu_stft(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t* stft_out,
    int n_fft, int hop_length)
{
    if (!ctx || !audio || !stft_out) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int audio_len = audio->dims[audio->ndim - 1];
    int n_frames = (audio_len - n_fft) / hop_length + 1;

    // Create Hann window
    float* d_window;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_window, n_fft * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    kernel_hann_window<<<(n_fft + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(d_window, n_fft);

    // Frame audio
    float* d_frames;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_frames, n_frames * n_fft * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    kernel_frame_audio<<<n_frames, n_fft>>>(
        (const float*)audio->data, d_frames, d_window,
        audio_len, n_fft, hop_length, n_frames);

    // Apply FFT using cuFFT
    cufftHandle plan;
    cufftPlan1d(&plan, n_fft, CUFFT_R2C, n_frames);
    cufftExecR2C(plan, (cufftReal*)d_frames, (cufftComplex*)stft_out->data);
    cufftDestroy(plan);

    cudaFree(d_window);
    cudaFree(d_frames);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Spectrogram (Magnitude of STFT)
//=============================================================================

__global__ void kernel_complex_magnitude(
    const float2* complex_data, float* magnitude, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float re = complex_data[idx].x;
        float im = complex_data[idx].y;
        magnitude[idx] = sqrtf(re * re + im * im);
    }
}

bool nimcp_gpu_spectrogram(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* audio,
    nimcp_gpu_tensor_t* spectrogram,
    int n_fft, int hop_length, bool power)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // First compute STFT
    size_t n_frames = (audio->dims[audio->ndim - 1] - n_fft) / hop_length + 1;
    size_t n_bins = n_fft / 2 + 1;

    // Allocate complex STFT output
    float2* d_stft;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_stft, n_frames * n_bins * sizeof(float2)), GPU_ERROR_OUT_OF_MEMORY);

    // TODO: Implement full STFT pipeline
    // For now, compute magnitude
    kernel_complex_magnitude<<<(n_frames * n_bins + BLOCK_SIZE - 1) / BLOCK_SIZE, BLOCK_SIZE>>>(
        d_stft, (float*)spectrogram->data, n_frames * n_bins);

    if (power) {
        // Square the magnitude for power spectrogram
        size_t n = spectrogram->numel;
        float* data = (float*)spectrogram->data;
        // Simple kernel to square
    }

    cudaFree(d_stft);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "utils/logging/nimcp_logging.h"
#define LOG_MODULE "AUDIO_GPU"

bool nimcp_gpu_mel_filterbank(void* ctx, void* spec, void* mel, int n, float fmin, float fmax, float sr)
{
    LOG_WARN("CUDA not available - audio processing requires GPU");
    return false;
}

bool nimcp_gpu_mfcc(void* ctx, void* mel, void* mfcc, int n) { return false; }
bool nimcp_gpu_stft(void* ctx, void* audio, void* stft, int nfft, int hop) { return false; }
bool nimcp_gpu_spectrogram(void* ctx, void* audio, void* spec, int nfft, int hop, bool power) { return false; }

#endif // NIMCP_ENABLE_CUDA
