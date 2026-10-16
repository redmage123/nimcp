/**
 * @file nimcp_financial_gpu_rng.cu
 * @brief GPU Random Number Generation for Financial Simulations
 *
 * WHAT: cuRAND wrapper for parallel RNG in Monte Carlo simulations
 * WHY:  High-quality, efficient random number generation on GPU
 * HOW:  cuRAND device API with XORWOW generator
 *
 * RNG ARCHITECTURE:
 * =================
 * This module provides financial-domain-specific RNG with cuRAND.
 * The pattern follows the central GPU statistics module:
 *   - See: gpu/statistics/nimcp_statistics_gpu.h for stats_gpu_rng_create/destroy
 *   - See: gpu/common/nimcp_device_utils.cuh for shared device RNG functions
 *
 * Domain-specific RNG is maintained here because:
 *   1. Financial simulations require precise seed control for reproducibility
 *   2. Correlation structures (e.g., Cholesky) are finance-specific
 *   3. Jump-diffusion and variance-gamma processes need custom sampling
 *
 * For general-purpose GPU statistics, prefer stats_gpu_rng_t from
 * nimcp_statistics_gpu.h.
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

#include "gpu/financial/nimcp_financial_gpu.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_rng_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_rng_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_rng_error, sizeof(g_rng_error), fmt, args);
    va_end(args);
}

//=============================================================================
// RNG State Structure
//=============================================================================

struct fin_gpu_rng_s {
    curandState* d_states;          /**< Device RNG states */
    uint32_t num_states;            /**< Number of parallel generators */
    uint64_t seed;                  /**< Initial seed */
    nimcp_gpu_context_t* ctx;       /**< Associated GPU context */
    bool initialized;               /**< Initialization flag */
};

//=============================================================================
// Initialization Kernel
//=============================================================================

/**
 * @brief Initialize cuRAND states
 */
static __global__ void kernel_init_rng_states(
    curandState* states,
    uint64_t seed,
    uint32_t n)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Initialize with unique sequence for each thread
    curand_init(seed, idx, 0, &states[idx]);
}

//=============================================================================
// Generation Kernels
//=============================================================================

/**
 * @brief Generate uniform random numbers in [0, 1)
 */
static __global__ void kernel_generate_uniform(
    curandState* states,
    float* output,
    uint32_t n,
    uint32_t num_states)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Use state for this thread (wrap around if more outputs than states)
    uint32_t state_idx = idx % num_states;

    output[idx] = curand_uniform(&states[state_idx]);
}

/**
 * @brief Generate standard normal random numbers (mean=0, std=1)
 */
__global__ void kernel_generate_normal(
    curandState* states,
    float* output,
    uint32_t n,
    uint32_t num_states)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    uint32_t state_idx = idx % num_states;

    output[idx] = curand_normal(&states[state_idx]);
}

/**
 * @brief Generate correlated normal random numbers using Cholesky decomposition
 *
 * @param states      RNG states
 * @param cholesky    Lower triangular Cholesky factor [n x n]
 * @param n           Number of correlated variables
 * @param num_sets    Number of random vector sets to generate
 * @param output      Output [num_sets x n]
 */
__global__ void kernel_generate_correlated_normal(
    curandState* states,
    const float* __restrict__ cholesky,
    uint32_t n,
    uint32_t num_sets,
    float* __restrict__ output)
{
    extern __shared__ float s_z[];  // Independent normals

    uint32_t set_idx = blockIdx.x;
    if (set_idx >= num_sets) return;

    // Generate n independent standard normals in shared memory
    for (uint32_t i = threadIdx.x; i < n; i += blockDim.x) {
        uint32_t state_idx = (set_idx * n + i) % (gridDim.x * blockDim.x);
        s_z[i] = curand_normal(&states[state_idx]);
    }
    __syncthreads();

    // Apply Cholesky transformation: x = L * z
    float* out = &output[set_idx * n];
    for (uint32_t i = threadIdx.x; i < n; i += blockDim.x) {
        float sum = 0.0f;
        for (uint32_t j = 0; j <= i; j++) {
            sum += cholesky[i * n + j] * s_z[j];
        }
        out[i] = sum;
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

fin_gpu_rng_t* fin_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n,
    uint64_t seed)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        set_rng_error("Invalid GPU context");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
        return NULL;
    }
    if (n == 0) {
        set_rng_error("Zero RNG states requested");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero RNG states requested");
        return NULL;
    }

    fin_gpu_rng_t* rng = (fin_gpu_rng_t*)nimcp_calloc(1, sizeof(fin_gpu_rng_t));
    if (!rng) {
        set_rng_error("Failed to allocate RNG structure");
        return NULL;
    }

    rng->ctx = ctx;
    rng->num_states = n;
    rng->seed = (seed == 0) ? (uint64_t)time(NULL) : seed;

    // Allocate device states with recovery
    size_t states_size = n * sizeof(curandState);
    cudaError_t err = cudaMalloc(&rng->d_states, states_size);
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&rng->d_states, states_size);
        }
        if (err != cudaSuccess) {
            set_rng_error("Failed to allocate RNG states: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "Failed to allocate RNG states: %s", cudaGetErrorString(err));
            nimcp_free(rng);
            return NULL;
        }
    }

    // Initialize states
    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        rng->d_states, rng->seed, n);

    err = cudaGetLastError();
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result = {0};
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_KERNEL_LAUNCH, err, &result)) {
            set_rng_error("RNG init kernel failed: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err, "RNG init kernel failed: %s", cudaGetErrorString(err));
            cudaFree(rng->d_states);
            nimcp_free(rng);
            return NULL;
        }
    }

    cudaStreamSynchronize(stream);

    rng->initialized = true;
    return rng;
}

void fin_gpu_rng_destroy(fin_gpu_rng_t* rng) {
    if (!rng) return;

    if (rng->d_states) {
        cudaFree(rng->d_states);
    }

    nimcp_free(rng);
}

bool fin_gpu_rng_reseed(fin_gpu_rng_t* rng, uint64_t seed) {
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!rng || !rng->initialized) {
        set_rng_error("Invalid RNG state");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid RNG state");
        return false;
    }

    rng->seed = (seed == 0) ? (uint64_t)time(NULL) : seed;

    cudaStream_t stream = nimcp_gpu_get_compute_stream(rng->ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(rng->num_states, block_size);

    kernel_init_rng_states<<<grid_size, block_size, 0, stream>>>(
        rng->d_states, rng->seed, rng->num_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

bool fin_gpu_rng_uniform(
    fin_gpu_rng_t* rng,
    float* output,
    uint32_t n)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!rng || !rng->initialized || !output) {
        set_rng_error("Invalid RNG or output pointer");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid RNG or output pointer");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(rng->ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);

    // Allocate device output (host pointer cannot be used in kernel)
    float* d_output = NULL;
    size_t out_size = n * sizeof(float);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_output, out_size), GPU_ERROR_OUT_OF_MEMORY);

    kernel_generate_uniform<<<grid_size, block_size, 0, stream>>>(
        rng->d_states, d_output, n, rng->num_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Copy results back to host
    NIMCP_CUDA_RECOVER(cudaMemcpy(output, d_output, out_size,
                                  cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    cudaFree(d_output);

    return true;
}

bool fin_gpu_rng_normal(
    fin_gpu_rng_t* rng,
    float* output,
    uint32_t n)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!rng || !rng->initialized || !output) {
        set_rng_error("Invalid RNG or output pointer");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid RNG or output pointer");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(rng->ctx);
    uint32_t block_size = 256;
    uint32_t grid_size = NIMCP_CUDA_GRID_SIZE(n, block_size);

    // Allocate device output (host pointer cannot be used in kernel)
    float* d_output = NULL;
    size_t out_size = n * sizeof(float);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_output, out_size), GPU_ERROR_OUT_OF_MEMORY);

    kernel_generate_normal<<<grid_size, block_size, 0, stream>>>(
        rng->d_states, d_output, n, rng->num_states);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Copy results back to host
    NIMCP_CUDA_RECOVER(cudaMemcpy(output, d_output, out_size,
                                  cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    cudaFree(d_output);

    return true;
}

bool fin_gpu_rng_correlated_normal(
    fin_gpu_rng_t* rng,
    const float* cholesky,
    uint32_t n,
    uint32_t num_sets,
    float* output)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!rng || !rng->initialized || !cholesky || !output) {
        set_rng_error("Invalid parameters");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid parameters");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(rng->ctx);
    uint32_t block_size = min(256u, n);
    size_t shared_size = n * sizeof(float);

    kernel_generate_correlated_normal<<<num_sets, block_size, shared_size, stream>>>(
        rng->d_states, cholesky, n, num_sets, output);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

const char* fin_gpu_get_last_error(void) {
    return g_rng_error;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

extern "C" {

fin_gpu_rng_t* fin_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n,
    uint64_t seed)
{
    (void)ctx; (void)n; (void)seed;
    return NULL;
}

void fin_gpu_rng_destroy(fin_gpu_rng_t* rng) {
    (void)rng;
}

bool fin_gpu_rng_reseed(fin_gpu_rng_t* rng, uint64_t seed) {
    (void)rng; (void)seed;
    return false;
}

bool fin_gpu_rng_uniform(fin_gpu_rng_t* rng, float* output, uint32_t n) {
    (void)rng; (void)output; (void)n;
    return false;
}

bool fin_gpu_rng_normal(fin_gpu_rng_t* rng, float* output, uint32_t n) {
    (void)rng; (void)output; (void)n;
    return false;
}

bool fin_gpu_rng_correlated_normal(
    fin_gpu_rng_t* rng,
    const float* cholesky,
    uint32_t n,
    uint32_t num_sets,
    float* output)
{
    (void)rng; (void)cholesky; (void)n; (void)num_sets; (void)output;
    return false;
}

const char* fin_gpu_get_last_error(void) {
    return "GPU support not compiled";
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
