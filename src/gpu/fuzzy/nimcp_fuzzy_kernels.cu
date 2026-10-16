/**
 * @file nimcp_fuzzy_kernels.cu
 * @brief CUDA Kernels for Membership Function Evaluation
 *
 * WHAT: Device functions and kernels for 14 MF types + 8 hedges
 * WHY:  GPU-accelerated batch MF evaluation for fuzzy inference
 * HOW:  One thread per (sample, MF) pair with shared memory optimization
 *
 * SUPPORTED MF TYPES:
 *   - Triangular, Trapezoidal, Gaussian, Double Gaussian
 *   - Bell, Sigmoid, Sigmoid Diff/Prod
 *   - Pi-shaped, S-shaped, Z-shaped
 *   - Singleton, Piecewise Linear
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdarg.h>

#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/fuzzy/nimcp_fuzzy_gpu_types.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/common/nimcp_device_utils.cuh"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_fuzzy_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_fuzzy_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_fuzzy_last_error, sizeof(g_fuzzy_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Device Functions: Membership Function Evaluation
//=============================================================================

/**
 * @brief Triangular MF: params[0]=a, params[1]=b, params[2]=c
 */
__device__ __forceinline__ float mf_triangular(float x, const float* params) {
    float a = params[0];
    float b = params[1];
    float c = params[2];

    if (x <= a || x >= c) return 0.0f;
    if (x <= b) return (x - a) / (b - a + NIMCP_EPS);
    return (c - x) / (c - b + NIMCP_EPS);
}

/**
 * @brief Trapezoidal MF: params[0]=a, params[1]=b, params[2]=c, params[3]=d
 */
__device__ __forceinline__ float mf_trapezoidal(float x, const float* params) {
    float a = params[0];
    float b = params[1];
    float c = params[2];
    float d = params[3];

    if (x <= a || x >= d) return 0.0f;
    if (x >= b && x <= c) return 1.0f;
    if (x < b) return (x - a) / (b - a + NIMCP_EPS);
    return (d - x) / (d - c + NIMCP_EPS);
}

/**
 * @brief Gaussian MF: params[0]=mean, params[1]=sigma
 */
__device__ __forceinline__ float mf_gaussian(float x, const float* params) {
    float mean = params[0];
    float sigma = params[1];
    float normalized = (x - mean) / (sigma + NIMCP_EPS);
    return expf(-0.5f * normalized * normalized);
}

/**
 * @brief Double Gaussian (asymmetric): params[0-1]=left gaussian, params[2-3]=right
 */
__device__ __forceinline__ float mf_gaussian_double(float x, const float* params) {
    float mean1 = params[0];
    float sigma1 = params[1];
    float mean2 = params[2];
    float sigma2 = params[3];

    // Use left Gaussian for x < midpoint, right for x >= midpoint
    float midpoint = (mean1 + mean2) * 0.5f;
    if (x <= midpoint) {
        float n = (x - mean1) / (sigma1 + NIMCP_EPS);
        return expf(-0.5f * n * n);
    } else {
        float n = (x - mean2) / (sigma2 + NIMCP_EPS);
        return expf(-0.5f * n * n);
    }
}

/**
 * @brief Generalized Bell MF: params[0]=width, params[1]=slope, params[2]=center
 */
__device__ __forceinline__ float mf_bell(float x, const float* params) {
    float a = params[0];  // width
    float b = params[1];  // slope
    float c = params[2];  // center

    float base = fabsf((x - c) / (a + NIMCP_EPS));
    return 1.0f / (1.0f + powf(base, 2.0f * b));
}

/**
 * @brief Sigmoid MF: params[0]=slope, params[1]=center
 */
__device__ __forceinline__ float mf_sigmoid(float x, const float* params) {
    float a = params[0];  // slope
    float c = params[1];  // center
    return 1.0f / (1.0f + expf(-a * (x - c)));
}

/**
 * @brief Difference of two sigmoids: params[0-1]=first, params[2-3]=second
 */
__device__ __forceinline__ float mf_sigmoid_diff(float x, const float* params) {
    float s1 = 1.0f / (1.0f + expf(-params[0] * (x - params[1])));
    float s2 = 1.0f / (1.0f + expf(-params[2] * (x - params[3])));
    return fabsf(s1 - s2);
}

/**
 * @brief Product of two sigmoids: params[0-1]=first, params[2-3]=second
 */
__device__ __forceinline__ float mf_sigmoid_prod(float x, const float* params) {
    float s1 = 1.0f / (1.0f + expf(-params[0] * (x - params[1])));
    float s2 = 1.0f / (1.0f + expf(-params[2] * (x - params[3])));
    return s1 * s2;
}

/**
 * @brief S-shaped MF: params[0]=foot, params[1]=shoulder
 */
__device__ __forceinline__ float mf_s_shaped(float x, const float* params) {
    float a = params[0];  // foot
    float b = params[1];  // shoulder

    if (x <= a) return 0.0f;
    if (x >= b) return 1.0f;

    float mid = (a + b) * 0.5f;
    if (x <= mid) {
        float t = (x - a) / (b - a + NIMCP_EPS);
        return 2.0f * t * t;
    } else {
        float t = (b - x) / (b - a + NIMCP_EPS);
        return 1.0f - 2.0f * t * t;
    }
}

/**
 * @brief Z-shaped MF: params[0]=shoulder, params[1]=foot
 */
__device__ __forceinline__ float mf_z_shaped(float x, const float* params) {
    float a = params[0];  // shoulder
    float b = params[1];  // foot

    if (x <= a) return 1.0f;
    if (x >= b) return 0.0f;

    float mid = (a + b) * 0.5f;
    if (x <= mid) {
        float t = (x - a) / (b - a + NIMCP_EPS);
        return 1.0f - 2.0f * t * t;
    } else {
        float t = (b - x) / (b - a + NIMCP_EPS);
        return 2.0f * t * t;
    }
}

/**
 * @brief Pi-shaped MF: combination of S and Z
 */
__device__ __forceinline__ float mf_pi_shaped(float x, const float* params) {
    float a = params[0];
    float b = params[1];
    float c = params[2];
    float d = params[3];

    // S-shaped from a to b, flat from b to c, Z-shaped from c to d
    float s_params[2] = {a, b};
    float z_params[2] = {c, d};

    return fminf(mf_s_shaped(x, s_params), mf_z_shaped(x, z_params));
}

/**
 * @brief Singleton MF: params[0]=x0
 */
__device__ __forceinline__ float mf_singleton(float x, const float* params) {
    float x0 = params[0];
    return (fabsf(x - x0) < NIMCP_EPS) ? 1.0f : 0.0f;
}

/**
 * @brief Piecewise linear MF (up to 4 points): params[0,1]=x0,y0, params[2,3]=x1,y1, etc.
 */
__device__ __forceinline__ float mf_piecewise_linear(float x, const float* params, uint32_t num_params) {
    uint32_t num_points = num_params / 2;
    if (num_points < 2) return 0.0f;

    // Find segment
    for (uint32_t i = 0; i < num_points - 1; i++) {
        float x0 = params[i * 2];
        float y0 = params[i * 2 + 1];
        float x1 = params[(i + 1) * 2];
        float y1 = params[(i + 1) * 2 + 1];

        if (x >= x0 && x <= x1) {
            float t = (x - x0) / (x1 - x0 + NIMCP_EPS);
            return y0 + t * (y1 - y0);
        }
    }

    // Outside range
    if (x < params[0]) return params[1];
    return params[(num_points - 1) * 2 + 1];
}

//=============================================================================
// Device Function: MF Dispatcher
//=============================================================================

/**
 * @brief Evaluate any MF type based on type enum
 */
__device__ float mf_evaluate_device(float x, uint32_t type, const float* params, uint32_t num_params) {
    switch (type) {
        case 0:  // FUZZY_MF_TRIANGULAR
            return mf_triangular(x, params);
        case 1:  // FUZZY_MF_TRAPEZOIDAL
            return mf_trapezoidal(x, params);
        case 2:  // FUZZY_MF_GAUSSIAN
            return mf_gaussian(x, params);
        case 3:  // FUZZY_MF_GAUSSIAN_DOUBLE
            return mf_gaussian_double(x, params);
        case 4:  // FUZZY_MF_BELL
            return mf_bell(x, params);
        case 5:  // FUZZY_MF_SIGMOID
            return mf_sigmoid(x, params);
        case 6:  // FUZZY_MF_SIGMOID_DIFF
            return mf_sigmoid_diff(x, params);
        case 7:  // FUZZY_MF_SIGMOID_PROD
            return mf_sigmoid_prod(x, params);
        case 8:  // FUZZY_MF_PI_SHAPED
            return mf_pi_shaped(x, params);
        case 9:  // FUZZY_MF_S_SHAPED
            return mf_s_shaped(x, params);
        case 10: // FUZZY_MF_Z_SHAPED
            return mf_z_shaped(x, params);
        case 11: // FUZZY_MF_SINGLETON
            return mf_singleton(x, params);
        case 12: // FUZZY_MF_PIECEWISE_LINEAR
            return mf_piecewise_linear(x, params, num_params);
        default:
            return 0.0f;
    }
}

//=============================================================================
// Device Functions: Hedge Application
//=============================================================================

/**
 * @brief Apply linguistic hedge to membership value
 * Note: Not __forceinline__ so it can be linked from other translation units
 */
__device__ float apply_hedge_device(float mu, uint32_t hedge) {
    switch (hedge) {
        case 0:  // FUZZY_HEDGE_NONE
            return mu;
        case 1:  // FUZZY_HEDGE_VERY (concentration)
            return mu * mu;
        case 2:  // FUZZY_HEDGE_SOMEWHAT (dilation)
            return sqrtf(mu);
        case 3:  // FUZZY_HEDGE_EXTREMELY
            return mu * mu * mu;
        case 4:  // FUZZY_HEDGE_SLIGHTLY (intensification around 0.5)
            return (mu <= 0.5f) ? sqrtf(mu * 0.5f) : (1.0f - sqrtf(0.5f * (1.0f - mu)));
        case 5:  // FUZZY_HEDGE_NOT
            return 1.0f - mu;
        case 6:  // FUZZY_HEDGE_MORE_OR_LESS
            return powf(mu, 0.75f);
        case 7:  // FUZZY_HEDGE_INDEED (intensification)
            return (mu <= 0.5f) ? (2.0f * mu * mu) : (1.0f - 2.0f * (1.0f - mu) * (1.0f - mu));
        default:
            return mu;
    }
}

//=============================================================================
// CUDA Kernels
//=============================================================================

/**
 * @brief Batch MF evaluation kernel
 *
 * Each thread evaluates one (sample, MF) pair.
 * Grid: (num_samples * num_mfs + blockDim.x - 1) / blockDim.x
 * Block: 256 threads
 *
 * @param inputs       Input values [num_samples]
 * @param mf_types     MF types [num_mfs]
 * @param mf_hedges    Hedges [num_mfs]
 * @param mf_params    Flattened params [num_mfs * FUZZY_GPU_MAX_PARAMS]
 * @param mf_num_params Param counts [num_mfs]
 * @param mf_alpha_cuts Alpha cuts [num_mfs]
 * @param memberships  Output [num_samples * num_mfs]
 * @param num_samples  Number of input samples
 * @param num_mfs      Number of MFs
 * @param apply_hedges Whether to apply hedges
 * @param apply_alpha  Whether to apply alpha cuts
 */
__global__ void kernel_mf_evaluate_batch(
    const float* __restrict__ inputs,
    const uint32_t* __restrict__ mf_types,
    const uint32_t* __restrict__ mf_hedges,
    const float* __restrict__ mf_params,
    const uint32_t* __restrict__ mf_num_params,
    const float* __restrict__ mf_alpha_cuts,
    float* __restrict__ memberships,
    uint32_t num_samples,
    uint32_t num_mfs,
    bool apply_hedges,
    bool apply_alpha)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = num_samples * num_mfs;

    if (idx >= total) return;

    uint32_t sample_idx = idx / num_mfs;
    uint32_t mf_idx = idx % num_mfs;

    float x = inputs[sample_idx];
    uint32_t type = mf_types[mf_idx];
    uint32_t num_p = mf_num_params[mf_idx];
    const float* params = &mf_params[mf_idx * FUZZY_GPU_MAX_PARAMS];

    // Evaluate MF
    float mu = mf_evaluate_device(x, type, params, num_p);

    // Apply hedge if enabled
    if (apply_hedges) {
        uint32_t hedge = mf_hedges[mf_idx];
        mu = apply_hedge_device(mu, hedge);
    }

    // Apply alpha-cut if enabled
    if (apply_alpha) {
        float alpha = mf_alpha_cuts[mf_idx];
        if (mu < alpha) mu = 0.0f;
    }

    // Clamp to [0, 1]
    memberships[idx] = nimcp_device_saturate(mu);
}

/**
 * @brief Discretization kernel: sample MF at uniform points
 *
 * @param mf_types      MF types [num_mfs]
 * @param mf_hedges     Hedges [num_mfs]
 * @param mf_params     Params [num_mfs * MAX_PARAMS]
 * @param mf_num_params Param counts [num_mfs]
 * @param discretized   Output [num_mfs * resolution]
 * @param num_mfs       Number of MFs
 * @param resolution    Discretization resolution
 * @param x_min         Universe start
 * @param x_max         Universe end
 */
__global__ void kernel_mf_discretize(
    const uint32_t* __restrict__ mf_types,
    const uint32_t* __restrict__ mf_hedges,
    const float* __restrict__ mf_params,
    const uint32_t* __restrict__ mf_num_params,
    float* __restrict__ discretized,
    uint32_t num_mfs,
    uint32_t resolution,
    float x_min,
    float x_max)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = num_mfs * resolution;

    if (idx >= total) return;

    uint32_t mf_idx = idx / resolution;
    uint32_t point_idx = idx % resolution;

    // Compute x value for this point
    float x = x_min + (x_max - x_min) * (float)point_idx / (float)(resolution - 1);

    uint32_t type = mf_types[mf_idx];
    uint32_t num_p = mf_num_params[mf_idx];
    uint32_t hedge = mf_hedges[mf_idx];
    const float* params = &mf_params[mf_idx * FUZZY_GPU_MAX_PARAMS];

    float mu = mf_evaluate_device(x, type, params, num_p);
    mu = apply_hedge_device(mu, hedge);

    discretized[idx] = nimcp_device_saturate(mu);
}

/**
 * @brief Fuzzification kernel: evaluate variable terms
 *
 * @param inputs       Crisp inputs [num_samples * num_vars]
 * @param variables    Variable definitions [num_vars]
 * @param fuzzified    Output memberships [num_samples * num_vars * max_terms]
 * @param num_samples  Number of samples
 * @param num_vars     Number of input variables
 * @param max_terms    Maximum terms per variable
 */
__global__ void kernel_fuzzify_batch(
    const float* __restrict__ inputs,
    const fuzzy_gpu_variable_t* __restrict__ variables,
    float* __restrict__ fuzzified,
    uint32_t num_samples,
    uint32_t num_vars,
    uint32_t max_terms)
{
    uint32_t sample_idx = blockIdx.x;
    uint32_t var_idx = blockIdx.y;
    uint32_t term_idx = threadIdx.x;

    if (sample_idx >= num_samples || var_idx >= num_vars) return;

    const fuzzy_gpu_variable_t* var = &variables[var_idx];
    if (term_idx >= var->num_terms) {
        // Zero out unused terms
        fuzzified[sample_idx * num_vars * max_terms + var_idx * max_terms + term_idx] = 0.0f;
        return;
    }

    float x = inputs[sample_idx * num_vars + var_idx];
    const fuzzy_gpu_mf_t* mf = &var->terms[term_idx];

    float mu = mf_evaluate_device(x, mf->type, mf->params, mf->num_params);
    mu = apply_hedge_device(mu, mf->hedge);

    if (mu < mf->alpha_cut) mu = 0.0f;

    fuzzified[sample_idx * num_vars * max_terms + var_idx * max_terms + term_idx] =
        nimcp_device_saturate(mu);
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

const char* nimcp_gpu_fuzzy_get_last_error(void) {
    return g_fuzzy_last_error;
}

bool nimcp_gpu_fuzzy_mf_evaluate_batch(
    nimcp_gpu_context_t* ctx,
    const float* inputs,
    const fuzzy_gpu_mf_t* mfs,
    uint32_t num_mfs,
    float* memberships,
    const nimcp_gpu_mf_eval_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_fuzzy_error("Invalid GPU context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
            return false;
        }
    }
    if (!inputs || !mfs || !memberships) {
        set_fuzzy_error("NULL input/output pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "NULL input/output pointers");
        return false;
    }
    if (num_mfs == 0 || params->num_samples == 0) {
        // Try parameter correction
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result)) {
            set_fuzzy_error("Zero samples or MFs");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero samples or MFs");
            return false;
        }
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Pre-declare variables used after potential goto (CUDA C++ requirement)
    uint32_t total_elements = 0;
    uint32_t block_size = 0;
    uint32_t grid_size = 0;

    // Allocate device memory for MF definitions
    uint32_t* d_types = NULL;
    uint32_t* d_hedges = NULL;
    float* d_params = NULL;
    uint32_t* d_num_params = NULL;
    float* d_alpha_cuts = NULL;

    // Device memory for inputs and outputs (host pointers cannot be used in kernels)
    float* d_inputs = NULL;
    float* d_memberships = NULL;
    size_t inputs_size = params->num_samples * sizeof(float);
    size_t memberships_size = params->num_samples * num_mfs * sizeof(float);

    size_t types_size = num_mfs * sizeof(uint32_t);
    size_t params_size = num_mfs * FUZZY_GPU_MAX_PARAMS * sizeof(float);

    // Use recovery-aware allocation macros
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_types, types_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_hedges, types_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_params, params_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_num_params, types_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_alpha_cuts, num_mfs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_inputs, inputs_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_memberships, memberships_size), GPU_ERROR_OUT_OF_MEMORY);

    // Prepare host arrays
    uint32_t* h_types = (uint32_t*)nimcp_malloc(types_size);
    uint32_t* h_hedges = (uint32_t*)nimcp_malloc(types_size);
    float* h_params = (float*)nimcp_malloc(params_size);
    uint32_t* h_num_params = (uint32_t*)nimcp_malloc(types_size);
    float* h_alpha_cuts = (float*)nimcp_malloc(num_mfs * sizeof(float));

    if (!h_types || !h_hedges || !h_params || !h_num_params || !h_alpha_cuts) {
        set_fuzzy_error("Host memory allocation failed");
        goto cleanup_error;
    }

    // Copy MF data to flat arrays
    for (uint32_t i = 0; i < num_mfs; i++) {
        h_types[i] = mfs[i].type;
        h_hedges[i] = mfs[i].hedge;
        h_num_params[i] = mfs[i].num_params;
        h_alpha_cuts[i] = mfs[i].alpha_cut;
        memcpy(&h_params[i * FUZZY_GPU_MAX_PARAMS], mfs[i].params,
               FUZZY_GPU_MAX_PARAMS * sizeof(float));
    }

    // Upload to device (with recovery)
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_types, h_types, types_size,
                                      cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_hedges, h_hedges, types_size,
                                      cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_params, h_params, params_size,
                                      cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_num_params, h_num_params, types_size,
                                      cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_alpha_cuts, h_alpha_cuts,
                                      num_mfs * sizeof(float),
                                      cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    // Upload inputs to device
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_inputs, inputs, inputs_size,
                                      cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Launch kernel
    total_elements = params->num_samples * num_mfs;
    block_size = FUZZY_GPU_BLOCK_SIZE;
    grid_size = NIMCP_CUDA_GRID_SIZE(total_elements, block_size);

    kernel_mf_evaluate_batch<<<grid_size, block_size, 0, stream>>>(
        d_inputs, d_types, d_hedges, d_params, d_num_params, d_alpha_cuts,
        d_memberships, params->num_samples, num_mfs,
        params->apply_hedges, params->apply_alpha_cuts);

    // Check kernel launch with recovery
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Synchronize
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Copy results back to host
    NIMCP_CUDA_RECOVER(cudaMemcpy(memberships, d_memberships, memberships_size,
                                  cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    // Cleanup
    nimcp_free(h_types);
    nimcp_free(h_hedges);
    nimcp_free(h_params);
    nimcp_free(h_num_params);
    nimcp_free(h_alpha_cuts);
    cudaFree(d_types);
    cudaFree(d_hedges);
    cudaFree(d_params);
    cudaFree(d_num_params);
    cudaFree(d_alpha_cuts);
    cudaFree(d_inputs);
    cudaFree(d_memberships);

    return true;

cleanup_error:
    if (h_types) nimcp_free(h_types);
    if (h_hedges) nimcp_free(h_hedges);
    if (h_params) nimcp_free(h_params);
    if (h_num_params) nimcp_free(h_num_params);
    if (h_alpha_cuts) nimcp_free(h_alpha_cuts);
    if (d_types) cudaFree(d_types);
    if (d_hedges) cudaFree(d_hedges);
    if (d_params) cudaFree(d_params);
    if (d_num_params) cudaFree(d_num_params);
    if (d_alpha_cuts) cudaFree(d_alpha_cuts);
    if (d_inputs) cudaFree(d_inputs);
    if (d_memberships) cudaFree(d_memberships);
    return false;
}

bool nimcp_gpu_fuzzy_mf_discretize_batch(
    nimcp_gpu_context_t* ctx,
    const fuzzy_gpu_mf_t* mfs,
    uint32_t num_mfs,
    float* discretized,
    const nimcp_gpu_discretize_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_fuzzy_error("Invalid GPU context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
            return false;
        }
    }
    if (!mfs || !discretized) {
        set_fuzzy_error("NULL input/output pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "NULL input/output pointers");
        return false;
    }
    if (num_mfs == 0 || params->resolution == 0) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result)) {
            set_fuzzy_error("Zero MFs or resolution");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero MFs or resolution");
            return false;
        }
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Pre-declare variables used after potential goto (CUDA C++ requirement)
    uint32_t total = 0;
    uint32_t block_size = 0;
    uint32_t grid_size = 0;

    // Allocate device memory
    uint32_t* d_types = NULL;
    uint32_t* d_hedges = NULL;
    float* d_params = NULL;
    uint32_t* d_num_params = NULL;

    // Device memory for output (host pointer cannot be used in kernels)
    float* d_discretized = NULL;
    size_t disc_size = num_mfs * params->resolution * sizeof(float);

    size_t types_size = num_mfs * sizeof(uint32_t);
    size_t params_size = num_mfs * FUZZY_GPU_MAX_PARAMS * sizeof(float);

    NIMCP_CUDA_RECOVER(cudaMalloc(&d_types, types_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_hedges, types_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_params, params_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_num_params, types_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_discretized, disc_size), GPU_ERROR_OUT_OF_MEMORY);

    // Prepare and upload (same pattern as evaluate)
    uint32_t* h_types = (uint32_t*)nimcp_malloc(types_size);
    uint32_t* h_hedges = (uint32_t*)nimcp_malloc(types_size);
    float* h_params = (float*)nimcp_malloc(params_size);
    uint32_t* h_num_params = (uint32_t*)nimcp_malloc(types_size);

    if (!h_types || !h_hedges || !h_params || !h_num_params) {
        set_fuzzy_error("Host memory allocation failed");
        goto cleanup_discretize;
    }

    for (uint32_t i = 0; i < num_mfs; i++) {
        h_types[i] = mfs[i].type;
        h_hedges[i] = params->apply_hedge ? params->hedge : mfs[i].hedge;
        h_num_params[i] = mfs[i].num_params;
        memcpy(&h_params[i * FUZZY_GPU_MAX_PARAMS], mfs[i].params,
               FUZZY_GPU_MAX_PARAMS * sizeof(float));
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_types, h_types, types_size,
                                       cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_hedges, h_hedges, types_size,
                                       cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_params, h_params, params_size,
                                       cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_num_params, h_num_params, types_size,
                                       cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Launch kernel
    total = num_mfs * params->resolution;
    block_size = FUZZY_GPU_BLOCK_SIZE;
    grid_size = NIMCP_CUDA_GRID_SIZE(total, block_size);

    kernel_mf_discretize<<<grid_size, block_size, 0, stream>>>(
        d_types, d_hedges, d_params, d_num_params,
        d_discretized, num_mfs, params->resolution,
        params->x_min, params->x_max);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Copy results back to host
    NIMCP_CUDA_RECOVER(cudaMemcpy(discretized, d_discretized, disc_size,
                                  cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    nimcp_free(h_types);
    nimcp_free(h_hedges);
    nimcp_free(h_params);
    nimcp_free(h_num_params);
    cudaFree(d_types);
    cudaFree(d_hedges);
    cudaFree(d_params);
    cudaFree(d_num_params);
    cudaFree(d_discretized);

    return true;

cleanup_discretize:
    if (h_types) nimcp_free(h_types);
    if (h_hedges) nimcp_free(h_hedges);
    if (h_params) nimcp_free(h_params);
    if (h_num_params) nimcp_free(h_num_params);
    if (d_types) cudaFree(d_types);
    if (d_hedges) cudaFree(d_hedges);
    if (d_params) cudaFree(d_params);
    if (d_num_params) cudaFree(d_num_params);
    if (d_discretized) cudaFree(d_discretized);
    return false;
}

fuzzy_gpu_mf_t nimcp_gpu_fuzzy_mf_from_cpu(
    const fuzzy_mf_t* cpu_mf,
    fuzzy_hedge_t hedge,
    float alpha_cut)
{
    fuzzy_gpu_mf_t gpu_mf = {0};

    if (!cpu_mf) return gpu_mf;

    gpu_mf.type = (uint32_t)cpu_mf->type;
    gpu_mf.hedge = (uint32_t)hedge;
    gpu_mf.num_params = cpu_mf->num_params;
    gpu_mf.alpha_cut = alpha_cut;

    for (uint32_t i = 0; i < FUZZY_GPU_MAX_PARAMS && i < cpu_mf->num_params; i++) {
        gpu_mf.params[i] = cpu_mf->params[i];
    }

    return gpu_mf;
}

int nimcp_gpu_fuzzy_variable_from_cpu(
    const fuzzy_variable_t* cpu_var,
    fuzzy_gpu_variable_t* out_var)
{
    if (!cpu_var || !out_var) return FUZZY_GPU_ERR_NULL_INPUT;

    out_var->universe_min = cpu_var->universe_min;
    out_var->universe_max = cpu_var->universe_max;
    out_var->num_terms = cpu_var->num_terms;

    for (uint32_t i = 0; i < cpu_var->num_terms && i < FUZZY_GPU_MAX_TERMS; i++) {
        out_var->terms[i] = nimcp_gpu_fuzzy_mf_from_cpu(
            &cpu_var->terms[i].mf,
            cpu_var->terms[i].hedge,
            cpu_var->terms[i].alpha_cut);
    }

    return FUZZY_GPU_ERR_OK;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

// CPU fallback stubs
extern "C" {

const char* nimcp_gpu_fuzzy_get_last_error(void) {
    return "GPU support not compiled";
}

bool nimcp_gpu_fuzzy_mf_evaluate_batch(
    nimcp_gpu_context_t* ctx,
    const float* inputs,
    const fuzzy_gpu_mf_t* mfs,
    uint32_t num_mfs,
    float* memberships,
    const nimcp_gpu_mf_eval_params_t* params)
{
    (void)ctx; (void)inputs; (void)mfs; (void)num_mfs;
    (void)memberships; (void)params;
    return false;
}

bool nimcp_gpu_fuzzy_mf_discretize_batch(
    nimcp_gpu_context_t* ctx,
    const fuzzy_gpu_mf_t* mfs,
    uint32_t num_mfs,
    float* discretized,
    const nimcp_gpu_discretize_params_t* params)
{
    (void)ctx; (void)mfs; (void)num_mfs; (void)discretized; (void)params;
    return false;
}

fuzzy_gpu_mf_t nimcp_gpu_fuzzy_mf_from_cpu(
    const fuzzy_mf_t* cpu_mf,
    fuzzy_hedge_t hedge,
    float alpha_cut)
{
    fuzzy_gpu_mf_t gpu_mf = {0};
    (void)cpu_mf; (void)hedge; (void)alpha_cut;
    return gpu_mf;
}

int nimcp_gpu_fuzzy_variable_from_cpu(
    const fuzzy_variable_t* cpu_var,
    fuzzy_gpu_variable_t* out_var)
{
    (void)cpu_var; (void)out_var;
    return FUZZY_GPU_ERR_CUDA;
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
