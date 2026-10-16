/**
 * @file nimcp_fuzzy_defuzz_kernels.cu
 * @brief CUDA Kernels for Defuzzification
 *
 * WHAT: 7 defuzzification methods with parallel reduction
 * WHY:  GPU-accelerated conversion from fuzzy sets to crisp values
 * HOW:  Parallel reduction for centroid, bisector; parallel search for MOM/SOM/LOM
 *
 * METHODS:
 *   - Centroid (Center of Gravity)
 *   - Bisector of Area
 *   - Mean of Maximum (MOM)
 *   - Smallest of Maximum (SOM)
 *   - Largest of Maximum (LOM)
 *   - Weighted Average
 *   - Weighted Sum
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

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
// Shared Memory Reduction Helpers
//=============================================================================

/**
 * @brief Warp-level sum reduction using shuffle
 */
__device__ __forceinline__ float warp_reduce_sum(float val) {
    for (int offset = NIMCP_CUDA_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level max reduction using shuffle
 */
__device__ __forceinline__ float warp_reduce_max(float val) {
    for (int offset = NIMCP_CUDA_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fmaxf(val, __shfl_down_sync(0xffffffff, val, offset));
    }
    return val;
}

/**
 * @brief Warp-level min index reduction
 * Returns index of minimum value in warp
 */
__device__ __forceinline__ void warp_reduce_min_idx(float* val, int* idx) {
    for (int offset = NIMCP_CUDA_WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other_val = __shfl_down_sync(0xffffffff, *val, offset);
        int other_idx = __shfl_down_sync(0xffffffff, *idx, offset);
        if (other_val < *val) {
            *val = other_val;
            *idx = other_idx;
        }
    }
}

/**
 * @brief Warp-level max index reduction
 */
__device__ __forceinline__ void warp_reduce_max_idx(float* val, int* idx) {
    for (int offset = NIMCP_CUDA_WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other_val = __shfl_down_sync(0xffffffff, *val, offset);
        int other_idx = __shfl_down_sync(0xffffffff, *idx, offset);
        if (other_val > *val) {
            *val = other_val;
            *idx = other_idx;
        }
    }
}

//=============================================================================
// Defuzzification Kernels
//=============================================================================

/**
 * @brief Centroid defuzzification kernel
 *
 * Computes: sum(x * mu(x)) / sum(mu(x))
 * Uses parallel reduction within each block.
 *
 * One block per sample, threads cooperate on reduction.
 */
__global__ void kernel_defuzz_centroid(
    const float* __restrict__ aggregated,  // [num_samples x resolution]
    float* __restrict__ outputs,           // [num_samples]
    float x_min,
    float x_max,
    uint32_t resolution,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];
    float* s_numerator = sdata;
    float* s_denominator = &sdata[blockDim.x];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_data = &aggregated[sample_idx * resolution];
    float dx = (x_max - x_min) / (float)(resolution - 1);

    // Each thread handles multiple points
    float local_num = 0.0f;
    float local_den = 0.0f;

    for (uint32_t i = threadIdx.x; i < resolution; i += blockDim.x) {
        float x = x_min + dx * i;
        float mu = sample_data[i];
        local_num += x * mu;
        local_den += mu;
    }

    s_numerator[threadIdx.x] = local_num;
    s_denominator[threadIdx.x] = local_den;
    __syncthreads();

    // Block reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            s_numerator[threadIdx.x] += s_numerator[threadIdx.x + s];
            s_denominator[threadIdx.x] += s_denominator[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        float den = s_denominator[0];
        if (den > NIMCP_EPS) {
            outputs[sample_idx] = s_numerator[0] / den;
        } else {
            outputs[sample_idx] = (x_min + x_max) * 0.5f;  // Default to midpoint
        }
    }
}

/**
 * @brief Bisector defuzzification kernel
 *
 * Finds x where area to left equals area to right.
 */
__global__ void kernel_defuzz_bisector(
    const float* __restrict__ aggregated,
    float* __restrict__ outputs,
    float x_min,
    float x_max,
    uint32_t resolution,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_data = &aggregated[sample_idx * resolution];
    float dx = (x_max - x_min) / (float)(resolution - 1);

    // First pass: compute total area
    float local_sum = 0.0f;
    for (uint32_t i = threadIdx.x; i < resolution; i += blockDim.x) {
        local_sum += sample_data[i];
    }

    sdata[threadIdx.x] = local_sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            sdata[threadIdx.x] += sdata[threadIdx.x + s];
        }
        __syncthreads();
    }

    float total_area = sdata[0];
    float half_area = total_area * 0.5f;

    __syncthreads();

    // Thread 0 finds bisector via scan
    if (threadIdx.x == 0) {
        float cumsum = 0.0f;
        uint32_t bisector_idx = 0;

        for (uint32_t i = 0; i < resolution; i++) {
            cumsum += sample_data[i];
            if (cumsum >= half_area) {
                bisector_idx = i;
                break;
            }
        }

        outputs[sample_idx] = x_min + dx * bisector_idx;
    }
}

/**
 * @brief Mean of Maximum (MOM) defuzzification kernel
 *
 * Finds mean of all x where mu(x) is maximum.
 */
__global__ void kernel_defuzz_mom(
    const float* __restrict__ aggregated,
    float* __restrict__ outputs,
    float x_min,
    float x_max,
    uint32_t resolution,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];
    float* s_max = sdata;
    float* s_sum = &sdata[blockDim.x];
    int* s_count = (int*)&sdata[2 * blockDim.x];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_data = &aggregated[sample_idx * resolution];
    float dx = (x_max - x_min) / (float)(resolution - 1);

    // First: find maximum value
    float local_max = -1e30f;
    for (uint32_t i = threadIdx.x; i < resolution; i += blockDim.x) {
        local_max = fmaxf(local_max, sample_data[i]);
    }

    s_max[threadIdx.x] = local_max;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            s_max[threadIdx.x] = fmaxf(s_max[threadIdx.x], s_max[threadIdx.x + s]);
        }
        __syncthreads();
    }

    float max_val = s_max[0];
    __syncthreads();

    // Second: find mean of positions with maximum value
    float local_sum = 0.0f;
    int local_count = 0;
    float tolerance = max_val * 0.001f + NIMCP_EPS;

    for (uint32_t i = threadIdx.x; i < resolution; i += blockDim.x) {
        if (fabsf(sample_data[i] - max_val) < tolerance) {
            float x = x_min + dx * i;
            local_sum += x;
            local_count++;
        }
    }

    s_sum[threadIdx.x] = local_sum;
    s_count[threadIdx.x] = local_count;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            s_sum[threadIdx.x] += s_sum[threadIdx.x + s];
            s_count[threadIdx.x] += s_count[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        if (s_count[0] > 0) {
            outputs[sample_idx] = s_sum[0] / (float)s_count[0];
        } else {
            outputs[sample_idx] = (x_min + x_max) * 0.5f;
        }
    }
}

/**
 * @brief Smallest of Maximum (SOM) defuzzification kernel
 */
__global__ void kernel_defuzz_som(
    const float* __restrict__ aggregated,
    float* __restrict__ outputs,
    float x_min,
    float x_max,
    uint32_t resolution,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_data = &aggregated[sample_idx * resolution];
    float dx = (x_max - x_min) / (float)(resolution - 1);

    // Find maximum
    float local_max = -1e30f;
    for (uint32_t i = threadIdx.x; i < resolution; i += blockDim.x) {
        local_max = fmaxf(local_max, sample_data[i]);
    }

    sdata[threadIdx.x] = local_max;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            sdata[threadIdx.x] = fmaxf(sdata[threadIdx.x], sdata[threadIdx.x + s]);
        }
        __syncthreads();
    }

    float max_val = sdata[0];
    __syncthreads();

    // Thread 0 finds smallest index with max value
    if (threadIdx.x == 0) {
        float tolerance = max_val * 0.001f + NIMCP_EPS;
        for (uint32_t i = 0; i < resolution; i++) {
            if (fabsf(sample_data[i] - max_val) < tolerance) {
                outputs[sample_idx] = x_min + dx * i;
                return;
            }
        }
        outputs[sample_idx] = x_min;
    }
}

/**
 * @brief Largest of Maximum (LOM) defuzzification kernel
 */
__global__ void kernel_defuzz_lom(
    const float* __restrict__ aggregated,
    float* __restrict__ outputs,
    float x_min,
    float x_max,
    uint32_t resolution,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_data = &aggregated[sample_idx * resolution];
    float dx = (x_max - x_min) / (float)(resolution - 1);

    // Find maximum
    float local_max = -1e30f;
    for (uint32_t i = threadIdx.x; i < resolution; i += blockDim.x) {
        local_max = fmaxf(local_max, sample_data[i]);
    }

    sdata[threadIdx.x] = local_max;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            sdata[threadIdx.x] = fmaxf(sdata[threadIdx.x], sdata[threadIdx.x + s]);
        }
        __syncthreads();
    }

    float max_val = sdata[0];
    __syncthreads();

    // Thread 0 finds largest index with max value
    if (threadIdx.x == 0) {
        float tolerance = max_val * 0.001f + NIMCP_EPS;
        for (int i = (int)resolution - 1; i >= 0; i--) {
            if (fabsf(sample_data[i] - max_val) < tolerance) {
                outputs[sample_idx] = x_min + dx * i;
                return;
            }
        }
        outputs[sample_idx] = x_max;
    }
}

/**
 * @brief Weighted Average defuzzification kernel
 *
 * Uses term centroids weighted by firing strengths.
 */
__global__ void kernel_defuzz_weighted_avg(
    const float* __restrict__ strengths,   // [num_samples x num_terms]
    const float* __restrict__ centroids,   // [num_terms]
    float* __restrict__ outputs,           // [num_samples]
    uint32_t num_terms,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];
    float* s_num = sdata;
    float* s_den = &sdata[blockDim.x];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_strengths = &strengths[sample_idx * num_terms];

    float local_num = 0.0f;
    float local_den = 0.0f;

    for (uint32_t i = threadIdx.x; i < num_terms; i += blockDim.x) {
        float w = sample_strengths[i];
        local_num += w * centroids[i];
        local_den += w;
    }

    s_num[threadIdx.x] = local_num;
    s_den[threadIdx.x] = local_den;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            s_num[threadIdx.x] += s_num[threadIdx.x + s];
            s_den[threadIdx.x] += s_den[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        if (s_den[0] > NIMCP_EPS) {
            outputs[sample_idx] = s_num[0] / s_den[0];
        } else {
            // Default: mean of centroids
            float sum = 0.0f;
            for (uint32_t i = 0; i < num_terms; i++) sum += centroids[i];
            outputs[sample_idx] = sum / (float)num_terms;
        }
    }
}

/**
 * @brief Weighted Sum defuzzification kernel (unnormalized)
 */
__global__ void kernel_defuzz_weighted_sum(
    const float* __restrict__ strengths,
    const float* __restrict__ centroids,
    float* __restrict__ outputs,
    uint32_t num_terms,
    uint32_t num_samples)
{
    extern __shared__ float sdata[];

    uint32_t sample_idx = blockIdx.x;
    if (sample_idx >= num_samples) return;

    const float* sample_strengths = &strengths[sample_idx * num_terms];

    float local_sum = 0.0f;
    for (uint32_t i = threadIdx.x; i < num_terms; i += blockDim.x) {
        local_sum += sample_strengths[i] * centroids[i];
    }

    sdata[threadIdx.x] = local_sum;
    __syncthreads();

    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            sdata[threadIdx.x] += sdata[threadIdx.x + s];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        outputs[sample_idx] = sdata[0];
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

static __thread char g_defuzz_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_defuzz_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_defuzz_error, sizeof(g_defuzz_error), fmt, args);
    va_end(args);
}

bool nimcp_gpu_fuzzy_defuzzify_batch(
    nimcp_gpu_context_t* ctx,
    const float* aggregated,
    float* outputs,
    const nimcp_gpu_defuzz_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_defuzz_error("Invalid GPU context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
            return false;
        }
    }
    if (!aggregated || !outputs) {
        set_defuzz_error("NULL input/output pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "NULL input/output pointers");
        return false;
    }
    if (params->num_samples == 0 || params->resolution == 0) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result)) {
            set_defuzz_error("Zero samples or resolution");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Zero samples or resolution");
            return false;
        }
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    uint32_t block_size = 256;
    uint32_t num_blocks = params->num_samples;
    size_t shared_size = 3 * block_size * sizeof(float);  // For MOM which needs 3 arrays

    // Allocate device memory for inputs and outputs (host pointers cannot be used in kernels)
    size_t agg_size = (size_t)params->num_samples * params->resolution * sizeof(float);
    size_t out_size = params->num_samples * sizeof(float);
    float* d_aggregated = NULL;
    float* d_outputs = NULL;

    NIMCP_CUDA_RECOVER(cudaMalloc(&d_aggregated, agg_size), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_outputs, out_size), GPU_ERROR_OUT_OF_MEMORY);

    // Copy aggregated data to device
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_aggregated, aggregated, agg_size,
                                       cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    switch (params->method) {
        case 0:  // FUZZY_DEFUZZ_CENTROID
            shared_size = 2 * block_size * sizeof(float);
            kernel_defuzz_centroid<<<num_blocks, block_size, shared_size, stream>>>(
                d_aggregated, d_outputs, params->x_min, params->x_max,
                params->resolution, params->num_samples);
            break;

        case 1:  // FUZZY_DEFUZZ_BISECTOR
            shared_size = block_size * sizeof(float);
            kernel_defuzz_bisector<<<num_blocks, block_size, shared_size, stream>>>(
                d_aggregated, d_outputs, params->x_min, params->x_max,
                params->resolution, params->num_samples);
            break;

        case 2:  // FUZZY_DEFUZZ_MOM
            kernel_defuzz_mom<<<num_blocks, block_size, shared_size, stream>>>(
                d_aggregated, d_outputs, params->x_min, params->x_max,
                params->resolution, params->num_samples);
            break;

        case 3:  // FUZZY_DEFUZZ_SOM
            shared_size = block_size * sizeof(float);
            kernel_defuzz_som<<<num_blocks, block_size, shared_size, stream>>>(
                d_aggregated, d_outputs, params->x_min, params->x_max,
                params->resolution, params->num_samples);
            break;

        case 4:  // FUZZY_DEFUZZ_LOM
            shared_size = block_size * sizeof(float);
            kernel_defuzz_lom<<<num_blocks, block_size, shared_size, stream>>>(
                d_aggregated, d_outputs, params->x_min, params->x_max,
                params->resolution, params->num_samples);
            break;

        default:
            cudaFree(d_aggregated);
            cudaFree(d_outputs);
            set_defuzz_error("Unsupported defuzzification method: %u", params->method);
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Unsupported defuzzification method: %u", params->method);
            return false;
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Copy results back to host
    NIMCP_CUDA_RECOVER(cudaMemcpy(outputs, d_outputs, out_size,
                                  cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    cudaFree(d_aggregated);
    cudaFree(d_outputs);

    return true;
}

bool nimcp_gpu_fuzzy_defuzzify_multi_method(
    nimcp_gpu_context_t* ctx,
    const float* aggregated,
    const uint32_t* methods,
    uint32_t num_methods,
    float* outputs,
    const nimcp_gpu_defuzz_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_defuzz_error("NULL context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "NULL context");
            return false;
        }
    }
    if (!methods || !outputs) {
        set_defuzz_error("NULL pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "NULL pointers");
        return false;
    }

    // Run each method sequentially
    nimcp_gpu_defuzz_params_t local_params = *params;

    for (uint32_t m = 0; m < num_methods; m++) {
        local_params.method = methods[m];

        float* method_output = &outputs[m * params->num_samples];

        if (!nimcp_gpu_fuzzy_defuzzify_batch(ctx, aggregated, method_output, &local_params)) {
            return false;
        }
    }

    return true;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

extern "C" {

bool nimcp_gpu_fuzzy_defuzzify_batch(
    nimcp_gpu_context_t* ctx,
    const float* aggregated,
    float* outputs,
    const nimcp_gpu_defuzz_params_t* params)
{
    (void)ctx; (void)aggregated; (void)outputs; (void)params;
    return false;
}

bool nimcp_gpu_fuzzy_defuzzify_multi_method(
    nimcp_gpu_context_t* ctx,
    const float* aggregated,
    const uint32_t* methods,
    uint32_t num_methods,
    float* outputs,
    const nimcp_gpu_defuzz_params_t* params)
{
    (void)ctx; (void)aggregated; (void)methods; (void)num_methods;
    (void)outputs; (void)params;
    return false;
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
