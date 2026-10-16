/**
 * @file nimcp_tensor_fp16.cu
 * @brief Mixed Precision (FP16/BF16) CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for half-precision tensor operations
 * WHY:  2-3x speedup on modern GPUs with minimal accuracy loss
 * HOW:  Uses __half/__nv_bfloat16 types and Tensor Core operations
 *
 * IMPLEMENTATION NOTES:
 * - Uses CUDA half-precision intrinsics (__hadd, __hmul, etc.)
 * - Tensor Core support via cuBLAS when available (SM 7.0+)
 * - Fallback to regular FP16 ops on older GPUs
 * - Numerically stable operations compute in FP32, output FP16
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

#include "gpu/tensor/nimcp_tensor_fp16.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "TENSOR_FP16"

// Use recovery macro for cuBLAS instead of simple check
#define CUBLAS_CHECK(call) NIMCP_CUBLAS_RECOVER(call, GPU_ERROR_LIBRARY)

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// For reduction operations
#define REDUCE_BLOCK_SIZE 256

//=============================================================================
// FP16 Conversion Kernels
//=============================================================================

/**
 * @brief Convert FP32 to FP16 kernel
 */
__global__ void kernel_fp32_to_fp16(const float* __restrict__ src,
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
__global__ void kernel_fp16_to_fp32(const __half* __restrict__ src,
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
__global__ void kernel_fp32_to_bf16(const float* __restrict__ src,
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
__global__ void kernel_bf16_to_fp32(const __nv_bfloat16* __restrict__ src,
                                     float* __restrict__ dst,
                                     size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = __bfloat162float(src[idx]);
    }
}

//=============================================================================
// FP16 Element-wise Kernels
//=============================================================================

/**
 * @brief FP16 element-wise addition
 */
__global__ void kernel_add_fp16(const __half* __restrict__ a,
                                 const __half* __restrict__ b,
                                 __half* __restrict__ c,
                                 size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = __hadd(a[idx], b[idx]);
    }
}

/**
 * @brief FP16 element-wise multiplication
 */
__global__ void kernel_mul_fp16(const __half* __restrict__ a,
                                 const __half* __restrict__ b,
                                 __half* __restrict__ c,
                                 size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = __hmul(a[idx], b[idx]);
    }
}

/**
 * @brief FP16 scalar multiplication
 */
__global__ void kernel_scale_fp16(const __half* __restrict__ x,
                                   __half* __restrict__ y,
                                   float scale,
                                   size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        y[idx] = __float2half(__half2float(x[idx]) * scale);
    }
}

/**
 * @brief FP16 fused multiply-add: out = a * b + c
 */
__global__ void kernel_fma_fp16(const __half* __restrict__ a,
                                 const __half* __restrict__ b,
                                 const __half* __restrict__ c,
                                 __half* __restrict__ out,
                                 size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = __hfma(a[idx], b[idx], c[idx]);
    }
}

//=============================================================================
// FP16 Activation Kernels
//=============================================================================

/**
 * @brief FP16 ReLU activation
 */
__global__ void kernel_relu_fp16(const __half* __restrict__ x,
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

/**
 * @brief FP16 GELU activation (approximate version)
 * GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
 */
__global__ void kernel_gelu_fp16(const __half* __restrict__ x,
                                  __half* __restrict__ y,
                                  size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // Compute in FP32 for accuracy, convert back to FP16
        float val = __half2float(x[idx]);
        float val_cubed = val * val * val;
        float inner = 0.7978845608f * (val + 0.044715f * val_cubed);
        float result = 0.5f * val * (1.0f + tanhf(inner));
        y[idx] = __float2half(result);
    }
}

/**
 * @brief FP16 Sigmoid activation
 */
__global__ void kernel_sigmoid_fp16(const __half* __restrict__ x,
                                     __half* __restrict__ y,
                                     size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = __half2float(x[idx]);
        float result = 1.0f / (1.0f + expf(-val));
        y[idx] = __float2half(result);
    }
}

/**
 * @brief FP16 Tanh activation
 */
__global__ void kernel_tanh_fp16(const __half* __restrict__ x,
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
 * @brief FP16 SiLU/Swish activation: x * sigmoid(x)
 */
__global__ void kernel_silu_fp16(const __half* __restrict__ x,
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

//=============================================================================
// Numerically Stable FP16 Operations
//=============================================================================

/**
 * @brief Numerically stable softmax in FP16
 * Computes max and sum in FP32 for stability
 */
__global__ void kernel_softmax_stable_fp16(const __half* __restrict__ x,
                                            __half* __restrict__ y,
                                            int batch,
                                            int dim)
{
    int batch_idx = blockIdx.x;
    if (batch_idx >= batch) return;

    const __half* x_row = x + batch_idx * dim;
    __half* y_row = y + batch_idx * dim;

    // Shared memory for reductions
    extern __shared__ float shared[];
    float* shared_max = shared;
    float* shared_sum = shared + blockDim.x;

    // Step 1: Find max (in FP32 for stability)
    float thread_max = -FLT_MAX;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        thread_max = fmaxf(thread_max, val);
    }
    shared_max[threadIdx.x] = thread_max;
    __syncthreads();

    // Block-level max reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_max[threadIdx.x] = fmaxf(shared_max[threadIdx.x],
                                             shared_max[threadIdx.x + stride]);
        }
        __syncthreads();
    }
    float max_val = shared_max[0];
    __syncthreads();

    // Step 2: Compute exp(x - max) and sum (in FP32)
    float thread_sum = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        float exp_val = expf(val - max_val);
        // Store exp_val temporarily (as FP32 in shared, will convert later)
        thread_sum += exp_val;
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    // Block-level sum reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float sum_val = shared_sum[0];
    __syncthreads();

    // Step 3: Normalize and write output as FP16
    float inv_sum = 1.0f / sum_val;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        float exp_val = expf(val - max_val);
        y_row[i] = __float2half(exp_val * inv_sum);
    }
}

/**
 * @brief FP16 LayerNorm with FP32 accumulation
 */
__global__ void kernel_layernorm_fp16(const __half* __restrict__ x,
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
    float* shared_sum_sq = shared + blockDim.x;

    // Compute mean (in FP32)
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

    // Compute variance (in FP32)
    float thread_var = 0.0f;
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float diff = __half2float(x_row[i]) - mean;
        thread_var += diff * diff;
    }
    shared_sum_sq[threadIdx.x] = thread_var;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum_sq[threadIdx.x] += shared_sum_sq[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float var = shared_sum_sq[0] / (float)dim;
    float inv_std = rsqrtf(var + eps);
    __syncthreads();

    // Normalize and apply affine transform, output FP16
    for (int i = threadIdx.x; i < dim; i += blockDim.x) {
        float val = __half2float(x_row[i]);
        float normalized = (val - mean) * inv_std;
        float g = gamma ? __half2float(gamma[i]) : 1.0f;
        float b = beta ? __half2float(beta[i]) : 0.0f;
        y_row[i] = __float2half(normalized * g + b);
    }
}

/**
 * @brief FP16 BatchNorm kernel
 */
__global__ void kernel_batchnorm_fp16(const __half* __restrict__ x,
                                       const float* __restrict__ mean,
                                       const float* __restrict__ var,
                                       const __half* __restrict__ gamma,
                                       const __half* __restrict__ beta,
                                       __half* __restrict__ y,
                                       int N,
                                       int C,
                                       int HW,
                                       float eps)
{
    // x: [N, C, H*W] - process one element per thread
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = N * C * HW;

    if (idx < total) {
        int c = (idx / HW) % C;

        float val = __half2float(x[idx]);
        float m = mean[c];
        float v = var[c];
        float inv_std = rsqrtf(v + eps);

        float normalized = (val - m) * inv_std;
        float g = gamma ? __half2float(gamma[c]) : 1.0f;
        float b = beta ? __half2float(beta[c]) : 0.0f;

        y[idx] = __float2half(normalized * g + b);
    }
}

//=============================================================================
// Loss Scaling Kernels
//=============================================================================

/**
 * @brief Scale gradients in FP16
 */
__global__ void kernel_scale_gradients_fp16(__half* __restrict__ grads,
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
 * @brief Check for inf/nan in FP16 tensor
 * Uses atomic to set found_inf flag
 */
__global__ void kernel_check_inf_nan_fp16(const __half* __restrict__ data,
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
// Mixed Precision Adam Optimizer Kernel
//=============================================================================

/**
 * @brief Adam update with mixed precision
 * Updates FP32 master weights, syncs to FP16 compute weights
 */
__global__ void kernel_adam_mixed_precision(float* __restrict__ master_weights,
                                             __half* __restrict__ compute_weights,
                                             const __half* __restrict__ gradients,
                                             float* __restrict__ m,
                                             float* __restrict__ v,
                                             float lr,
                                             float beta1,
                                             float beta2,
                                             float eps,
                                             float weight_decay,
                                             int t,
                                             size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // Load values
        float w = master_weights[idx];
        float g = __half2float(gradients[idx]);
        float m_t = m[idx];
        float v_t = v[idx];

        // Apply weight decay (decoupled - AdamW style)
        if (weight_decay > 0.0f) {
            w = w - lr * weight_decay * w;
        }

        // Update biased first moment estimate
        m_t = beta1 * m_t + (1.0f - beta1) * g;

        // Update biased second moment estimate
        v_t = beta2 * v_t + (1.0f - beta2) * g * g;

        // Bias correction
        float m_hat = m_t / (1.0f - powf(beta1, (float)t));
        float v_hat = v_t / (1.0f - powf(beta2, (float)t));

        // Update weight
        w = w - lr * m_hat / (sqrtf(v_hat) + eps);

        // Store updated values
        master_weights[idx] = w;
        m[idx] = m_t;
        v[idx] = v_t;

        // Sync to compute weights (FP16)
        compute_weights[idx] = __float2half(w);
    }
}

/**
 * @brief SGD with momentum, mixed precision
 */
__global__ void kernel_sgd_mixed_precision(float* __restrict__ master_weights,
                                            __half* __restrict__ compute_weights,
                                            const __half* __restrict__ gradients,
                                            float* __restrict__ momentum_buffer,
                                            float lr,
                                            float momentum,
                                            float weight_decay,
                                            bool nesterov,
                                            size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float w = master_weights[idx];
        float g = __half2float(gradients[idx]);
        float buf = momentum_buffer[idx];

        // Apply weight decay
        if (weight_decay > 0.0f) {
            g = g + weight_decay * w;
        }

        // Apply momentum
        if (momentum > 0.0f) {
            buf = momentum * buf + g;
            momentum_buffer[idx] = buf;

            if (nesterov) {
                g = g + momentum * buf;
            } else {
                g = buf;
            }
        }

        // Update weight
        w = w - lr * g;

        // Store
        master_weights[idx] = w;
        compute_weights[idx] = __float2half(w);
    }
}

//=============================================================================
// API Implementations
//=============================================================================

// Helper to get GPU precision enum for FP16
static nimcp_gpu_precision_t mp_dtype_to_gpu_precision(nimcp_mp_dtype_t dtype)
{
    switch (dtype) {
        case MP_DTYPE_FP16: return NIMCP_GPU_PRECISION_FP16;
        case MP_DTYPE_BF16: return NIMCP_GPU_PRECISION_BF16;
        case MP_DTYPE_FP32:
        default: return NIMCP_GPU_PRECISION_FP32;
    }
}

//-----------------------------------------------------------------------------
// Mixed Precision Tensor Lifecycle
//-----------------------------------------------------------------------------

nimcp_mp_tensor_t* nimcp_mp_tensor_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* fp32_tensor,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master)
{
    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !fp32_tensor) {
        LOG_ERROR("Invalid parameters for mp tensor creation");
        return NULL;
    }

    nimcp_mp_tensor_t* tensor = (nimcp_mp_tensor_t*)nimcp_calloc(1, sizeof(nimcp_mp_tensor_t));
    if (!tensor) {
        LOG_ERROR("Failed to allocate mp tensor structure");
        return NULL;
    }

    tensor->compute_dtype = compute_dtype;
    tensor->has_master = keep_master;

    // Create FP16/BF16 compute tensor
    nimcp_gpu_precision_t precision = mp_dtype_to_gpu_precision(compute_dtype);
    tensor->fp16_data = nimcp_gpu_tensor_create(ctx, fp32_tensor->dims,
                                                 fp32_tensor->ndim, precision);
    if (!tensor->fp16_data) {
        nimcp_free(tensor);
        return NULL;
    }
    tensor->owns_compute = true;

    // Convert FP32 to FP16/BF16
    if (compute_dtype == MP_DTYPE_FP16) {
        kernel_fp32_to_fp16<<<GRID_SIZE(fp32_tensor->numel), BLOCK_SIZE>>>(
            (const float*)fp32_tensor->data,
            (__half*)tensor->fp16_data->data,
            fp32_tensor->numel);
    } else if (compute_dtype == MP_DTYPE_BF16) {
        kernel_fp32_to_bf16<<<GRID_SIZE(fp32_tensor->numel), BLOCK_SIZE>>>(
            (const float*)fp32_tensor->data,
            (__nv_bfloat16*)tensor->fp16_data->data,
            fp32_tensor->numel);
    }

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Conversion kernel failed: %s", cudaGetErrorString(err));
        nimcp_gpu_tensor_destroy(tensor->fp16_data);
        nimcp_free(tensor);
        return NULL;
    }

    // Keep master copy if requested
    if (keep_master) {
        tensor->fp32_master = nimcp_gpu_tensor_clone(fp32_tensor);
        if (!tensor->fp32_master) {
            nimcp_gpu_tensor_destroy(tensor->fp16_data);
            nimcp_free(tensor);
            return NULL;
        }
        tensor->owns_master = true;
    }

    LOG_DEBUG("Created mp tensor: numel=%zu, dtype=%d, has_master=%d",
              fp32_tensor->numel, compute_dtype, keep_master);

    return tensor;
}

nimcp_mp_tensor_t* nimcp_mp_tensor_create_empty(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master)
{
    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !dims || ndim == 0) {
        return NULL;
    }

    nimcp_mp_tensor_t* tensor = (nimcp_mp_tensor_t*)nimcp_calloc(1, sizeof(nimcp_mp_tensor_t));
    if (!tensor) return NULL;

    tensor->compute_dtype = compute_dtype;
    tensor->has_master = keep_master;

    nimcp_gpu_precision_t precision = mp_dtype_to_gpu_precision(compute_dtype);
    tensor->fp16_data = nimcp_gpu_tensor_create(ctx, dims, ndim, precision);
    if (!tensor->fp16_data) {
        nimcp_free(tensor);
        return NULL;
    }
    tensor->owns_compute = true;

    if (keep_master) {
        tensor->fp32_master = nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32);
        if (!tensor->fp32_master) {
            nimcp_gpu_tensor_destroy(tensor->fp16_data);
            nimcp_free(tensor);
            return NULL;
        }
        tensor->owns_master = true;
    }

    return tensor;
}

void nimcp_mp_tensor_destroy(nimcp_mp_tensor_t* tensor)
{
    if (!tensor) return;

    if (tensor->owns_compute && tensor->fp16_data) {
        nimcp_gpu_tensor_destroy(tensor->fp16_data);
    }
    if (tensor->owns_master && tensor->fp32_master) {
        nimcp_gpu_tensor_destroy(tensor->fp32_master);
    }
    nimcp_free(tensor);
}

bool nimcp_mp_tensor_sync_compute(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* tensor)
{
    if (!ctx || !tensor || !tensor->fp32_master || !tensor->fp16_data) {
        return false;
    }

    size_t n = tensor->fp32_master->numel;

    if (tensor->compute_dtype == MP_DTYPE_FP16) {
        kernel_fp32_to_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)tensor->fp32_master->data,
            (__half*)tensor->fp16_data->data, n);
    } else if (tensor->compute_dtype == MP_DTYPE_BF16) {
        kernel_fp32_to_bf16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)tensor->fp32_master->data,
            (__nv_bfloat16*)tensor->fp16_data->data, n);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_mp_tensor_sync_master(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* tensor)
{
    if (!ctx || !tensor || !tensor->fp32_master || !tensor->fp16_data) {
        return false;
    }

    size_t n = tensor->fp16_data->numel;

    if (tensor->compute_dtype == MP_DTYPE_FP16) {
        kernel_fp16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __half*)tensor->fp16_data->data,
            (float*)tensor->fp32_master->data, n);
    } else if (tensor->compute_dtype == MP_DTYPE_BF16) {
        kernel_bf16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __nv_bfloat16*)tensor->fp16_data->data,
            (float*)tensor->fp32_master->data, n);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//-----------------------------------------------------------------------------
// Loss Scaler API
//-----------------------------------------------------------------------------

nimcp_loss_scaler_t* nimcp_loss_scaler_create(bool dynamic)
{
    return nimcp_loss_scaler_create_custom(
        MP_DEFAULT_INIT_SCALE,
        MP_DEFAULT_GROWTH_FACTOR,
        MP_DEFAULT_BACKOFF_FACTOR,
        MP_DEFAULT_GROWTH_INTERVAL,
        dynamic
    );
}

nimcp_loss_scaler_t* nimcp_loss_scaler_create_custom(
    float init_scale,
    float growth_factor,
    float backoff_factor,
    int growth_interval,
    bool dynamic)
{
    nimcp_loss_scaler_t* scaler = (nimcp_loss_scaler_t*)nimcp_calloc(1, sizeof(nimcp_loss_scaler_t));
    if (!scaler) return NULL;

    scaler->scale = init_scale;
    scaler->growth_factor = growth_factor;
    scaler->backoff_factor = backoff_factor;
    scaler->growth_interval = growth_interval;
    scaler->min_scale = MP_MIN_SCALE;
    scaler->max_scale = MP_MAX_SCALE;
    scaler->dynamic = dynamic;
    scaler->consecutive_ok = 0;

    return scaler;
}

void nimcp_loss_scaler_destroy(nimcp_loss_scaler_t* scaler)
{
    nimcp_free(scaler);
}

float nimcp_loss_scaler_scale(nimcp_loss_scaler_t* scaler, float loss)
{
    if (!scaler) return loss;
    return loss * scaler->scale;
}

bool nimcp_loss_scaler_unscale(
    nimcp_gpu_context_t* ctx,
    nimcp_loss_scaler_t* scaler,
    nimcp_gpu_tensor_t* gradients)
{
    if (!ctx || !scaler || !gradients) return false;

    float inv_scale = 1.0f / scaler->scale;
    size_t n = gradients->numel;

    // Unscale gradients
    if (gradients->precision == NIMCP_GPU_PRECISION_FP32) {
        // FP32 gradients - use existing scalar multiply
        return nimcp_gpu_mul_scalar(ctx, gradients, inv_scale, gradients);
    }

    return false; // Use unscale_fp16 for FP16 gradients
}

bool nimcp_loss_scaler_unscale_fp16(
    nimcp_gpu_context_t* ctx,
    nimcp_loss_scaler_t* scaler,
    nimcp_gpu_tensor_t* gradients)
{
    if (!ctx || !scaler || !gradients) return false;
    if (gradients->precision != NIMCP_GPU_PRECISION_FP16) return false;

    float inv_scale = 1.0f / scaler->scale;
    size_t n = gradients->numel;

    kernel_scale_gradients_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (__half*)gradients->data, inv_scale, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

void nimcp_loss_scaler_update(nimcp_loss_scaler_t* scaler, bool gradients_valid)
{
    if (!scaler || !scaler->dynamic) return;

    scaler->total_steps++;

    if (gradients_valid) {
        scaler->consecutive_ok++;

        // Grow scale if we've had enough consecutive successful steps
        if (scaler->consecutive_ok >= scaler->growth_interval) {
            float new_scale = scaler->scale * scaler->growth_factor;
            if (new_scale <= scaler->max_scale) {
                scaler->scale = new_scale;
                scaler->scale_increases++;
                LOG_DEBUG("Loss scale increased to %f", scaler->scale);
            }
            scaler->consecutive_ok = 0;
        }
    } else {
        // Overflow detected - reduce scale
        scaler->overflow_count++;
        scaler->consecutive_ok = 0;

        float new_scale = scaler->scale * scaler->backoff_factor;
        if (new_scale >= scaler->min_scale) {
            scaler->scale = new_scale;
            scaler->scale_decreases++;
            LOG_DEBUG("Loss scale decreased to %f", scaler->scale);
        } else {
            LOG_WARN("Loss scale at minimum (%f), cannot reduce further", scaler->min_scale);
        }
    }
}

float nimcp_loss_scaler_get_scale(const nimcp_loss_scaler_t* scaler)
{
    return scaler ? scaler->scale : 1.0f;
}

bool nimcp_loss_scaler_should_skip(const nimcp_loss_scaler_t* scaler, bool gradients_valid)
{
    return scaler && !gradients_valid;
}

//-----------------------------------------------------------------------------
// AMP Context API
//-----------------------------------------------------------------------------

nimcp_amp_context_t* nimcp_amp_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_mp_dtype_t compute_dtype,
    bool enable_scaler)
{
    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx) return NULL;

    nimcp_amp_context_t* ctx = (nimcp_amp_context_t*)nimcp_calloc(1, sizeof(nimcp_amp_context_t));
    if (!ctx) return NULL;

    ctx->gpu_ctx = gpu_ctx;
    ctx->default_dtype = compute_dtype;
    ctx->enabled = true;
    ctx->autocasting = false;

    // Set per-op dtypes
    ctx->matmul_dtype = compute_dtype;
    ctx->conv_dtype = compute_dtype;
    ctx->norm_dtype = MP_DTYPE_FP32;      // Always FP32 for stability
    ctx->softmax_dtype = MP_DTYPE_FP32;   // Always FP32 for stability
    ctx->loss_dtype = MP_DTYPE_FP32;      // Always FP32

    // Create loss scaler if requested
    if (enable_scaler) {
        ctx->scaler = nimcp_loss_scaler_create(true);
    }

    // Detect hardware capabilities
    ctx->compute_capability_major = gpu_ctx->device_info.compute_capability_major;
    ctx->compute_capability_minor = gpu_ctx->device_info.compute_capability_minor;
    ctx->tensor_cores_available = (ctx->compute_capability_major >= 7);
    ctx->bf16_supported = (ctx->compute_capability_major >= 8);

    LOG_INFO("AMP context created: dtype=%d, tensor_cores=%d, bf16=%d",
             compute_dtype, ctx->tensor_cores_available, ctx->bf16_supported);

    return ctx;
}

void nimcp_amp_destroy(nimcp_amp_context_t* ctx)
{
    if (!ctx) return;
    if (ctx->scaler) {
        nimcp_loss_scaler_destroy(ctx->scaler);
    }
    nimcp_free(ctx);
}

bool nimcp_amp_autocast_enter(nimcp_amp_context_t* ctx)
{
    if (!ctx) return false;
    ctx->autocasting = true;
    return true;
}

bool nimcp_amp_autocast_exit(nimcp_amp_context_t* ctx)
{
    if (!ctx) return false;
    ctx->autocasting = false;
    return true;
}

bool nimcp_amp_is_autocasting(const nimcp_amp_context_t* ctx)
{
    return ctx && ctx->autocasting;
}

nimcp_mp_dtype_t nimcp_amp_get_dtype(const nimcp_amp_context_t* ctx,
                                      nimcp_mp_op_category_t category)
{
    if (!ctx || !ctx->autocasting) return MP_DTYPE_FP32;

    switch (category) {
        case MP_OP_COMPUTE:
            return ctx->matmul_dtype;
        case MP_OP_REDUCE:
        case MP_OP_NORMALIZE:
            return MP_DTYPE_FP32;  // Always FP32 for stability
        case MP_OP_ELEMENTWISE:
            return ctx->default_dtype;
        case MP_OP_PRESERVE:
        default:
            return MP_DTYPE_FP32;
    }
}

nimcp_gpu_tensor_t* nimcp_amp_cast_tensor(
    nimcp_amp_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    nimcp_mp_op_category_t category)
{
    if (!ctx || !tensor) return tensor;

    nimcp_mp_dtype_t target_dtype = nimcp_amp_get_dtype(ctx, category);

    // Check if cast is needed
    nimcp_gpu_precision_t current = tensor->precision;
    nimcp_gpu_precision_t target = mp_dtype_to_gpu_precision(target_dtype);

    if (current == target) {
        return tensor;  // No cast needed
    }

    // Create new tensor with target precision
    nimcp_gpu_tensor_t* casted = nimcp_gpu_tensor_create(
        ctx->gpu_ctx, tensor->dims, tensor->ndim, target);
    if (!casted) return tensor;

    // Perform conversion
    bool success = false;
    size_t n = tensor->numel;

    if (current == NIMCP_GPU_PRECISION_FP32 && target == NIMCP_GPU_PRECISION_FP16) {
        kernel_fp32_to_fp16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)tensor->data, (__half*)casted->data, n);
        success = true;
    } else if (current == NIMCP_GPU_PRECISION_FP16 && target == NIMCP_GPU_PRECISION_FP32) {
        kernel_fp16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __half*)tensor->data, (float*)casted->data, n);
        success = true;
    } else if (current == NIMCP_GPU_PRECISION_FP32 && target == NIMCP_GPU_PRECISION_BF16) {
        kernel_fp32_to_bf16<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const float*)tensor->data, (__nv_bfloat16*)casted->data, n);
        success = true;
    } else if (current == NIMCP_GPU_PRECISION_BF16 && target == NIMCP_GPU_PRECISION_FP32) {
        kernel_bf16_to_fp32<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (const __nv_bfloat16*)tensor->data, (float*)casted->data, n);
        success = true;
    }

    if (!success || cudaGetLastError() != cudaSuccess) {
        nimcp_gpu_tensor_destroy(casted);
        return tensor;
    }

    // Update stats
    if (target == NIMCP_GPU_PRECISION_FP16 || target == NIMCP_GPU_PRECISION_BF16) {
        ctx->fp16_ops++;
    } else {
        ctx->fp32_ops++;
    }

    return casted;
}

//-----------------------------------------------------------------------------
// FP16 Conversion API
//-----------------------------------------------------------------------------

bool nimcp_fp32_to_fp16(nimcp_gpu_context_t* ctx,
                        const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst)
{
    if (!ctx || !src || !dst) return false;
    if (src->numel != dst->numel) return false;

    kernel_fp32_to_fp16<<<GRID_SIZE(src->numel), BLOCK_SIZE>>>(
        (const float*)src->data, (__half*)dst->data, src->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_to_fp32(nimcp_gpu_context_t* ctx,
                        const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst)
{
    if (!ctx || !src || !dst) return false;
    if (src->numel != dst->numel) return false;

    kernel_fp16_to_fp32<<<GRID_SIZE(src->numel), BLOCK_SIZE>>>(
        (const __half*)src->data, (float*)dst->data, src->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp32_to_bf16(nimcp_gpu_context_t* ctx,
                        const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst)
{
    if (!ctx || !src || !dst) return false;
    if (src->numel != dst->numel) return false;

    kernel_fp32_to_bf16<<<GRID_SIZE(src->numel), BLOCK_SIZE>>>(
        (const float*)src->data, (__nv_bfloat16*)dst->data, src->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_bf16_to_fp32(nimcp_gpu_context_t* ctx,
                        const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst)
{
    if (!ctx || !src || !dst) return false;
    if (src->numel != dst->numel) return false;

    kernel_bf16_to_fp32<<<GRID_SIZE(src->numel), BLOCK_SIZE>>>(
        (const __nv_bfloat16*)src->data, (float*)dst->data, src->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//-----------------------------------------------------------------------------
// FP16 Element-wise Operations API
//-----------------------------------------------------------------------------

bool nimcp_fp16_add(nimcp_gpu_context_t* ctx,
                    const nimcp_gpu_tensor_t* a,
                    const nimcp_gpu_tensor_t* b,
                    nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    if (a->numel != b->numel || a->numel != out->numel) return false;

    kernel_add_fp16<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const __half*)a->data, (const __half*)b->data,
        (__half*)out->data, a->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_mul(nimcp_gpu_context_t* ctx,
                    const nimcp_gpu_tensor_t* a,
                    const nimcp_gpu_tensor_t* b,
                    nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    if (a->numel != b->numel || a->numel != out->numel) return false;

    kernel_mul_fp16<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const __half*)a->data, (const __half*)b->data,
        (__half*)out->data, a->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_scale(nimcp_gpu_context_t* ctx,
                      const nimcp_gpu_tensor_t* x,
                      float scale,
                      nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    if (x->numel != out->numel) return false;

    kernel_scale_fp16<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const __half*)x->data, (__half*)out->data, scale, x->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_fma(nimcp_gpu_context_t* ctx,
                    const nimcp_gpu_tensor_t* a,
                    const nimcp_gpu_tensor_t* b,
                    const nimcp_gpu_tensor_t* c,
                    nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !c || !out) return false;
    if (a->numel != b->numel || a->numel != c->numel || a->numel != out->numel) return false;

    kernel_fma_fp16<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const __half*)a->data, (const __half*)b->data,
        (const __half*)c->data, (__half*)out->data, a->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//-----------------------------------------------------------------------------
// FP16 GEMM Operations API
//-----------------------------------------------------------------------------

bool nimcp_fp16_gemm(nimcp_gpu_context_t* ctx,
                     const nimcp_gpu_tensor_t* A,
                     const nimcp_gpu_tensor_t* B,
                     nimcp_gpu_tensor_t* C,
                     float alpha,
                     float beta,
                     bool trans_a,
                     bool trans_b)
{
    if (!ctx || !A || !B || !C) return false;

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    if (!handle) {
        LOG_ERROR("cuBLAS handle not initialized");
        return false;
    }

    // Get dimensions
    int M = trans_a ? A->dims[A->ndim - 1] : A->dims[A->ndim - 2];
    int K_a = trans_a ? A->dims[A->ndim - 2] : A->dims[A->ndim - 1];
    int K_b = trans_b ? B->dims[B->ndim - 1] : B->dims[B->ndim - 2];
    int N = trans_b ? B->dims[B->ndim - 2] : B->dims[B->ndim - 1];

    if (K_a != K_b) {
        LOG_ERROR("GEMM dimension mismatch: K_a=%d, K_b=%d", K_a, K_b);
        return false;
    }

    cublasOperation_t opA = trans_a ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasOperation_t opB = trans_b ? CUBLAS_OP_T : CUBLAS_OP_N;

    // Use half precision GEMM (cublasHgemm) if available
    // Note: cuBLAS uses column-major, so we swap A and B
    __half alpha_half = __float2half(alpha);
    __half beta_half = __float2half(beta);

    CUBLAS_CHECK(cublasHgemm(handle,
        opB, opA,
        N, M, K_a,
        &alpha_half,
        (const __half*)B->data, trans_b ? K_a : N,
        (const __half*)A->data, trans_a ? M : K_a,
        &beta_half,
        (__half*)C->data, N));

    return true;
}

bool nimcp_fp16_gemm_batched(nimcp_gpu_context_t* ctx,
                              const nimcp_gpu_tensor_t* A,
                              const nimcp_gpu_tensor_t* B,
                              nimcp_gpu_tensor_t* C,
                              float alpha,
                              float beta,
                              bool trans_a,
                              bool trans_b)
{
    if (!ctx || !A || !B || !C) return false;
    if (A->ndim < 3 || B->ndim < 3) return false;

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;

    int batch = A->dims[0];
    int M = trans_a ? A->dims[2] : A->dims[1];
    int K = trans_a ? A->dims[1] : A->dims[2];
    int N = trans_b ? B->dims[1] : B->dims[2];

    cublasOperation_t opA = trans_a ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasOperation_t opB = trans_b ? CUBLAS_OP_T : CUBLAS_OP_N;

    __half alpha_half = __float2half(alpha);
    __half beta_half = __float2half(beta);

    long long strideA = M * K;
    long long strideB = K * N;
    long long strideC = M * N;

    CUBLAS_CHECK(cublasHgemmStridedBatched(handle,
        opB, opA,
        N, M, K,
        &alpha_half,
        (const __half*)B->data, trans_b ? K : N, strideB,
        (const __half*)A->data, trans_a ? M : K, strideA,
        &beta_half,
        (__half*)C->data, N, strideC,
        batch));

    return true;
}

//-----------------------------------------------------------------------------
// FP16 Activation Functions API
//-----------------------------------------------------------------------------

bool nimcp_fp16_relu(nimcp_gpu_context_t* ctx,
                     const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;

    kernel_relu_fp16<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const __half*)x->data, (__half*)out->data, x->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_gelu(nimcp_gpu_context_t* ctx,
                     const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;

    kernel_gelu_fp16<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const __half*)x->data, (__half*)out->data, x->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_sigmoid(nimcp_gpu_context_t* ctx,
                        const nimcp_gpu_tensor_t* x,
                        nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;

    kernel_sigmoid_fp16<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const __half*)x->data, (__half*)out->data, x->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_tanh(nimcp_gpu_context_t* ctx,
                     const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;

    kernel_tanh_fp16<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const __half*)x->data, (__half*)out->data, x->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_silu(nimcp_gpu_context_t* ctx,
                     const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;

    kernel_silu_fp16<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const __half*)x->data, (__half*)out->data, x->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//-----------------------------------------------------------------------------
// Numerically Stable Operations API
//-----------------------------------------------------------------------------

bool nimcp_fp16_softmax_stable(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* x,
                                nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    if (x->ndim < 1) return false;

    int dim = x->dims[x->ndim - 1];
    int batch = x->numel / dim;

    // Shared memory: 2 * blockDim.x floats
    size_t shared_mem = 2 * BLOCK_SIZE * sizeof(float);

    kernel_softmax_stable_fp16<<<batch, BLOCK_SIZE, shared_mem>>>(
        (const __half*)x->data, (__half*)out->data, batch, dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_layernorm(nimcp_gpu_context_t* ctx,
                           const nimcp_gpu_tensor_t* x,
                           const nimcp_gpu_tensor_t* gamma,
                           const nimcp_gpu_tensor_t* beta,
                           nimcp_gpu_tensor_t* out,
                           float eps)
{
    if (!ctx || !x || !out) return false;
    if (x->ndim < 1) return false;

    int dim = x->dims[x->ndim - 1];
    int batch = x->numel / dim;

    // Shared memory: 2 * blockDim.x floats
    size_t shared_mem = 2 * BLOCK_SIZE * sizeof(float);

    kernel_layernorm_fp16<<<batch, BLOCK_SIZE, shared_mem>>>(
        (const __half*)x->data,
        gamma ? (const __half*)gamma->data : NULL,
        beta ? (const __half*)beta->data : NULL,
        (__half*)out->data, batch, dim, eps);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_batchnorm(nimcp_gpu_context_t* ctx,
                           const nimcp_gpu_tensor_t* x,
                           const nimcp_gpu_tensor_t* running_mean,
                           const nimcp_gpu_tensor_t* running_var,
                           const nimcp_gpu_tensor_t* gamma,
                           const nimcp_gpu_tensor_t* beta,
                           nimcp_gpu_tensor_t* out,
                           float eps)
{
    if (!ctx || !x || !running_mean || !running_var || !out) return false;
    if (x->ndim < 3) return false;

    int N = x->dims[0];
    int C = x->dims[1];
    int HW = 1;
    for (uint32_t i = 2; i < x->ndim; i++) {
        HW *= x->dims[i];
    }

    int total = N * C * HW;

    kernel_batchnorm_fp16<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (const __half*)x->data,
        (const float*)running_mean->data,
        (const float*)running_var->data,
        gamma ? (const __half*)gamma->data : NULL,
        beta ? (const __half*)beta->data : NULL,
        (__half*)out->data, N, C, HW, eps);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//-----------------------------------------------------------------------------
// Loss Scaling Kernels API
//-----------------------------------------------------------------------------

bool nimcp_fp16_scale_gradients(nimcp_gpu_context_t* ctx,
                                 nimcp_gpu_tensor_t* grads,
                                 float scale)
{
    if (!ctx || !grads) return false;

    kernel_scale_gradients_fp16<<<GRID_SIZE(grads->numel), BLOCK_SIZE>>>(
        (__half*)grads->data, scale, grads->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_fp16_check_inf_nan(nimcp_gpu_context_t* ctx,
                               const nimcp_gpu_tensor_t* data,
                               int* found_inf)
{
    if (!ctx || !data || !found_inf) return false;

    // Allocate device memory for flag
    int* d_found_inf;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_found_inf, sizeof(int)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_found_inf, 0, sizeof(int)), GPU_ERROR_CUDA_RUNTIME);

    kernel_check_inf_nan_fp16<<<GRID_SIZE(data->numel), BLOCK_SIZE>>>(
        (const __half*)data->data, d_found_inf, data->numel);

    NIMCP_CUDA_RECOVER(cudaMemcpy(found_inf, d_found_inf, sizeof(int), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaFree(d_found_inf), GPU_ERROR_CUDA_RUNTIME);

    return true;
}

//-----------------------------------------------------------------------------
// Mixed Precision Optimizer Support API
//-----------------------------------------------------------------------------

bool nimcp_mp_adam_update(nimcp_gpu_context_t* ctx,
                          nimcp_mp_tensor_t* mp_tensor,
                          const nimcp_gpu_tensor_t* gradients,
                          nimcp_gpu_tensor_t* m,
                          nimcp_gpu_tensor_t* v,
                          float lr,
                          float beta1,
                          float beta2,
                          float eps,
                          float weight_decay,
                          int step)
{
    if (!ctx || !mp_tensor || !gradients || !m || !v) return false;
    if (!mp_tensor->has_master) {
        LOG_ERROR("Mixed precision Adam requires master weights");
        return false;
    }

    size_t n = mp_tensor->fp32_master->numel;

    kernel_adam_mixed_precision<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)mp_tensor->fp32_master->data,
        (__half*)mp_tensor->fp16_data->data,
        (const __half*)gradients->data,
        (float*)m->data,
        (float*)v->data,
        lr, beta1, beta2, eps, weight_decay, step, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_mp_sgd_update(nimcp_gpu_context_t* ctx,
                         nimcp_mp_tensor_t* mp_tensor,
                         const nimcp_gpu_tensor_t* gradients,
                         nimcp_gpu_tensor_t* momentum_buffer,
                         float lr,
                         float momentum,
                         float weight_decay,
                         bool nesterov)
{
    if (!ctx || !mp_tensor || !gradients) return false;
    if (!mp_tensor->has_master) {
        LOG_ERROR("Mixed precision SGD requires master weights");
        return false;
    }

    size_t n = mp_tensor->fp32_master->numel;

    kernel_sgd_mixed_precision<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)mp_tensor->fp32_master->data,
        (__half*)mp_tensor->fp16_data->data,
        (const __half*)gradients->data,
        momentum_buffer ? (float*)momentum_buffer->data : NULL,
        lr, momentum, weight_decay, nesterov, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//-----------------------------------------------------------------------------
// Hardware Capability Detection API
//-----------------------------------------------------------------------------

bool nimcp_tensor_cores_available(nimcp_gpu_context_t* ctx)
{
    if (!ctx) return false;
    return ctx->device_info.compute_capability_major >= 7;
}

bool nimcp_bf16_supported(nimcp_gpu_context_t* ctx)
{
    if (!ctx) return false;
    return ctx->device_info.compute_capability_major >= 8;
}

nimcp_mp_dtype_t nimcp_get_recommended_dtype(nimcp_gpu_context_t* ctx)
{
    if (!ctx) return MP_DTYPE_FP32;

    // Ampere+ (SM 8.0+): Prefer BF16 for better range
    if (ctx->device_info.compute_capability_major >= 8) {
        return MP_DTYPE_BF16;
    }
    // Volta/Turing (SM 7.0+): Use FP16
    else if (ctx->device_info.compute_capability_major >= 7) {
        return MP_DTYPE_FP16;
    }
    // Older GPUs: FP32 only
    return MP_DTYPE_FP32;
}

void nimcp_amp_get_stats(const nimcp_amp_context_t* ctx,
                          uint64_t* fp16_ops,
                          uint64_t* fp32_ops,
                          uint64_t* tc_ops)
{
    if (!ctx) return;
    if (fp16_ops) *fp16_ops = ctx->fp16_ops;
    if (fp32_ops) *fp32_ops = ctx->fp32_ops;
    if (tc_ops) *tc_ops = ctx->tensor_core_ops;
}

const char* nimcp_mp_dtype_name(nimcp_mp_dtype_t dtype)
{
    switch (dtype) {
        case MP_DTYPE_FP32: return "FP32";
        case MP_DTYPE_FP16: return "FP16";
        case MP_DTYPE_BF16: return "BF16";
        case MP_DTYPE_TF32: return "TF32";
        default: return "UNKNOWN";
    }
}

size_t nimcp_mp_dtype_size(nimcp_mp_dtype_t dtype)
{
    switch (dtype) {
        case MP_DTYPE_FP32: return 4;
        case MP_DTYPE_FP16: return 2;
        case MP_DTYPE_BF16: return 2;
        case MP_DTYPE_TF32: return 4;
        default: return 4;
    }
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/tensor/nimcp_tensor_fp16.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>

#define LOG_MODULE "TENSOR_FP16"

nimcp_mp_tensor_t* nimcp_mp_tensor_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* fp32_tensor,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master)
{
    LOG_WARN("CUDA not available - mixed precision tensors require GPU");
    return NULL;
}

nimcp_mp_tensor_t* nimcp_mp_tensor_create_empty(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master)
{
    return NULL;
}

void nimcp_mp_tensor_destroy(nimcp_mp_tensor_t* tensor) {}
bool nimcp_mp_tensor_sync_compute(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* tensor) { return false; }
bool nimcp_mp_tensor_sync_master(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* tensor) { return false; }

nimcp_loss_scaler_t* nimcp_loss_scaler_create(bool dynamic) { return NULL; }
nimcp_loss_scaler_t* nimcp_loss_scaler_create_custom(
    float init_scale, float growth_factor, float backoff_factor,
    int growth_interval, bool dynamic) { return NULL; }
void nimcp_loss_scaler_destroy(nimcp_loss_scaler_t* scaler) {}
float nimcp_loss_scaler_scale(nimcp_loss_scaler_t* scaler, float loss) { return loss; }
bool nimcp_loss_scaler_unscale(nimcp_gpu_context_t* ctx, nimcp_loss_scaler_t* scaler,
                                nimcp_gpu_tensor_t* gradients) { return false; }
bool nimcp_loss_scaler_unscale_fp16(nimcp_gpu_context_t* ctx, nimcp_loss_scaler_t* scaler,
                                     nimcp_gpu_tensor_t* gradients) { return false; }
void nimcp_loss_scaler_update(nimcp_loss_scaler_t* scaler, bool gradients_valid) {}
float nimcp_loss_scaler_get_scale(const nimcp_loss_scaler_t* scaler) { return 1.0f; }
bool nimcp_loss_scaler_should_skip(const nimcp_loss_scaler_t* scaler, bool gradients_valid) { return false; }

nimcp_amp_context_t* nimcp_amp_create(nimcp_gpu_context_t* gpu_ctx,
                                       nimcp_mp_dtype_t compute_dtype,
                                       bool enable_scaler) { return NULL; }
void nimcp_amp_destroy(nimcp_amp_context_t* ctx) {}
bool nimcp_amp_autocast_enter(nimcp_amp_context_t* ctx) { return false; }
bool nimcp_amp_autocast_exit(nimcp_amp_context_t* ctx) { return false; }
bool nimcp_amp_is_autocasting(const nimcp_amp_context_t* ctx) { return false; }
nimcp_mp_dtype_t nimcp_amp_get_dtype(const nimcp_amp_context_t* ctx,
                                      nimcp_mp_op_category_t category) { return MP_DTYPE_FP32; }
nimcp_gpu_tensor_t* nimcp_amp_cast_tensor(nimcp_amp_context_t* ctx,
                                           nimcp_gpu_tensor_t* tensor,
                                           nimcp_mp_op_category_t category) { return tensor; }

bool nimcp_fp32_to_fp16(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst) { return false; }
bool nimcp_fp16_to_fp32(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst) { return false; }
bool nimcp_fp32_to_bf16(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst) { return false; }
bool nimcp_bf16_to_fp32(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src,
                        nimcp_gpu_tensor_t* dst) { return false; }

bool nimcp_fp16_add(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                    const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_mul(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                    const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_scale(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                      float scale, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_fma(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                    const nimcp_gpu_tensor_t* b, const nimcp_gpu_tensor_t* c,
                    nimcp_gpu_tensor_t* out) { return false; }

bool nimcp_fp16_gemm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                     const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                     float alpha, float beta, bool trans_a, bool trans_b) { return false; }
bool nimcp_fp16_gemm_batched(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                              const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                              float alpha, float beta, bool trans_a, bool trans_b) { return false; }

bool nimcp_fp16_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_gelu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_sigmoid(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                        nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_tanh(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_silu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                     nimcp_gpu_tensor_t* out) { return false; }

bool nimcp_fp16_softmax_stable(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                                nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_fp16_layernorm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                           const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* beta,
                           nimcp_gpu_tensor_t* out, float eps) { return false; }
bool nimcp_fp16_batchnorm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                           const nimcp_gpu_tensor_t* running_mean,
                           const nimcp_gpu_tensor_t* running_var,
                           const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* beta,
                           nimcp_gpu_tensor_t* out, float eps) { return false; }

bool nimcp_fp16_scale_gradients(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* grads,
                                 float scale) { return false; }
bool nimcp_fp16_check_inf_nan(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* data,
                               int* found_inf) { return false; }

bool nimcp_mp_adam_update(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* mp_tensor,
                          const nimcp_gpu_tensor_t* gradients, nimcp_gpu_tensor_t* m,
                          nimcp_gpu_tensor_t* v, float lr, float beta1, float beta2,
                          float eps, float weight_decay, int step) { return false; }
bool nimcp_mp_sgd_update(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* mp_tensor,
                         const nimcp_gpu_tensor_t* gradients,
                         nimcp_gpu_tensor_t* momentum_buffer, float lr, float momentum,
                         float weight_decay, bool nesterov) { return false; }

bool nimcp_tensor_cores_available(nimcp_gpu_context_t* ctx) { return false; }
bool nimcp_bf16_supported(nimcp_gpu_context_t* ctx) { return false; }
nimcp_mp_dtype_t nimcp_get_recommended_dtype(nimcp_gpu_context_t* ctx) { return MP_DTYPE_FP32; }
void nimcp_amp_get_stats(const nimcp_amp_context_t* ctx, uint64_t* fp16_ops,
                          uint64_t* fp32_ops, uint64_t* tc_ops) {}

const char* nimcp_mp_dtype_name(nimcp_mp_dtype_t dtype)
{
    switch (dtype) {
        case MP_DTYPE_FP32: return "FP32";
        case MP_DTYPE_FP16: return "FP16";
        case MP_DTYPE_BF16: return "BF16";
        case MP_DTYPE_TF32: return "TF32";
        default: return "UNKNOWN";
    }
}

size_t nimcp_mp_dtype_size(nimcp_mp_dtype_t dtype)
{
    switch (dtype) {
        case MP_DTYPE_FP32: return 4;
        case MP_DTYPE_FP16: return 2;
        case MP_DTYPE_BF16: return 2;
        case MP_DTYPE_TF32: return 4;
        default: return 4;
    }
}

#endif // NIMCP_ENABLE_CUDA
