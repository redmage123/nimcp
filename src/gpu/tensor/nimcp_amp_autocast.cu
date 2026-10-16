/**
 * @file nimcp_amp_autocast.cu
 * @brief Automatic Mixed Precision (AMP) Autocast CUDA Implementation
 *
 * WHAT: CUDA implementation for automatic precision selection context
 * WHY:  Simplifies mixed precision training with automatic dtype selection
 * HOW:  Thread-local autocast state with operation-specific precision rules
 *
 * IMPLEMENTATION NOTES:
 * - Uses nimcp_tensor_fp16.cu kernels for FP16/BF16 conversions
 * - Implements LRU cache for tensor conversions to avoid redundant casts
 * - Integrates with loss scaler for gradient underflow prevention
 * - Supports nested autocast regions with mode stack
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#include <cublas_v2.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gpu/tensor/nimcp_amp_autocast.h"
#include "gpu/tensor/nimcp_tensor_fp16.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "AMP_AUTOCAST"

// cuBLAS check with recovery (returns NULL variant)
#define CUBLAS_CHECK(call) do { \
    cublasStatus_t _status = (call); \
    if (_status != CUBLAS_STATUS_SUCCESS) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_LIBRARY, cudaErrorUnknown, &_result)) { \
            _status = (call); \
        } \
        if (_status != CUBLAS_STATUS_SUCCESS) { \
            LOG_ERROR("cuBLAS error at %s:%d: %d (unrecoverable)", __FILE__, __LINE__, _status); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 0, \
                "cuBLAS error (unrecoverable): %d", _status); \
            return NULL; \
        } \
    } \
} while(0)

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// FP16/BF16 Conversion Kernels
//=============================================================================

/**
 * @brief Convert FP32 to FP16 kernel
 */
__global__ void kernel_autocast_fp32_to_fp16(const float* __restrict__ src,
                                              __half* __restrict__ dst,
                                              size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = __float2half(src[idx]);
    }
}

/**
 * @brief Convert FP16 to FP32 kernel
 */
__global__ void kernel_autocast_fp16_to_fp32(const __half* __restrict__ src,
                                              float* __restrict__ dst,
                                              size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = __half2float(src[idx]);
    }
}

/**
 * @brief Convert FP32 to BF16 kernel
 */
__global__ void kernel_autocast_fp32_to_bf16(const float* __restrict__ src,
                                              __nv_bfloat16* __restrict__ dst,
                                              size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = __float2bfloat16(src[idx]);
    }
}

/**
 * @brief Convert BF16 to FP32 kernel
 */
__global__ void kernel_autocast_bf16_to_fp32(const __nv_bfloat16* __restrict__ src,
                                              float* __restrict__ dst,
                                              size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = __bfloat162float(src[idx]);
    }
}

//=============================================================================
// Autocast Operation Kernels
//=============================================================================

/**
 * @brief FP16 Softmax kernel (computes in FP32 for stability)
 */
__global__ void kernel_autocast_softmax_fp16(const __half* __restrict__ x,
                                              __half* __restrict__ y,
                                              int batch,
                                              int dim)
{
    int batch_idx = blockIdx.x;
    if (batch_idx >= batch) return;

    const __half* x_row = x + batch_idx * dim;
    __half* y_row = y + batch_idx * dim;

    extern __shared__ float shared[];
    float* shared_max = shared;
    float* shared_sum = shared + blockDim.x;

    // Find max (FP32 for stability)
    float thread_max = -FLT_MAX;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        thread_max = fmaxf(thread_max, val);
    }
    shared_max[threadIdx.x] = thread_max;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_max[threadIdx.x] = fmaxf(shared_max[threadIdx.x],
                                             shared_max[threadIdx.x + stride]);
        }
        __syncthreads();
    }
    float max_val = shared_max[0];
    __syncthreads();

    // Compute exp(x - max) and sum (FP32)
    float thread_sum = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        float exp_val = expf(val - max_val);
        thread_sum += exp_val;
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float sum_val = shared_sum[0];
    __syncthreads();

    // Normalize and output FP16
    float inv_sum = 1.0f / sum_val;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        float exp_val = expf(val - max_val);
        y_row[i] = __float2half(exp_val * inv_sum);
    }
}

/**
 * @brief FP32 Softmax kernel (for stable operations)
 */
__global__ void kernel_autocast_softmax_fp32(const float* __restrict__ x,
                                              float* __restrict__ y,
                                              int batch,
                                              int dim)
{
    int batch_idx = blockIdx.x;
    if (batch_idx >= batch) return;

    const float* x_row = x + batch_idx * dim;
    float* y_row = y + batch_idx * dim;

    extern __shared__ float shared[];
    float* shared_max = shared;
    float* shared_sum = shared + blockDim.x;

    // Find max
    float thread_max = -FLT_MAX;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        thread_max = fmaxf(thread_max, x_row[i]);
    }
    shared_max[threadIdx.x] = thread_max;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_max[threadIdx.x] = fmaxf(shared_max[threadIdx.x],
                                             shared_max[threadIdx.x + stride]);
        }
        __syncthreads();
    }
    float max_val = shared_max[0];
    __syncthreads();

    // Compute exp and sum
    float thread_sum = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float exp_val = expf(x_row[i] - max_val);
        thread_sum += exp_val;
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float sum_val = shared_sum[0];
    __syncthreads();

    // Normalize
    float inv_sum = 1.0f / sum_val;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        y_row[i] = expf(x_row[i] - max_val) * inv_sum;
    }
}

/**
 * @brief FP16 LayerNorm kernel (computes in FP32 for stability)
 */
__global__ void kernel_autocast_layernorm_fp16(const __half* __restrict__ x,
                                                const __half* __restrict__ gamma,
                                                const __half* __restrict__ beta,
                                                __half* __restrict__ y,
                                                int batch,
                                                int dim,
                                                float eps)
{
    int batch_idx = blockIdx.x;
    if (batch_idx >= batch) return;

    const __half* x_row = x + batch_idx * dim;
    __half* y_row = y + batch_idx * dim;

    extern __shared__ float shared[];
    float* shared_sum = shared;
    float* shared_var = shared + blockDim.x;

    // Compute mean (FP32)
    float thread_sum = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        thread_sum += __half2float(x_row[i]);
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float mean = shared_sum[0] / (float)dim;
    __syncthreads();

    // Compute variance (FP32)
    float thread_var = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float diff = __half2float(x_row[i]) - mean;
        thread_var += diff * diff;
    }
    shared_var[threadIdx.x] = thread_var;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_var[threadIdx.x] += shared_var[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float var = shared_var[0] / (float)dim;
    float inv_std = rsqrtf(var + eps);
    __syncthreads();

    // Normalize and apply affine, output FP16
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        float normalized = (val - mean) * inv_std;
        float g = gamma ? __half2float(gamma[i]) : 1.0f;
        float b = beta ? __half2float(beta[i]) : 0.0f;
        y_row[i] = __float2half(normalized * g + b);
    }
}

/**
 * @brief FP32 LayerNorm kernel
 */
__global__ void kernel_autocast_layernorm_fp32(const float* __restrict__ x,
                                                const float* __restrict__ gamma,
                                                const float* __restrict__ beta,
                                                float* __restrict__ y,
                                                int batch,
                                                int dim,
                                                float eps)
{
    int batch_idx = blockIdx.x;
    if (batch_idx >= batch) return;

    const float* x_row = x + batch_idx * dim;
    float* y_row = y + batch_idx * dim;

    extern __shared__ float shared[];
    float* shared_sum = shared;
    float* shared_var = shared + blockDim.x;

    // Compute mean
    float thread_sum = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        thread_sum += x_row[i];
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float mean = shared_sum[0] / (float)dim;
    __syncthreads();

    // Compute variance
    float thread_var = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float diff = x_row[i] - mean;
        thread_var += diff * diff;
    }
    shared_var[threadIdx.x] = thread_var;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_var[threadIdx.x] += shared_var[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float var = shared_var[0] / (float)dim;
    float inv_std = rsqrtf(var + eps);
    __syncthreads();

    // Normalize and apply affine
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float normalized = (x_row[i] - mean) * inv_std;
        float g = gamma ? gamma[i] : 1.0f;
        float b = beta ? beta[i] : 0.0f;
        y_row[i] = normalized * g + b;
    }
}

/**
 * @brief FP16 activation kernels
 */
__global__ void kernel_autocast_relu_fp16(const __half* __restrict__ x,
                                           __half* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        __half val = x[idx];
        __half zero = __float2half(0.0f);
        y[idx] = __hgt(val, zero) ? val : zero;
    }
}

__global__ void kernel_autocast_gelu_fp16(const __half* __restrict__ x,
                                           __half* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(x[idx]);
        float val_cubed = val * val * val;
        float inner = 0.7978845608f * (val + 0.044715f * val_cubed);
        float result = 0.5f * val * (1.0f + tanhf(inner));
        y[idx] = __float2half(result);
    }
}

__global__ void kernel_autocast_silu_fp16(const __half* __restrict__ x,
                                           __half* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(x[idx]);
        float sigmoid = 1.0f / (1.0f + expf(-val));
        y[idx] = __float2half(val * sigmoid);
    }
}

__global__ void kernel_autocast_sigmoid_fp16(const __half* __restrict__ x,
                                              __half* __restrict__ y,
                                              size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(x[idx]);
        y[idx] = __float2half(1.0f / (1.0f + expf(-val)));
    }
}

__global__ void kernel_autocast_tanh_fp16(const __half* __restrict__ x,
                                           __half* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(x[idx]);
        y[idx] = __float2half(tanhf(val));
    }
}

/**
 * @brief FP32 activation kernels
 */
__global__ void kernel_autocast_relu_fp32(const float* __restrict__ x,
                                           float* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = fmaxf(0.0f, x[idx]);
    }
}

__global__ void kernel_autocast_gelu_fp32(const float* __restrict__ x,
                                           float* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = x[idx];
        float val_cubed = val * val * val;
        float inner = 0.7978845608f * (val + 0.044715f * val_cubed);
        y[idx] = 0.5f * val * (1.0f + tanhf(inner));
    }
}

__global__ void kernel_autocast_silu_fp32(const float* __restrict__ x,
                                           float* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = x[idx];
        float sigmoid = 1.0f / (1.0f + expf(-val));
        y[idx] = val * sigmoid;
    }
}

__global__ void kernel_autocast_sigmoid_fp32(const float* __restrict__ x,
                                              float* __restrict__ y,
                                              size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = 1.0f / (1.0f + expf(-x[idx]));
    }
}

__global__ void kernel_autocast_tanh_fp32(const float* __restrict__ x,
                                           float* __restrict__ y,
                                           size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = tanhf(x[idx]);
    }
}

/**
 * @brief Scale gradients kernel (FP32)
 */
__global__ void kernel_autocast_scale_gradients_fp32(float* __restrict__ grads,
                                                      float scale,
                                                      size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        grads[idx] *= scale;
    }
}

/**
 * @brief Scale gradients kernel (FP16)
 */
__global__ void kernel_autocast_scale_gradients_fp16(__half* __restrict__ grads,
                                                      float scale,
                                                      size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(grads[idx]);
        grads[idx] = __float2half(val * scale);
    }
}

/**
 * @brief Check for inf/nan in tensor (FP32)
 */
__global__ void kernel_autocast_check_inf_nan_fp32(const float* __restrict__ data,
                                                    int* __restrict__ found_inf,
                                                    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = data[idx];
        if (isinf(val) || isnan(val)) {
            atomicExch(found_inf, 1);
        }
    }
}

/**
 * @brief Check for inf/nan in tensor (FP16)
 */
__global__ void kernel_autocast_check_inf_nan_fp16(const __half* __restrict__ data,
                                                    int* __restrict__ found_inf,
                                                    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(data[idx]);
        if (isinf(val) || isnan(val)) {
            atomicExch(found_inf, 1);
        }
    }
}

//=============================================================================
// Default Operation Precision Mapping
//=============================================================================

/**
 * @brief Default dtype for each operation in FP16 mode
 */
static const nimcp_mp_dtype_t g_fp16_op_dtypes[AUTOCAST_OP_COUNT] = {
    [AUTOCAST_OP_MATMUL]      = MP_DTYPE_FP16,
    [AUTOCAST_OP_CONV]        = MP_DTYPE_FP16,
    [AUTOCAST_OP_ATTENTION]   = MP_DTYPE_FP16,
    [AUTOCAST_OP_LINEAR]      = MP_DTYPE_FP16,
    [AUTOCAST_OP_EMBEDDING]   = MP_DTYPE_FP16,
    [AUTOCAST_OP_SOFTMAX]     = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_LAYERNORM]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_BATCHNORM]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_GROUPNORM]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_LOSS]        = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_REDUCTION]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_ACTIVATION]  = MP_DTYPE_FP16,
    [AUTOCAST_OP_ELEMENTWISE] = MP_DTYPE_FP16,
    [AUTOCAST_OP_CUSTOM]      = MP_DTYPE_FP32,
};

/**
 * @brief Default dtype for each operation in BF16 mode
 */
static const nimcp_mp_dtype_t g_bf16_op_dtypes[AUTOCAST_OP_COUNT] = {
    [AUTOCAST_OP_MATMUL]      = MP_DTYPE_BF16,
    [AUTOCAST_OP_CONV]        = MP_DTYPE_BF16,
    [AUTOCAST_OP_ATTENTION]   = MP_DTYPE_BF16,
    [AUTOCAST_OP_LINEAR]      = MP_DTYPE_BF16,
    [AUTOCAST_OP_EMBEDDING]   = MP_DTYPE_BF16,
    [AUTOCAST_OP_SOFTMAX]     = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_LAYERNORM]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_BATCHNORM]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_GROUPNORM]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_LOSS]        = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_REDUCTION]   = MP_DTYPE_FP32,  // Always FP32 for stability
    [AUTOCAST_OP_ACTIVATION]  = MP_DTYPE_BF16,
    [AUTOCAST_OP_ELEMENTWISE] = MP_DTYPE_BF16,
    [AUTOCAST_OP_CUSTOM]      = MP_DTYPE_FP32,
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Convert mp_dtype to gpu_precision
 */
static nimcp_gpu_precision_t mp_dtype_to_precision(nimcp_mp_dtype_t dtype)
{
    switch (dtype) {
        case MP_DTYPE_FP16: return NIMCP_GPU_PRECISION_FP16;
        case MP_DTYPE_BF16: return NIMCP_GPU_PRECISION_BF16;
        case MP_DTYPE_TF32: return NIMCP_GPU_PRECISION_TF32;
        case MP_DTYPE_FP32:
        default: return NIMCP_GPU_PRECISION_FP32;
    }
}

/**
 * @brief Convert gpu_precision to mp_dtype
 */
static nimcp_mp_dtype_t precision_to_mp_dtype(nimcp_gpu_precision_t precision)
{
    switch (precision) {
        case NIMCP_GPU_PRECISION_FP16: return MP_DTYPE_FP16;
        case NIMCP_GPU_PRECISION_BF16: return MP_DTYPE_BF16;
        case NIMCP_GPU_PRECISION_TF32: return MP_DTYPE_TF32;
        case NIMCP_GPU_PRECISION_FP32:
        default: return MP_DTYPE_FP32;
    }
}

/**
 * @brief Initialize op dtypes based on mode
 */
static void init_op_dtypes(nimcp_autocast_ctx_t* ctx, nimcp_autocast_mode_t mode)
{
    const nimcp_mp_dtype_t* defaults;

    switch (mode) {
        case AUTOCAST_FP16:
            defaults = g_fp16_op_dtypes;
            break;
        case AUTOCAST_BF16:
            defaults = g_bf16_op_dtypes;
            break;
        case AUTOCAST_TF32:
            // TF32 uses FP32 for everything but with TF32 tensor cores
            for (int i = 0; i < AUTOCAST_OP_COUNT; i++) {
                ctx->op_dtypes[i] = MP_DTYPE_FP32;
                ctx->op_force_fp32[i] = false;
            }
            return;
        case AUTOCAST_DISABLED:
        default:
            for (int i = 0; i < AUTOCAST_OP_COUNT; i++) {
                ctx->op_dtypes[i] = MP_DTYPE_FP32;
                ctx->op_force_fp32[i] = true;
            }
            return;
    }

    memcpy(ctx->op_dtypes, defaults, sizeof(ctx->op_dtypes));

    // Set force_fp32 for operations that always use FP32
    for (int i = 0; i < AUTOCAST_OP_COUNT; i++) {
        ctx->op_force_fp32[i] = (ctx->op_dtypes[i] == MP_DTYPE_FP32);
    }
}

//=============================================================================
// Cache Management
//=============================================================================

/**
 * @brief Find cached tensor conversion
 */
static nimcp_gpu_tensor_t* cache_lookup(nimcp_autocast_ctx_t* ctx,
                                         const nimcp_gpu_tensor_t* original,
                                         nimcp_mp_dtype_t target_dtype)
{
    for (int i = 0; i < ctx->cache_count; i++) {
        nimcp_autocast_cache_entry_t* entry = &ctx->cache[i];
        if (entry->valid &&
            entry->original == original &&
            entry->target_dtype == target_dtype) {
            entry->last_used = ++ctx->cache_timestamp;
            ctx->cache_hits++;
            return entry->converted;
        }
    }
    ctx->cache_misses++;
    return NULL;
}

/**
 * @brief Add tensor to cache
 */
static void cache_insert(nimcp_autocast_ctx_t* ctx,
                         const nimcp_gpu_tensor_t* original,
                         nimcp_gpu_tensor_t* converted,
                         nimcp_mp_dtype_t target_dtype)
{
    // Find empty slot or LRU entry
    int slot = -1;
    uint64_t oldest_time = UINT64_MAX;
    int oldest_slot = 0;

    for (int i = 0; i < AUTOCAST_CACHE_SIZE; i++) {
        if (!ctx->cache[i].valid) {
            slot = i;
            break;
        }
        if (ctx->cache[i].last_used < oldest_time) {
            oldest_time = ctx->cache[i].last_used;
            oldest_slot = i;
        }
    }

    // Evict LRU if cache is full
    if (slot < 0) {
        slot = oldest_slot;
        if (ctx->cache[slot].converted) {
            nimcp_gpu_tensor_destroy(ctx->cache[slot].converted);
        }
    }

    ctx->cache[slot].original = original;
    ctx->cache[slot].converted = converted;
    ctx->cache[slot].target_dtype = target_dtype;
    ctx->cache[slot].last_used = ++ctx->cache_timestamp;
    ctx->cache[slot].valid = true;

    if (slot >= ctx->cache_count) {
        ctx->cache_count = slot + 1;
    }
}

//=============================================================================
// Autocast Context Lifecycle
//=============================================================================

void nimcp_autocast_default_config(nimcp_autocast_config_t* config,
                                    nimcp_autocast_mode_t mode)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->mode = mode;
    config->enable_caching = true;
    config->enable_scaler = (mode == AUTOCAST_FP16);  // Only for FP16
    config->init_scale = MP_DEFAULT_INIT_SCALE;
    config->op_overrides = NULL;
    config->num_overrides = 0;
}

nimcp_autocast_ctx_t* nimcp_autocast_create(nimcp_gpu_context_t* gpu_ctx,
                                             nimcp_autocast_mode_t mode)
{
    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_autocast_config_t config;
    nimcp_autocast_default_config(&config, mode);
    return nimcp_autocast_create_with_config(gpu_ctx, &config);
}

nimcp_autocast_ctx_t* nimcp_autocast_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    const nimcp_autocast_config_t* config)
{
    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx || !config) {
        LOG_ERROR("Invalid parameters for autocast creation");
        return NULL;
    }

    nimcp_autocast_ctx_t* ctx = (nimcp_autocast_ctx_t*)nimcp_calloc(1, sizeof(nimcp_autocast_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate autocast context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->mode = config->mode;
    ctx->enabled = (config->mode != AUTOCAST_DISABLED);
    ctx->nesting_level = 0;

    // Initialize operation dtypes
    init_op_dtypes(ctx, config->mode);

    // Apply any overrides
    if (config->op_overrides && config->num_overrides > 0) {
        for (int i = 0; i < config->num_overrides && i < AUTOCAST_OP_COUNT; i++) {
            ctx->op_dtypes[i] = config->op_overrides[i];
            ctx->op_force_fp32[i] = (config->op_overrides[i] == MP_DTYPE_FP32);
        }
    }

    // Create loss scaler if enabled
    if (config->enable_scaler && config->mode == AUTOCAST_FP16) {
        ctx->scaler = nimcp_loss_scaler_create_custom(
            config->init_scale,
            MP_DEFAULT_GROWTH_FACTOR,
            MP_DEFAULT_BACKOFF_FACTOR,
            MP_DEFAULT_GROWTH_INTERVAL,
            true
        );
        ctx->owns_scaler = true;
    }

    LOG_INFO("Autocast context created: mode=%s, scaler=%s",
             nimcp_autocast_mode_name(config->mode),
             ctx->scaler ? "enabled" : "disabled");

    return ctx;
}

void nimcp_autocast_destroy(nimcp_autocast_ctx_t* ctx)
{
    if (!ctx) return;

    // Free cached tensors
    nimcp_autocast_clear_cache(ctx);

    // Free loss scaler if owned
    if (ctx->owns_scaler && ctx->scaler) {
        nimcp_loss_scaler_destroy(ctx->scaler);
    }

    nimcp_free(ctx);
    LOG_DEBUG("Autocast context destroyed");
}

//=============================================================================
// Autocast Region Control
//=============================================================================

bool nimcp_autocast_begin(nimcp_autocast_ctx_t* ctx)
{
    if (!ctx) return false;

    if (ctx->nesting_level >= AUTOCAST_MAX_NESTING) {
        LOG_ERROR("Autocast nesting level exceeded (%d)", AUTOCAST_MAX_NESTING);
        return false;
    }

    // Save current mode on stack
    ctx->mode_stack[ctx->nesting_level] = ctx->mode;
    ctx->nesting_level++;
    ctx->enabled = true;

    LOG_DEBUG("Entered autocast region (level %d)", ctx->nesting_level);
    return true;
}

bool nimcp_autocast_begin_with_mode(nimcp_autocast_ctx_t* ctx,
                                     nimcp_autocast_mode_t mode)
{
    if (!ctx) return false;

    if (ctx->nesting_level >= AUTOCAST_MAX_NESTING) {
        LOG_ERROR("Autocast nesting level exceeded (%d)", AUTOCAST_MAX_NESTING);
        return false;
    }

    // Save current mode and set new mode
    ctx->mode_stack[ctx->nesting_level] = ctx->mode;
    ctx->nesting_level++;
    ctx->mode = mode;
    ctx->enabled = (mode != AUTOCAST_DISABLED);

    // Re-initialize op dtypes for new mode
    init_op_dtypes(ctx, mode);

    LOG_DEBUG("Entered autocast region with mode %s (level %d)",
              nimcp_autocast_mode_name(mode), ctx->nesting_level);
    return true;
}

bool nimcp_autocast_end(nimcp_autocast_ctx_t* ctx)
{
    if (!ctx) return false;

    if (ctx->nesting_level <= 0) {
        LOG_ERROR("Autocast end without matching begin");
        return false;
    }

    ctx->nesting_level--;
    ctx->mode = ctx->mode_stack[ctx->nesting_level];
    ctx->enabled = (ctx->mode != AUTOCAST_DISABLED) && (ctx->nesting_level > 0);

    // Restore op dtypes for previous mode
    if (ctx->nesting_level > 0) {
        init_op_dtypes(ctx, ctx->mode);
    }

    LOG_DEBUG("Exited autocast region (level %d)", ctx->nesting_level);
    return true;
}

bool nimcp_autocast_is_active(const nimcp_autocast_ctx_t* ctx)
{
    return ctx && ctx->enabled && ctx->nesting_level > 0;
}

nimcp_autocast_mode_t nimcp_autocast_get_mode(const nimcp_autocast_ctx_t* ctx)
{
    return ctx ? ctx->mode : AUTOCAST_DISABLED;
}

//=============================================================================
// Precision Query API
//=============================================================================

nimcp_mp_dtype_t nimcp_autocast_get_op_dtype(const nimcp_autocast_ctx_t* ctx,
                                              nimcp_autocast_op_t op)
{
    if (!ctx || op >= AUTOCAST_OP_COUNT) {
        return MP_DTYPE_FP32;
    }

    // Not in autocast region - use FP32
    if (!nimcp_autocast_is_active(ctx)) {
        return MP_DTYPE_FP32;
    }

    // Force FP32 override
    if (ctx->op_force_fp32[op]) {
        return MP_DTYPE_FP32;
    }

    return ctx->op_dtypes[op];
}

bool nimcp_autocast_set_op_dtype(nimcp_autocast_ctx_t* ctx,
                                  nimcp_autocast_op_t op,
                                  nimcp_mp_dtype_t dtype)
{
    if (!ctx || op >= AUTOCAST_OP_COUNT) {
        return false;
    }

    ctx->op_dtypes[op] = dtype;
    ctx->op_force_fp32[op] = false;  // Clear force flag when explicitly setting
    return true;
}

void nimcp_autocast_force_fp32(nimcp_autocast_ctx_t* ctx,
                                nimcp_autocast_op_t op,
                                bool force_fp32)
{
    if (ctx && op < AUTOCAST_OP_COUNT) {
        ctx->op_force_fp32[op] = force_fp32;
    }
}

//=============================================================================
// Automatic Tensor Casting
//=============================================================================

nimcp_gpu_tensor_t* nimcp_autocast_cast_input(nimcp_autocast_ctx_t* ctx,
                                               nimcp_gpu_tensor_t* tensor,
                                               nimcp_autocast_op_t op)
{
    if (!ctx || !tensor) return tensor;

    // Not in autocast region - no casting
    if (!nimcp_autocast_is_active(ctx)) {
        return tensor;
    }

    nimcp_mp_dtype_t target_dtype = nimcp_autocast_get_op_dtype(ctx, op);
    nimcp_mp_dtype_t current_dtype = precision_to_mp_dtype(tensor->precision);

    // Already correct dtype
    if (target_dtype == current_dtype) {
        return tensor;
    }

    // Check cache first
    nimcp_gpu_tensor_t* cached = cache_lookup(ctx, tensor, target_dtype);
    if (cached) {
        return cached;
    }

    // Create converted tensor
    nimcp_gpu_precision_t target_precision = mp_dtype_to_precision(target_dtype);
    nimcp_gpu_tensor_t* converted = nimcp_gpu_tensor_create(
        ctx->gpu_ctx, tensor->dims, tensor->ndim, target_precision);

    if (!converted) {
        LOG_ERROR("Failed to create converted tensor");
        return tensor;
    }

    // Perform conversion using our CUDA kernels
    size_t n = tensor->numel;
    bool success = false;

    if (current_dtype == MP_DTYPE_FP32 && target_dtype == MP_DTYPE_FP16) {
        kernel_autocast_fp32_to_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)tensor->data, (__half*)converted->data, n);
        success = (cudaGetLastError() == cudaSuccess);
        ctx->fp16_ops++;
    } else if (current_dtype == MP_DTYPE_FP16 && target_dtype == MP_DTYPE_FP32) {
        kernel_autocast_fp16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __half*)tensor->data, (float*)converted->data, n);
        success = (cudaGetLastError() == cudaSuccess);
        ctx->fp32_ops++;
    } else if (current_dtype == MP_DTYPE_FP32 && target_dtype == MP_DTYPE_BF16) {
        kernel_autocast_fp32_to_bf16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)tensor->data, (__nv_bfloat16*)converted->data, n);
        success = (cudaGetLastError() == cudaSuccess);
        ctx->fp16_ops++;
    } else if (current_dtype == MP_DTYPE_BF16 && target_dtype == MP_DTYPE_FP32) {
        kernel_autocast_bf16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __nv_bfloat16*)tensor->data, (float*)converted->data, n);
        success = (cudaGetLastError() == cudaSuccess);
        ctx->fp32_ops++;
    }

    if (!success) {
        LOG_ERROR("Tensor conversion failed");
        nimcp_gpu_tensor_destroy(converted);
        return tensor;
    }

    // Cache the conversion
    cache_insert(ctx, tensor, converted, target_dtype);
    ctx->casts_performed++;

    return converted;
}

nimcp_gpu_tensor_t* nimcp_autocast_cast_output_fp32(nimcp_autocast_ctx_t* ctx,
                                                     nimcp_gpu_tensor_t* tensor)
{
    if (!ctx || !tensor) return tensor;

    nimcp_mp_dtype_t current_dtype = precision_to_mp_dtype(tensor->precision);

    // Already FP32
    if (current_dtype == MP_DTYPE_FP32) {
        return tensor;
    }

    // Check cache
    nimcp_gpu_tensor_t* cached = cache_lookup(ctx, tensor, MP_DTYPE_FP32);
    if (cached) {
        return cached;
    }

    // Convert to FP32
    nimcp_gpu_tensor_t* converted = nimcp_gpu_tensor_create(
        ctx->gpu_ctx, tensor->dims, tensor->ndim, NIMCP_GPU_PRECISION_FP32);

    if (!converted) {
        return tensor;
    }

    size_t n = tensor->numel;
    bool success = false;

    if (current_dtype == MP_DTYPE_FP16) {
        kernel_autocast_fp16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __half*)tensor->data, (float*)converted->data, n);
        success = (cudaGetLastError() == cudaSuccess);
    } else if (current_dtype == MP_DTYPE_BF16) {
        kernel_autocast_bf16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __nv_bfloat16*)tensor->data, (float*)converted->data, n);
        success = (cudaGetLastError() == cudaSuccess);
    }

    if (!success) {
        nimcp_gpu_tensor_destroy(converted);
        return tensor;
    }

    cache_insert(ctx, tensor, converted, MP_DTYPE_FP32);
    ctx->casts_performed++;
    ctx->fp32_ops++;
    return converted;
}

void nimcp_autocast_clear_cache(nimcp_autocast_ctx_t* ctx)
{
    if (!ctx) return;

    for (int i = 0; i < ctx->cache_count; i++) {
        if (ctx->cache[i].valid && ctx->cache[i].converted) {
            nimcp_gpu_tensor_destroy(ctx->cache[i].converted);
        }
        ctx->cache[i].valid = false;
        ctx->cache[i].converted = NULL;
        ctx->cache[i].original = NULL;
    }
    ctx->cache_count = 0;
    ctx->cache_timestamp = 0;

    LOG_DEBUG("Autocast cache cleared");
}

//=============================================================================
// Autocast Operations
//=============================================================================

nimcp_gpu_tensor_t* nimcp_autocast_matmul(nimcp_autocast_ctx_t* ctx,
                                           nimcp_gpu_tensor_t* A,
                                           nimcp_gpu_tensor_t* B,
                                           nimcp_gpu_tensor_t* C,
                                           float alpha,
                                           float beta,
                                           bool trans_a,
                                           bool trans_b)
{
    if (!ctx || !A || !B) return NULL;

    // Cast inputs to appropriate dtype
    nimcp_gpu_tensor_t* A_cast = nimcp_autocast_cast_input(ctx, A, AUTOCAST_OP_MATMUL);
    nimcp_gpu_tensor_t* B_cast = nimcp_autocast_cast_input(ctx, B, AUTOCAST_OP_MATMUL);

    // Create output if needed
    if (!C) {
        int M = trans_a ? A->dims[A->ndim - 1] : A->dims[A->ndim - 2];
        int N = trans_b ? B->dims[B->ndim - 2] : B->dims[B->ndim - 1];
        size_t out_dims[] = {(size_t)M, (size_t)N};
        C = nimcp_gpu_tensor_create(ctx->gpu_ctx, out_dims, 2, A_cast->precision);
        if (!C) return NULL;
    }

    // Perform GEMM in appropriate precision
    nimcp_mp_dtype_t dtype = nimcp_autocast_get_op_dtype(ctx, AUTOCAST_OP_MATMUL);
    bool success = false;

    if (dtype == MP_DTYPE_FP16 || dtype == MP_DTYPE_BF16) {
        success = nimcp_fp16_gemm(ctx->gpu_ctx, A_cast, B_cast, C,
                                   alpha, beta, trans_a, trans_b);
        ctx->fp16_ops++;
    } else {
        success = nimcp_gpu_gemm(ctx->gpu_ctx, A_cast, B_cast, C,
                                  alpha, beta, trans_a, trans_b);
        ctx->fp32_ops++;
    }

    return success ? C : NULL;
}

nimcp_gpu_tensor_t* nimcp_autocast_softmax(nimcp_autocast_ctx_t* ctx,
                                            nimcp_gpu_tensor_t* x,
                                            nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x) return NULL;

    int dim = x->dims[x->ndim - 1];
    int batch = x->numel / dim;

    // Create output if needed
    if (!out) {
        out = nimcp_gpu_tensor_create(ctx->gpu_ctx, x->dims, x->ndim, x->precision);
        if (!out) return NULL;
    }

    // Softmax always computed in FP32 for stability, but can take FP16 input
    size_t shared_mem = 2 * BLOCK_SIZE * sizeof(float);

    if (x->precision == NIMCP_GPU_PRECISION_FP16) {
        kernel_autocast_softmax_fp16<<<batch, BLOCK_SIZE, shared_mem>>>(
            (const __half*)x->data, (__half*)out->data, batch, dim);
    } else {
        kernel_autocast_softmax_fp32<<<batch, BLOCK_SIZE, shared_mem>>>(
            (const float*)x->data, (float*)out->data, batch, dim);
    }

    if (cudaGetLastError() != cudaSuccess) {
        LOG_ERROR("Softmax kernel failed");
        return NULL;
    }

    ctx->fp32_ops++;  // Softmax counts as FP32 for stability
    return out;
}

nimcp_gpu_tensor_t* nimcp_autocast_layernorm(nimcp_autocast_ctx_t* ctx,
                                              nimcp_gpu_tensor_t* x,
                                              nimcp_gpu_tensor_t* gamma,
                                              nimcp_gpu_tensor_t* beta,
                                              nimcp_gpu_tensor_t* out,
                                              float eps)
{
    if (!ctx || !x) return NULL;

    int dim = x->dims[x->ndim - 1];
    int batch = x->numel / dim;

    // Create output if needed
    if (!out) {
        out = nimcp_gpu_tensor_create(ctx->gpu_ctx, x->dims, x->ndim, x->precision);
        if (!out) return NULL;
    }

    size_t shared_mem = 2 * BLOCK_SIZE * sizeof(float);

    if (x->precision == NIMCP_GPU_PRECISION_FP16) {
        kernel_autocast_layernorm_fp16<<<batch, BLOCK_SIZE, shared_mem>>>(
            (const __half*)x->data,
            gamma ? (const __half*)gamma->data : NULL,
            beta ? (const __half*)beta->data : NULL,
            (__half*)out->data, batch, dim, eps);
    } else {
        kernel_autocast_layernorm_fp32<<<batch, BLOCK_SIZE, shared_mem>>>(
            (const float*)x->data,
            gamma ? (const float*)gamma->data : NULL,
            beta ? (const float*)beta->data : NULL,
            (float*)out->data, batch, dim, eps);
    }

    if (cudaGetLastError() != cudaSuccess) {
        LOG_ERROR("LayerNorm kernel failed");
        return NULL;
    }

    ctx->fp32_ops++;  // LayerNorm counts as FP32 for stability
    return out;
}

nimcp_gpu_tensor_t* nimcp_autocast_activation(nimcp_autocast_ctx_t* ctx,
                                               nimcp_gpu_tensor_t* x,
                                               nimcp_gpu_tensor_t* out,
                                               int activation)
{
    if (!ctx || !x) return NULL;

    // Cast input to activation dtype
    nimcp_gpu_tensor_t* x_cast = nimcp_autocast_cast_input(ctx, x, AUTOCAST_OP_ACTIVATION);

    if (!out) {
        out = nimcp_gpu_tensor_create(ctx->gpu_ctx, x->dims, x->ndim, x_cast->precision);
        if (!out) return NULL;
    }

    size_t n = x_cast->numel;
    bool is_fp16 = (x_cast->precision == NIMCP_GPU_PRECISION_FP16);

    switch (activation) {
        case 0: // ReLU
            if (is_fp16) {
                kernel_autocast_relu_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const __half*)x_cast->data, (__half*)out->data, n);
            } else {
                kernel_autocast_relu_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const float*)x_cast->data, (float*)out->data, n);
            }
            break;

        case 1: // GELU
            if (is_fp16) {
                kernel_autocast_gelu_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const __half*)x_cast->data, (__half*)out->data, n);
            } else {
                kernel_autocast_gelu_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const float*)x_cast->data, (float*)out->data, n);
            }
            break;

        case 2: // SiLU/Swish
            if (is_fp16) {
                kernel_autocast_silu_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const __half*)x_cast->data, (__half*)out->data, n);
            } else {
                kernel_autocast_silu_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const float*)x_cast->data, (float*)out->data, n);
            }
            break;

        case 3: // Sigmoid
            if (is_fp16) {
                kernel_autocast_sigmoid_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const __half*)x_cast->data, (__half*)out->data, n);
            } else {
                kernel_autocast_sigmoid_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const float*)x_cast->data, (float*)out->data, n);
            }
            break;

        case 4: // Tanh
            if (is_fp16) {
                kernel_autocast_tanh_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const __half*)x_cast->data, (__half*)out->data, n);
            } else {
                kernel_autocast_tanh_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (const float*)x_cast->data, (float*)out->data, n);
            }
            break;

        default:
            LOG_ERROR("Unknown activation type: %d", activation);
            return NULL;
    }

    if (cudaGetLastError() != cudaSuccess) {
        LOG_ERROR("Activation kernel failed");
        return NULL;
    }

    if (is_fp16) {
        ctx->fp16_ops++;
    } else {
        ctx->fp32_ops++;
    }

    return out;
}

//=============================================================================
// Loss Scaling Integration
//=============================================================================

nimcp_loss_scaler_t* nimcp_autocast_get_scaler(nimcp_autocast_ctx_t* ctx)
{
    return ctx ? ctx->scaler : NULL;
}

void nimcp_autocast_set_scaler(nimcp_autocast_ctx_t* ctx,
                                nimcp_loss_scaler_t* scaler)
{
    if (!ctx) return;

    // Free old scaler if owned
    if (ctx->owns_scaler && ctx->scaler) {
        nimcp_loss_scaler_destroy(ctx->scaler);
    }

    ctx->scaler = scaler;
    ctx->owns_scaler = true;  // Take ownership
}

float nimcp_autocast_scale_loss(nimcp_autocast_ctx_t* ctx, float loss)
{
    if (!ctx || !ctx->scaler) return loss;
    return nimcp_loss_scaler_scale(ctx->scaler, loss);
}

bool nimcp_autocast_unscale_gradients(nimcp_autocast_ctx_t* ctx,
                                       nimcp_gpu_tensor_t* gradients)
{
    if (!ctx || !ctx->scaler || !gradients) return true;

    float inv_scale = 1.0f / nimcp_loss_scaler_get_scale(ctx->scaler);
    size_t n = gradients->numel;

    // Allocate device memory for inf check
    int* d_found_inf;
    if (cudaMalloc(&d_found_inf, sizeof(int)) != cudaSuccess) {
        return false;
    }
    cudaMemset(d_found_inf, 0, sizeof(int));

    // Unscale and check for inf/nan
    if (gradients->precision == NIMCP_GPU_PRECISION_FP16) {
        kernel_autocast_scale_gradients_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (__half*)gradients->data, inv_scale, n);
        kernel_autocast_check_inf_nan_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __half*)gradients->data, d_found_inf, n);
    } else {
        kernel_autocast_scale_gradients_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (float*)gradients->data, inv_scale, n);
        kernel_autocast_check_inf_nan_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)gradients->data, d_found_inf, n);
    }

    int found_inf = 0;
    cudaMemcpy(&found_inf, d_found_inf, sizeof(int), cudaMemcpyDeviceToHost);
    cudaFree(d_found_inf);

    if (found_inf) {
        LOG_DEBUG("Gradients contain inf/nan after unscaling");
        return false;
    }

    return true;
}

void nimcp_autocast_update_scale(nimcp_autocast_ctx_t* ctx, bool gradients_valid)
{
    if (ctx && ctx->scaler) {
        nimcp_loss_scaler_update(ctx->scaler, gradients_valid);
    }
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

void nimcp_autocast_get_stats(const nimcp_autocast_ctx_t* ctx,
                               uint64_t* casts_performed,
                               uint64_t* cache_hits,
                               uint64_t* cache_misses)
{
    if (!ctx) return;
    if (casts_performed) *casts_performed = ctx->casts_performed;
    if (cache_hits) *cache_hits = ctx->cache_hits;
    if (cache_misses) *cache_misses = ctx->cache_misses;
}

void nimcp_autocast_reset_stats(nimcp_autocast_ctx_t* ctx)
{
    if (!ctx) return;
    ctx->casts_performed = 0;
    ctx->cache_hits = 0;
    ctx->cache_misses = 0;
    ctx->fp16_ops = 0;
    ctx->fp32_ops = 0;
}

const char* nimcp_autocast_op_name(nimcp_autocast_op_t op)
{
    switch (op) {
        case AUTOCAST_OP_MATMUL:      return "MATMUL";
        case AUTOCAST_OP_CONV:        return "CONV";
        case AUTOCAST_OP_ATTENTION:   return "ATTENTION";
        case AUTOCAST_OP_LINEAR:      return "LINEAR";
        case AUTOCAST_OP_EMBEDDING:   return "EMBEDDING";
        case AUTOCAST_OP_SOFTMAX:     return "SOFTMAX";
        case AUTOCAST_OP_LAYERNORM:   return "LAYERNORM";
        case AUTOCAST_OP_BATCHNORM:   return "BATCHNORM";
        case AUTOCAST_OP_GROUPNORM:   return "GROUPNORM";
        case AUTOCAST_OP_LOSS:        return "LOSS";
        case AUTOCAST_OP_REDUCTION:   return "REDUCTION";
        case AUTOCAST_OP_ACTIVATION:  return "ACTIVATION";
        case AUTOCAST_OP_ELEMENTWISE: return "ELEMENTWISE";
        case AUTOCAST_OP_CUSTOM:      return "CUSTOM";
        default:                      return "UNKNOWN";
    }
}

const char* nimcp_autocast_mode_name(nimcp_autocast_mode_t mode)
{
    switch (mode) {
        case AUTOCAST_DISABLED: return "DISABLED";
        case AUTOCAST_FP16:     return "FP16";
        case AUTOCAST_BF16:     return "BF16";
        case AUTOCAST_TF32:     return "TF32";
        default:                return "UNKNOWN";
    }
}

void nimcp_autocast_print_config(const nimcp_autocast_ctx_t* ctx)
{
    if (!ctx) return;

    printf("=== Autocast Configuration ===\n");
    printf("Mode: %s\n", nimcp_autocast_mode_name(ctx->mode));
    printf("Enabled: %s\n", ctx->enabled ? "yes" : "no");
    printf("Nesting level: %d\n", ctx->nesting_level);
    printf("Loss scaler: %s\n", ctx->scaler ? "enabled" : "disabled");
    if (ctx->scaler) {
        printf("  Current scale: %.0f\n", nimcp_loss_scaler_get_scale(ctx->scaler));
    }
    printf("Cache entries: %d/%d\n", ctx->cache_count, AUTOCAST_CACHE_SIZE);
    printf("\nStatistics:\n");
    printf("  Casts performed: %lu\n", (unsigned long)ctx->casts_performed);
    printf("  Cache hits: %lu\n", (unsigned long)ctx->cache_hits);
    printf("  Cache misses: %lu\n", (unsigned long)ctx->cache_misses);
    printf("  FP16 ops: %lu\n", (unsigned long)ctx->fp16_ops);
    printf("  FP32 ops: %lu\n", (unsigned long)ctx->fp32_ops);

    printf("\nOperation dtypes:\n");
    for (int i = 0; i < AUTOCAST_OP_COUNT; i++) {
        printf("  %-12s: %s%s\n",
               nimcp_autocast_op_name((nimcp_autocast_op_t)i),
               nimcp_mp_dtype_name(ctx->op_dtypes[i]),
               ctx->op_force_fp32[i] ? " (forced FP32)" : "");
    }
    printf("==============================\n");
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/tensor/nimcp_amp_autocast.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "AMP_AUTOCAST"

void nimcp_autocast_default_config(nimcp_autocast_config_t* config,
                                    nimcp_autocast_mode_t mode)
{
    if (config) {
        memset(config, 0, sizeof(*config));
        config->mode = AUTOCAST_DISABLED;
        config->enable_caching = false;
        config->enable_scaler = false;
        config->init_scale = 1.0f;
        config->op_overrides = NULL;
        config->num_overrides = 0;
    }
}

nimcp_autocast_ctx_t* nimcp_autocast_create(nimcp_gpu_context_t* gpu_ctx,
                                             nimcp_autocast_mode_t mode)
{
    LOG_WARN("CUDA not available - autocast requires GPU");
    return NULL;
}

nimcp_autocast_ctx_t* nimcp_autocast_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    const nimcp_autocast_config_t* config)
{
    LOG_WARN("CUDA not available - autocast requires GPU");
    return NULL;
}

void nimcp_autocast_destroy(nimcp_autocast_ctx_t* ctx) {}

bool nimcp_autocast_begin(nimcp_autocast_ctx_t* ctx) { return false; }
bool nimcp_autocast_begin_with_mode(nimcp_autocast_ctx_t* ctx,
                                     nimcp_autocast_mode_t mode) { return false; }
bool nimcp_autocast_end(nimcp_autocast_ctx_t* ctx) { return false; }
bool nimcp_autocast_is_active(const nimcp_autocast_ctx_t* ctx) { return false; }
nimcp_autocast_mode_t nimcp_autocast_get_mode(const nimcp_autocast_ctx_t* ctx)
{
    return AUTOCAST_DISABLED;
}

nimcp_mp_dtype_t nimcp_autocast_get_op_dtype(const nimcp_autocast_ctx_t* ctx,
                                              nimcp_autocast_op_t op)
{
    return MP_DTYPE_FP32;
}

bool nimcp_autocast_set_op_dtype(nimcp_autocast_ctx_t* ctx,
                                  nimcp_autocast_op_t op,
                                  nimcp_mp_dtype_t dtype) { return false; }

void nimcp_autocast_force_fp32(nimcp_autocast_ctx_t* ctx,
                                nimcp_autocast_op_t op,
                                bool force_fp32) {}

nimcp_gpu_tensor_t* nimcp_autocast_cast_input(nimcp_autocast_ctx_t* ctx,
                                               nimcp_gpu_tensor_t* tensor,
                                               nimcp_autocast_op_t op)
{
    return tensor;
}

nimcp_gpu_tensor_t* nimcp_autocast_cast_output_fp32(nimcp_autocast_ctx_t* ctx,
                                                     nimcp_gpu_tensor_t* tensor)
{
    return tensor;
}

void nimcp_autocast_clear_cache(nimcp_autocast_ctx_t* ctx) {}

nimcp_gpu_tensor_t* nimcp_autocast_matmul(nimcp_autocast_ctx_t* ctx,
                                           nimcp_gpu_tensor_t* A,
                                           nimcp_gpu_tensor_t* B,
                                           nimcp_gpu_tensor_t* C,
                                           float alpha, float beta,
                                           bool trans_a, bool trans_b)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_autocast_softmax(nimcp_autocast_ctx_t* ctx,
                                            nimcp_gpu_tensor_t* x,
                                            nimcp_gpu_tensor_t* out)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_autocast_layernorm(nimcp_autocast_ctx_t* ctx,
                                              nimcp_gpu_tensor_t* x,
                                              nimcp_gpu_tensor_t* gamma,
                                              nimcp_gpu_tensor_t* beta,
                                              nimcp_gpu_tensor_t* out,
                                              float eps)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_autocast_activation(nimcp_autocast_ctx_t* ctx,
                                               nimcp_gpu_tensor_t* x,
                                               nimcp_gpu_tensor_t* out,
                                               int activation)
{
    return NULL;
}

nimcp_loss_scaler_t* nimcp_autocast_get_scaler(nimcp_autocast_ctx_t* ctx)
{
    return NULL;
}

void nimcp_autocast_set_scaler(nimcp_autocast_ctx_t* ctx,
                                nimcp_loss_scaler_t* scaler) {}

float nimcp_autocast_scale_loss(nimcp_autocast_ctx_t* ctx, float loss)
{
    return loss;
}

bool nimcp_autocast_unscale_gradients(nimcp_autocast_ctx_t* ctx,
                                       nimcp_gpu_tensor_t* gradients)
{
    return true;
}

void nimcp_autocast_update_scale(nimcp_autocast_ctx_t* ctx,
                                  bool gradients_valid) {}

void nimcp_autocast_get_stats(const nimcp_autocast_ctx_t* ctx,
                               uint64_t* casts_performed,
                               uint64_t* cache_hits,
                               uint64_t* cache_misses) {}

void nimcp_autocast_reset_stats(nimcp_autocast_ctx_t* ctx) {}

const char* nimcp_autocast_op_name(nimcp_autocast_op_t op)
{
    switch (op) {
        case AUTOCAST_OP_MATMUL:      return "MATMUL";
        case AUTOCAST_OP_CONV:        return "CONV";
        case AUTOCAST_OP_ATTENTION:   return "ATTENTION";
        case AUTOCAST_OP_LINEAR:      return "LINEAR";
        case AUTOCAST_OP_EMBEDDING:   return "EMBEDDING";
        case AUTOCAST_OP_SOFTMAX:     return "SOFTMAX";
        case AUTOCAST_OP_LAYERNORM:   return "LAYERNORM";
        case AUTOCAST_OP_BATCHNORM:   return "BATCHNORM";
        case AUTOCAST_OP_GROUPNORM:   return "GROUPNORM";
        case AUTOCAST_OP_LOSS:        return "LOSS";
        case AUTOCAST_OP_REDUCTION:   return "REDUCTION";
        case AUTOCAST_OP_ACTIVATION:  return "ACTIVATION";
        case AUTOCAST_OP_ELEMENTWISE: return "ELEMENTWISE";
        case AUTOCAST_OP_CUSTOM:      return "CUSTOM";
        default:                      return "UNKNOWN";
    }
}

const char* nimcp_autocast_mode_name(nimcp_autocast_mode_t mode)
{
    switch (mode) {
        case AUTOCAST_DISABLED: return "DISABLED";
        case AUTOCAST_FP16:     return "FP16";
        case AUTOCAST_BF16:     return "BF16";
        case AUTOCAST_TF32:     return "TF32";
        default:                return "UNKNOWN";
    }
}

void nimcp_autocast_print_config(const nimcp_autocast_ctx_t* ctx)
{
    printf("Autocast: CUDA not available\n");
}

#endif // NIMCP_ENABLE_CUDA
