/**
 * @file nimcp_tensor_kernels.cu
 * @brief GPU Tensor CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for tensor operations
 * WHY:  GPU acceleration for neural network computations
 * HOW:  cuBLAS for GEMM, custom kernels for element-wise and reductions
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cuda_fp16.h>
#include <math.h>
#include <float.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "TENSOR_GPU"

// Use recovery macro for cuBLAS instead of simple check
#define CUBLAS_CHECK(call) NIMCP_CUBLAS_RECOVER(call, GPU_ERROR_LIBRARY)

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32

#ifndef NIMCP_EPS
#define NIMCP_EPS 1e-7f
#endif

// Calculate grid size for N elements
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Helper Functions
//=============================================================================

static size_t get_elem_size(nimcp_gpu_precision_t precision)
{
    switch (precision) {
        case NIMCP_GPU_PRECISION_FP32: return sizeof(float);
        case NIMCP_GPU_PRECISION_FP16: return sizeof(__half);
        case NIMCP_GPU_PRECISION_BF16: return sizeof(__nv_bfloat16);
        case NIMCP_GPU_PRECISION_INT8: return sizeof(int8_t);
        case NIMCP_GPU_PRECISION_TF32: return sizeof(float);
        case NIMCP_GPU_PRECISION_UINT32: return sizeof(uint32_t);
        case NIMCP_GPU_PRECISION_INT32: return sizeof(int32_t);
        default: return sizeof(float);
    }
}

static size_t compute_numel(const size_t* dims, uint32_t ndim)
{
    if (!dims || ndim == 0) return 0;
    size_t numel = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        /* P3-T2: Overflow check - detect multiplication overflow */
        if (dims[i] != 0 && numel > SIZE_MAX / dims[i]) {
            return 0; /* Overflow would occur */
        }
        numel *= dims[i];
    }
    return numel;
}

//=============================================================================
// Tensor Lifecycle
//=============================================================================

nimcp_gpu_tensor_t* nimcp_gpu_tensor_create(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_gpu_precision_t precision)
{
    // Initialize GPU recovery system if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !dims || ndim == 0) {
        LOG_ERROR("Invalid parameters for tensor creation");
        return NULL;
    }

    nimcp_gpu_tensor_t* tensor = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
    if (!tensor) {
        LOG_ERROR("Failed to allocate tensor structure");
        return NULL;
    }

    tensor->ndim = ndim;
    tensor->precision = precision;
    tensor->layout = NIMCP_TENSOR_LAYOUT_ROW_MAJOR;
    tensor->ctx = ctx;
    tensor->owns_data = true;
    tensor->elem_size = get_elem_size(precision);

    // Allocate dimensions array on host
    tensor->dims = (size_t*)nimcp_malloc(ndim * sizeof(size_t));
    tensor->strides = (size_t*)nimcp_malloc(ndim * sizeof(size_t));
    if (!tensor->dims || !tensor->strides) {
        LOG_ERROR("Failed to allocate dimension arrays");
        nimcp_free(tensor->dims);
        nimcp_free(tensor->strides);
        nimcp_free(tensor);
        return NULL;
    }

    // Check for overflow before computing strides
    size_t safe_numel = compute_numel(dims, ndim);
    if (safe_numel == 0 && ndim > 0) {
        LOG_ERROR("Dimension overflow in tensor creation");
        nimcp_free(tensor->dims);
        nimcp_free(tensor->strides);
        nimcp_free(tensor);
        return NULL;
    }

    // Copy dimensions and compute strides (row-major)
    // Use safe_numel (overflow-checked) instead of recomputing
    tensor->numel = safe_numel;
    size_t stride_accum = 1;
    for (int i = ndim - 1; i >= 0; i--) {
        tensor->dims[i] = dims[i];
        tensor->strides[i] = stride_accum;
        stride_accum *= dims[i];
    }

    // Allocate device memory with recovery support
    size_t data_size = tensor->numel * tensor->elem_size;
    cudaError_t err = cudaMalloc(&tensor->data, data_size);
    if (err != cudaSuccess) {
        // Attempt recovery
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&tensor->data, data_size);
        }
        if (err != cudaSuccess) {
            LOG_ERROR("Failed to allocate %zu bytes on GPU: %s", data_size, cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, err,
                "GPU memory allocation failed (unrecoverable): %zu bytes", data_size);
            nimcp_free(tensor->dims);
            nimcp_free(tensor->strides);
            nimcp_free(tensor);
            return NULL;
        }
    }

    // Zero-initialize
    cudaMemset(tensor->data, 0, data_size);

    LOG_DEBUG("Created GPU tensor: %u dims, %zu elements, %zu bytes", ndim, tensor->numel, data_size);
    return tensor;
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_host(
    nimcp_gpu_context_t* ctx,
    const void* host_data,
    const size_t* dims,
    uint32_t ndim,
    nimcp_gpu_precision_t precision)
{
    if (!host_data) {
        LOG_ERROR("NULL host data provided");
        return NULL;
    }

    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, precision);
    if (!tensor) {
        return NULL;
    }

    size_t data_size = tensor->numel * tensor->elem_size;
    cudaError_t err = cudaMemcpy(tensor->data, host_data, data_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        // Attempt recovery
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &result)) {
            err = cudaMemcpy(tensor->data, host_data, data_size, cudaMemcpyHostToDevice);
        }
        if (err != cudaSuccess) {
            LOG_ERROR("Failed to copy data to GPU: %s", cudaGetErrorString(err));
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err,
                "GPU memcpy failed (unrecoverable): %s", cudaGetErrorString(err));
            nimcp_gpu_tensor_destroy(tensor);
            return NULL;
        }
    }

    return tensor;
}

bool nimcp_gpu_tensor_upload(nimcp_gpu_tensor_t* tensor, const void* host_data)
{
    if (!tensor || !host_data || !tensor->data) {
        LOG_ERROR("Invalid parameters for tensor upload");
        return false;
    }
    size_t data_size = tensor->numel * tensor->elem_size;
    cudaError_t err = cudaMemcpy(tensor->data, host_data, data_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to upload tensor data to GPU: %s", cudaGetErrorString(err));
        NIMCP_CUDA_RECOVER(err, GPU_ERROR_CUDA_RUNTIME);
        return false;
    }
    return true;
}

bool nimcp_gpu_tensor_to_host(const nimcp_gpu_tensor_t* tensor, void* host_data)
{
    if (!tensor || !host_data) {
        LOG_ERROR("Invalid parameters for tensor to host copy");
        return false;
    }

    size_t data_size = tensor->numel * tensor->elem_size;
    cudaError_t err = cudaMemcpy(host_data, tensor->data, data_size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy tensor data to host: %s", cudaGetErrorString(err));
        NIMCP_CUDA_RECOVER(err, GPU_ERROR_CUDA_RUNTIME);
        return false;
    }
    return true;
}

void nimcp_gpu_tensor_destroy(nimcp_gpu_tensor_t* tensor)
{
    if (!tensor) return;

    if (tensor->owns_data && tensor->data) {
        cudaFree(tensor->data);
    }

    nimcp_free(tensor->dims);
    nimcp_free(tensor->strides);
    nimcp_free(tensor);
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_clone(const nimcp_gpu_tensor_t* tensor)
{
    if (!tensor) return NULL;

    nimcp_gpu_tensor_t* clone = nimcp_gpu_tensor_create(
        tensor->ctx, tensor->dims, tensor->ndim, tensor->precision);
    if (!clone) return NULL;

    size_t data_size = tensor->numel * tensor->elem_size;
    cudaError_t err = cudaMemcpy(clone->data, tensor->data, data_size, cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        // Attempt recovery
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &result)) {
            err = cudaMemcpy(clone->data, tensor->data, data_size, cudaMemcpyDeviceToDevice);
        }
        if (err != cudaSuccess) {
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, err,
                "GPU memcpy failed during clone (unrecoverable): %s", cudaGetErrorString(err));
            nimcp_gpu_tensor_destroy(clone);
            return NULL;
        }
    }

    return clone;
}

//=============================================================================
// GEMM Operations (using cuBLAS)
//=============================================================================

bool nimcp_gpu_gemm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b)
{
    if (!ctx || !A || !B || !C) {
        LOG_ERROR("Invalid parameters for GEMM");
        return false;
    }

    if (A->ndim < 2 || B->ndim < 2 || C->ndim < 2) {
        LOG_ERROR("GEMM requires at least 2D tensors");
        return false;
    }

    // Get dimensions (assuming last two dims are matrix dims)
    int M = trans_a ? A->dims[A->ndim - 1] : A->dims[A->ndim - 2];
    int K_a = trans_a ? A->dims[A->ndim - 2] : A->dims[A->ndim - 1];
    int K_b = trans_b ? B->dims[B->ndim - 1] : B->dims[B->ndim - 2];
    int N = trans_b ? B->dims[B->ndim - 2] : B->dims[B->ndim - 1];

    if (K_a != K_b) {
        LOG_ERROR("GEMM dimension mismatch: K_a=%d, K_b=%d", K_a, K_b);
        return false;
    }

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    cublasOperation_t opA = trans_a ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasOperation_t opB = trans_b ? CUBLAS_OP_T : CUBLAS_OP_N;

    // Leading dimensions (column-major for cuBLAS)
    int lda = trans_a ? K_a : M;
    int ldb = trans_b ? N : K_a;
    int ldc = M;

    // cuBLAS uses column-major, so we compute B^T @ A^T = (A @ B)^T
    // which gives us C in row-major format
    CUBLAS_CHECK(cublasSgemm(handle,
        opB, opA,
        N, M, K_a,
        &alpha,
        (const float*)B->data, trans_b ? K_a : N,
        (const float*)A->data, trans_a ? M : K_a,
        &beta,
        (float*)C->data, N));

    return true;
}

bool nimcp_gpu_gemv(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    float alpha,
    float beta,
    bool trans_a)
{
    if (!ctx || !A || !x || !y) {
        LOG_ERROR("Invalid parameters for GEMV");
        return false;
    }

    int M = A->dims[A->ndim - 2];
    int N = A->dims[A->ndim - 1];

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    cublasOperation_t op = trans_a ? CUBLAS_OP_T : CUBLAS_OP_N;

    // For row-major A, we use transposed operation
    CUBLAS_CHECK(cublasSgemv(handle,
        trans_a ? CUBLAS_OP_N : CUBLAS_OP_T,
        N, M,
        &alpha,
        (const float*)A->data, N,
        (const float*)x->data, 1,
        &beta,
        (float*)y->data, 1));

    return true;
}

bool nimcp_gpu_gemm_batched(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b)
{
    if (!ctx || !A || !B || !C) {
        LOG_ERROR("Invalid parameters for batched GEMM");
        return false;
    }

    if (A->ndim < 3 || B->ndim < 3 || C->ndim < 3) {
        LOG_ERROR("Batched GEMM requires at least 3D tensors");
        return false;
    }

    int batch_size = A->dims[0];
    int M = trans_a ? A->dims[2] : A->dims[1];
    int K = trans_a ? A->dims[1] : A->dims[2];
    int N = trans_b ? B->dims[1] : B->dims[2];

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    cublasOperation_t opA = trans_a ? CUBLAS_OP_T : CUBLAS_OP_N;
    cublasOperation_t opB = trans_b ? CUBLAS_OP_T : CUBLAS_OP_N;

    long long strideA = M * K;
    long long strideB = K * N;
    long long strideC = M * N;

    CUBLAS_CHECK(cublasSgemmStridedBatched(handle,
        opB, opA,
        N, M, K,
        &alpha,
        (const float*)B->data, trans_b ? K : N, strideB,
        (const float*)A->data, trans_a ? M : K, strideA,
        &beta,
        (float*)C->data, N, strideC,
        batch_size));

    return true;
}

//=============================================================================
// Element-wise Kernels
//=============================================================================

__global__ void kernel_add(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = a[idx] + b[idx];
    }
}

__global__ void kernel_sub(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = a[idx] - b[idx];
    }
}

__global__ void kernel_mul(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = a[idx] * b[idx];
    }
}

__global__ void kernel_div(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        /* P1-12: Epsilon guard to prevent division-by-zero */
        out[idx] = a[idx] / (b[idx] + ((b[idx] >= 0.0f) ? NIMCP_EPS : -NIMCP_EPS));
    }
}

__global__ void kernel_add_scalar(const float* a, float scalar, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = a[idx] + scalar;
    }
}

__global__ void kernel_mul_scalar(const float* a, float scalar, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = a[idx] * scalar;
    }
}

bool nimcp_gpu_add(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    if (a->numel != b->numel || a->numel != out->numel) {
        LOG_ERROR("Tensor size mismatch for add");
        return false;
    }
    kernel_add<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_sub(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    if (a->numel != b->numel || a->numel != out->numel) return false;
    kernel_sub<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_mul(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    if (a->numel != b->numel || a->numel != out->numel) return false;
    kernel_mul<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_div(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    if (a->numel != b->numel || a->numel != out->numel) return false;
    kernel_div<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_add_scalar(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                          float scalar, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !out) return false;
    kernel_add_scalar<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, scalar, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_mul_scalar(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                          float scalar, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !out) return false;
    kernel_mul_scalar<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, scalar, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Activation Kernels
//=============================================================================

__global__ void kernel_relu(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = fmaxf(0.0f, x[idx]);
    }
}

__global__ void kernel_leaky_relu(const float* x, float* out, float alpha, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = x[idx];
        out[idx] = val > 0.0f ? val : alpha * val;
    }
}

__global__ void kernel_sigmoid(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = 1.0f / (1.0f + expf(-x[idx]));
    }
}

__global__ void kernel_tanh_activation(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = tanhf(x[idx]);
    }
}

__global__ void kernel_gelu(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // GELU approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
        float val = x[idx];
        float val_cubed = val * val * val;
        float inner = 0.7978845608f * (val + 0.044715f * val_cubed);  // sqrt(2/pi) ~ 0.7978845608
        out[idx] = 0.5f * val * (1.0f + tanhf(inner));
    }
}

__global__ void kernel_silu(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = x[idx];
        out[idx] = val / (1.0f + expf(-val));  // x * sigmoid(x)
    }
}

bool nimcp_gpu_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_relu<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_leaky_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                          nimcp_gpu_tensor_t* out, float alpha)
{
    if (!ctx || !x || !out) return false;
    kernel_leaky_relu<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const float*)x->data, (float*)out->data, alpha, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_sigmoid(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_sigmoid<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_tanh(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_tanh_activation<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_gelu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_gelu<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_silu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_silu<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Softmax Kernel (Numerically Stable)
//=============================================================================

__global__ void kernel_softmax_1d(const float* x, float* out, size_t batch_size, size_t dim_size)
{
    size_t batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    const float* x_row = x + batch_idx * dim_size;
    float* out_row = out + batch_idx * dim_size;

    // Step 1: Find max (for numerical stability)
    __shared__ float shared_max[BLOCK_SIZE];
    float thread_max = -FLT_MAX;
    for (size_t i = threadIdx.x; i < dim_size; i += blockDim.x) {
        thread_max = fmaxf(thread_max, x_row[i]);
    }
    shared_max[threadIdx.x] = thread_max;
    __syncthreads();

    // Reduce to find global max
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_max[threadIdx.x] = fmaxf(shared_max[threadIdx.x], shared_max[threadIdx.x + stride]);
        }
        __syncthreads();
    }
    float max_val = shared_max[0];

    // Step 2: Compute exp(x - max) and sum
    __shared__ float shared_sum[BLOCK_SIZE];
    float thread_sum = 0.0f;
    for (size_t i = threadIdx.x; i < dim_size; i += blockDim.x) {
        float exp_val = expf(x_row[i] - max_val);
        out_row[i] = exp_val;  // Store temporarily
        thread_sum += exp_val;
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    // Reduce to find sum
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    float sum_val = shared_sum[0];
    // W5-2: Guard against degenerate input (all -inf) producing sum_val=0
    if (sum_val < 1e-12f) sum_val = 1e-12f;

    // Step 3: Normalize
    for (size_t i = threadIdx.x; i < dim_size; i += blockDim.x) {
        out_row[i] = out_row[i] / sum_val;
    }
}

bool nimcp_gpu_softmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    if (x->ndim < 1) return false;

    size_t dim_size = x->dims[x->ndim - 1];
    if (dim_size == 0) {
        LOG_ERROR("softmax dim_size is 0 — cannot normalize over empty dimension");
        return false;
    }
    size_t batch_size = x->numel / dim_size;

    /* P3: batch_size used as CUDA grid dimension — clamp to 2^31-1 max. */
    if (batch_size == 0 || batch_size > (size_t)INT32_MAX) {
        LOG_ERROR("softmax batch_size %zu invalid or exceeds CUDA grid dim limit", batch_size);
        return false;
    }
    kernel_softmax_1d<<<(unsigned int)batch_size, BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, batch_size, dim_size);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__global__ void kernel_log_softmax_1d(const float* x, float* out, size_t batch_size, size_t dim_size)
{
    size_t batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    const float* x_row = x + batch_idx * dim_size;
    float* out_row = out + batch_idx * dim_size;

    // Step 1: Find max
    __shared__ float shared_max[BLOCK_SIZE];
    float thread_max = -FLT_MAX;
    for (size_t i = threadIdx.x; i < dim_size; i += blockDim.x) {
        thread_max = fmaxf(thread_max, x_row[i]);
    }
    shared_max[threadIdx.x] = thread_max;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_max[threadIdx.x] = fmaxf(shared_max[threadIdx.x], shared_max[threadIdx.x + stride]);
        }
        __syncthreads();
    }
    float max_val = shared_max[0];

    // Step 2: Compute sum of exp(x - max)
    __shared__ float shared_sum[BLOCK_SIZE];
    float thread_sum = 0.0f;
    for (size_t i = threadIdx.x; i < dim_size; i += blockDim.x) {
        thread_sum += expf(x_row[i] - max_val);
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }
    // W5-3: Guard against logf(0) when all exp() underflow to 0
    float safe_sum = shared_sum[0];
    if (safe_sum < 1e-12f) safe_sum = 1e-12f;
    float log_sum = logf(safe_sum);

    // Step 3: Compute log_softmax = x - max - log(sum)
    for (size_t i = threadIdx.x; i < dim_size; i += blockDim.x) {
        out_row[i] = x_row[i] - max_val - log_sum;
    }
}

bool nimcp_gpu_log_softmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    if (x->ndim < 1) return false;

    size_t dim_size = x->dims[x->ndim - 1];
    if (dim_size == 0) {
        LOG_ERROR("log_softmax dim_size is 0 — cannot normalize over empty dimension");
        return false;
    }
    size_t batch_size = x->numel / dim_size;

    /* Clamp batch_size to CUDA grid dim limit (2^31-1). */
    if (batch_size > (size_t)INT32_MAX) {
        LOG_ERROR("log_softmax batch_size %zu exceeds CUDA grid dim limit", batch_size);
        return false;
    }
    kernel_log_softmax_1d<<<(unsigned int)batch_size, BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, batch_size, dim_size);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Math Kernels
//=============================================================================

__global__ void kernel_exp(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = expf(x[idx]);
    }
}

__global__ void kernel_log(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        /* P2: Clamp to positive to prevent log(0) or log(negative) -> NaN/undefined */
        out[idx] = logf(fmaxf(x[idx], NIMCP_EPS));
    }
}

__global__ void kernel_sqrt(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        /* P2: Clamp to non-negative to prevent sqrt(negative) -> NaN */
        out[idx] = sqrtf(fmaxf(x[idx], 0.0f));
    }
}

__global__ void kernel_pow(const float* x, float exponent, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = powf(x[idx], exponent);
    }
}

__global__ void kernel_abs(const float* x, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = fabsf(x[idx]);
    }
}

__global__ void kernel_clamp(const float* x, float min_val, float max_val, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = fminf(fmaxf(x[idx], min_val), max_val);
    }
}

bool nimcp_gpu_exp(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_exp<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_log(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_log<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_sqrt(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_sqrt<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_pow(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float exponent, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_pow<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, exponent, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_abs(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_abs<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_clamp(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                     float min_val, float max_val, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    kernel_clamp<<<GRID_SIZE(x->numel), BLOCK_SIZE>>>(
        (const float*)x->data, min_val, max_val, (float*)out->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Reduction Kernels with Warp Shuffle
//=============================================================================

__device__ inline float warp_reduce_sum(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

__device__ inline float warp_reduce_max(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        val = fmaxf(val, __shfl_down_sync(0xffffffff, val, offset));
    }
    return val;
}

__device__ inline float warp_reduce_min(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        val = fminf(val, __shfl_down_sync(0xffffffff, val, offset));
    }
    return val;
}

__global__ void kernel_reduce_sum(const float* x, float* out, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float sum = 0.0f;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        sum += x[i];
    }

    // Warp-level reduction
    sum = warp_reduce_sum(sum);

    // First thread in each warp writes to shared memory
    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) {
        shared[warp_id] = sum;
    }
    __syncthreads();

    // First warp reduces shared memory
    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
        sum = warp_reduce_sum(sum);
        if (lane == 0) {
            atomicAdd(out, sum);
        }
    }
}

__global__ void kernel_reduce_max(const float* x, float* out, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float max_val = -FLT_MAX;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        max_val = fmaxf(max_val, x[i]);
    }

    max_val = warp_reduce_max(max_val);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) {
        shared[warp_id] = max_val;
    }
    __syncthreads();

    if (warp_id == 0) {
        max_val = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : -FLT_MAX;
        max_val = warp_reduce_max(max_val);
        if (lane == 0) {
            // Atomic max for float (using atomicCAS)
            int* out_int = (int*)out;
            int old = *out_int, assumed;
            do {
                assumed = old;
                old = atomicCAS(out_int, assumed,
                    __float_as_int(fmaxf(max_val, __int_as_float(assumed))));
            } while (assumed != old);
        }
    }
}

__global__ void kernel_reduce_min(const float* x, float* out, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float min_val = FLT_MAX;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        min_val = fminf(min_val, x[i]);
    }

    min_val = warp_reduce_min(min_val);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) {
        shared[warp_id] = min_val;
    }
    __syncthreads();

    if (warp_id == 0) {
        min_val = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : FLT_MAX;
        min_val = warp_reduce_min(min_val);
        if (lane == 0) {
            int* out_int = (int*)out;
            int old = *out_int, assumed;
            do {
                assumed = old;
                old = atomicCAS(out_int, assumed,
                    __float_as_int(fminf(min_val, __int_as_float(assumed))));
            } while (assumed != old);
        }
    }
}

//=============================================================================
// Axis-Specific Reduction Kernels
//=============================================================================

/**
 * @brief Reduction operation types for axis-specific reduction
 */
typedef enum nimcp_reduce_op {
    NIMCP_REDUCE_SUM,
    NIMCP_REDUCE_MEAN,
    NIMCP_REDUCE_MAX,
    NIMCP_REDUCE_MIN,
    NIMCP_REDUCE_PROD,
    NIMCP_REDUCE_ARGMAX,
    NIMCP_REDUCE_ARGMIN
} nimcp_reduce_op_t;

/**
 * @brief Generic reduction functor for sum operation
 */
struct ReduceSum {
    __device__ __forceinline__ float operator()(float a, float b) const {
        return a + b;
    }
    __device__ __forceinline__ float identity() const { return 0.0f; }
};

/**
 * @brief Generic reduction functor for max operation
 */
struct ReduceMax {
    __device__ __forceinline__ float operator()(float a, float b) const {
        return fmaxf(a, b);
    }
    __device__ __forceinline__ float identity() const { return -FLT_MAX; }
};

/**
 * @brief Generic reduction functor for min operation
 */
struct ReduceMin {
    __device__ __forceinline__ float operator()(float a, float b) const {
        return fminf(a, b);
    }
    __device__ __forceinline__ float identity() const { return FLT_MAX; }
};

/**
 * @brief Generic reduction functor for product operation
 */
struct ReduceProd {
    __device__ __forceinline__ float operator()(float a, float b) const {
        return a * b;
    }
    __device__ __forceinline__ float identity() const { return 1.0f; }
};

/**
 * @brief Kernel for sum reduction along a specific axis
 *
 * For tensor with shape [outer_size, reduce_size, inner_size]:
 * - Each output element (outer_idx, inner_idx) sums over reduce_size elements
 * - Output shape: [outer_size, inner_size]
 */
__global__ void kernel_reduce_sum_axis(
    const float* input,
    float* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    // Each thread handles one output element
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    // Sum over the reduction axis
    float sum = 0.0f;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        sum += input[base_idx + r * inner_size];
    }

    output[out_idx] = sum;
}

/**
 * @brief Kernel for max reduction along a specific axis
 */
__global__ void kernel_reduce_max_axis(
    const float* input,
    float* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    float max_val = -FLT_MAX;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        max_val = fmaxf(max_val, input[base_idx + r * inner_size]);
    }

    output[out_idx] = max_val;
}

/**
 * @brief Kernel for min reduction along a specific axis
 */
__global__ void kernel_reduce_min_axis(
    const float* input,
    float* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    float min_val = FLT_MAX;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        min_val = fminf(min_val, input[base_idx + r * inner_size]);
    }

    output[out_idx] = min_val;
}

/**
 * @brief Kernel for mean reduction along a specific axis
 */
__global__ void kernel_reduce_mean_axis(
    const float* input,
    float* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    float sum = 0.0f;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        sum += input[base_idx + r * inner_size];
    }

    output[out_idx] = sum / (float)reduce_size;
}

/**
 * @brief Kernel for argmax reduction along a specific axis
 */
__global__ void kernel_reduce_argmax_axis(
    const float* input,
    int* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    float max_val = -FLT_MAX;
    int max_idx = 0;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        float val = input[base_idx + r * inner_size];
        if (val > max_val) {
            max_val = val;
            max_idx = (int)r;
        }
    }

    output[out_idx] = max_idx;
}

/**
 * @brief Kernel for argmin reduction along a specific axis
 */
__global__ void kernel_reduce_argmin_axis(
    const float* input,
    int* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    float min_val = FLT_MAX;
    int min_idx = 0;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        float val = input[base_idx + r * inner_size];
        if (val < min_val) {
            min_val = val;
            min_idx = (int)r;
        }
    }

    output[out_idx] = min_idx;
}

/**
 * @brief Kernel for product reduction along a specific axis
 */
__global__ void kernel_reduce_prod_axis(
    const float* input,
    float* output,
    size_t outer_size,
    size_t reduce_size,
    size_t inner_size)
{
    size_t out_idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_out = outer_size * inner_size;

    if (out_idx >= total_out) return;

    size_t outer_idx = out_idx / inner_size;
    size_t inner_idx = out_idx % inner_size;

    float prod = 1.0f;
    size_t base_idx = outer_idx * reduce_size * inner_size + inner_idx;

    for (size_t r = 0; r < reduce_size; r++) {
        prod *= input[base_idx + r * inner_size];
    }

    output[out_idx] = prod;
}

/**
 * @brief Transpose for multi-axis reduction (move reduction axes to end)
 */
__global__ void kernel_transpose_for_reduce(
    const float* input,
    float* output,
    const size_t* input_dims,
    const int* perm,
    int ndim,
    size_t numel)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel) return;

    // Compute output coordinates from linear index
    size_t remaining = idx;
    size_t out_coords[8];  // Max 8 dimensions supported
    for (int d = ndim - 1; d >= 0; d--) {
        size_t dim_size = input_dims[perm[d]];
        out_coords[d] = remaining % dim_size;
        remaining /= dim_size;
    }

    // Compute input index from permuted coordinates
    size_t in_idx = 0;
    size_t stride = 1;
    for (int d = ndim - 1; d >= 0; d--) {
        // Inverse permutation: find where dimension d came from
        int src_dim = -1;
        for (int p = 0; p < ndim; p++) {
            if (perm[p] == d) {
                src_dim = p;
                break;
            }
        }
        in_idx += out_coords[src_dim] * stride;
        stride *= input_dims[d];
    }

    output[idx] = input[in_idx];
}

/**
 * @brief Helper to compute outer/reduce/inner sizes for axis reduction
 */
static void compute_axis_sizes(
    const size_t* dims,
    int ndim,
    int axis,
    size_t* outer_size,
    size_t* reduce_size,
    size_t* inner_size)
{
    *outer_size = 1;
    *reduce_size = dims[axis];
    *inner_size = 1;

    for (int i = 0; i < axis; i++) {
        *outer_size *= dims[i];
    }
    for (int i = axis + 1; i < ndim; i++) {
        *inner_size *= dims[i];
    }
}

/**
 * @brief Reduce tensor along a single axis
 */
extern "C" int nimcp_tensor_reduce_axis(
    void* gpu_ctx,
    const float* input,
    float* output,
    const size_t* input_dims,
    int input_ndim,
    int axis,
    nimcp_reduce_op_t op)
{
    if (!gpu_ctx || !input || !output || !input_dims || input_ndim <= 0) {
        return -1;
    }

    // Handle negative axis
    if (axis < 0) axis += input_ndim;
    if (axis < 0 || axis >= input_ndim) {
        LOG_ERROR("Invalid axis %d for tensor with %d dimensions", axis, input_ndim);
        return -1;
    }

    size_t outer_size, reduce_size, inner_size;
    compute_axis_sizes(input_dims, input_ndim, axis, &outer_size, &reduce_size, &inner_size);

    size_t total_out = outer_size * inner_size;
    int grid = GRID_SIZE(total_out);

    switch (op) {
        case NIMCP_REDUCE_SUM:
            kernel_reduce_sum_axis<<<grid, BLOCK_SIZE>>>(
                input, output, outer_size, reduce_size, inner_size);
            break;
        case NIMCP_REDUCE_MEAN:
            kernel_reduce_mean_axis<<<grid, BLOCK_SIZE>>>(
                input, output, outer_size, reduce_size, inner_size);
            break;
        case NIMCP_REDUCE_MAX:
            kernel_reduce_max_axis<<<grid, BLOCK_SIZE>>>(
                input, output, outer_size, reduce_size, inner_size);
            break;
        case NIMCP_REDUCE_MIN:
            kernel_reduce_min_axis<<<grid, BLOCK_SIZE>>>(
                input, output, outer_size, reduce_size, inner_size);
            break;
        case NIMCP_REDUCE_PROD:
            kernel_reduce_prod_axis<<<grid, BLOCK_SIZE>>>(
                input, output, outer_size, reduce_size, inner_size);
            break;
        case NIMCP_REDUCE_ARGMAX:
            kernel_reduce_argmax_axis<<<grid, BLOCK_SIZE>>>(
                input, (int*)output, outer_size, reduce_size, inner_size);
            break;
        case NIMCP_REDUCE_ARGMIN:
            kernel_reduce_argmin_axis<<<grid, BLOCK_SIZE>>>(
                input, (int*)output, outer_size, reduce_size, inner_size);
            break;
        default:
            LOG_ERROR("Unknown reduction operation: %d", op);
            return -1;
    }

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

/**
 * @brief Reduce tensor along multiple axes
 *
 * For multiple axes, we reduce one axis at a time, using temporary buffers.
 */
extern "C" int nimcp_tensor_reduce_axes(
    void* gpu_ctx,
    const float* input,
    float* output,
    const size_t* input_dims,
    int input_ndim,
    const int* axes,
    int num_axes,
    nimcp_reduce_op_t op,
    bool keepdims)
{
    if (!gpu_ctx || !input || !output || !input_dims || !axes ||
        input_ndim <= 0 || num_axes <= 0) {
        return -1;
    }

    /* P3-T1: Validate ndim against NIMCP_TENSOR_MAX_RANK (matches stack array sizes) */
    if (input_ndim > NIMCP_TENSOR_MAX_RANK) {
        return -1;
    }

    // For single axis, use direct reduction
    if (num_axes == 1) {
        return nimcp_tensor_reduce_axis(gpu_ctx, input, output, input_dims, input_ndim, axes[0], op);
    }

    // Sort axes in descending order to reduce from end first
    int sorted_axes[8];
    for (int i = 0; i < num_axes; i++) {
        sorted_axes[i] = axes[i];
        if (sorted_axes[i] < 0) sorted_axes[i] += input_ndim;
    }
    for (int i = 0; i < num_axes - 1; i++) {
        for (int j = i + 1; j < num_axes; j++) {
            if (sorted_axes[i] < sorted_axes[j]) {
                int tmp = sorted_axes[i];
                sorted_axes[i] = sorted_axes[j];
                sorted_axes[j] = tmp;
            }
        }
    }

    // Compute total elements
    size_t numel = 1;
    for (int i = 0; i < input_ndim; i++) {
        numel *= input_dims[i];
    }

    // Allocate temporary buffers
    float* temp1 = NULL;
    float* temp2 = NULL;
    /* P1-T1: Check cudaMalloc return values to prevent NULL dereference */
    cudaError_t alloc_err1 = cudaMalloc(&temp1, numel * sizeof(float));
    cudaError_t alloc_err2 = cudaMalloc(&temp2, numel * sizeof(float));
    if (alloc_err1 != cudaSuccess || alloc_err2 != cudaSuccess) {
        if (temp1) cudaFree(temp1);
        if (temp2) cudaFree(temp2);
        return -1;
    }

    // Copy current dims (will be modified as we reduce)
    size_t current_dims[8];
    for (int i = 0; i < input_ndim; i++) {
        current_dims[i] = input_dims[i];
    }
    int current_ndim = input_ndim;

    const float* src = input;
    float* dst = temp1;

    // Reduce each axis in order
    for (int a = 0; a < num_axes; a++) {
        int axis = sorted_axes[a];

        // Adjust axis for already-reduced dimensions
        for (int prev = 0; prev < a; prev++) {
            if (sorted_axes[prev] < axis) {
                axis--;  // Account for removed dimensions
            }
        }

        // Perform reduction
        int result = nimcp_tensor_reduce_axis(gpu_ctx, src, dst, current_dims, current_ndim, axis, op);
        if (result != 0) {
            cudaFree(temp1);
            cudaFree(temp2);
            return result;
        }

        // Update dimensions for next reduction
        for (int d = axis; d < current_ndim - 1; d++) {
            current_dims[d] = current_dims[d + 1];
        }
        current_ndim--;

        // Swap buffers
        src = dst;
        dst = (dst == temp1) ? temp2 : temp1;
    }

    // Copy final result to output
    size_t out_numel = 1;
    for (int i = 0; i < current_ndim; i++) {
        out_numel *= current_dims[i];
    }
    /* P2-T3: Check cudaMemcpy return value */
    cudaError_t cpy_err = cudaMemcpy(output, src, out_numel * sizeof(float), cudaMemcpyDeviceToDevice);

    cudaFree(temp1);
    cudaFree(temp2);

    return (cpy_err == cudaSuccess) ? 0 : -1;
}

bool nimcp_gpu_sum(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims)
{
    if (!ctx || !x || !out) return false;

    // Full reduction (all elements)
    if (axis < 0) {
        NIMCP_CUDA_RECOVER(cudaMemset(out->data, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);
        int grid = (x->numel + BLOCK_SIZE - 1) / BLOCK_SIZE;
        grid = grid > 256 ? 256 : grid;  // Limit grid size
        kernel_reduce_sum<<<grid, BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
        return true;
    }

    // Axis-specific reduction
    int result = nimcp_tensor_reduce_axis(ctx, (const float*)x->data, (float*)out->data,
                                          x->dims, (int)x->ndim, axis, NIMCP_REDUCE_SUM);
    return (result == 0);
}

bool nimcp_gpu_mean(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                    nimcp_gpu_tensor_t* out, int axis, bool keepdims)
{
    if (!ctx || !x || !out) return false;

    // Full reduction
    if (axis < 0) {
        if (!nimcp_gpu_sum(ctx, x, out, axis, keepdims)) return false;
        float scale = 1.0f / (float)x->numel;
        kernel_mul_scalar<<<1, 1>>>((const float*)out->data, scale, (float*)out->data, 1);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
        return true;
    }

    // Axis-specific mean reduction
    int result = nimcp_tensor_reduce_axis(ctx, (const float*)x->data, (float*)out->data,
                                          x->dims, (int)x->ndim, axis, NIMCP_REDUCE_MEAN);
    return (result == 0);
}

bool nimcp_gpu_max(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims)
{
    if (!ctx || !x || !out) return false;

    if (axis < 0) {
        float init = -FLT_MAX;
        NIMCP_CUDA_RECOVER(cudaMemcpy(out->data, &init, sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
        int grid = (x->numel + BLOCK_SIZE - 1) / BLOCK_SIZE;
        grid = grid > 256 ? 256 : grid;
        kernel_reduce_max<<<grid, BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
        return true;
    }

    // Axis-specific max reduction
    int result = nimcp_tensor_reduce_axis(ctx, (const float*)x->data, (float*)out->data,
                                          x->dims, (int)x->ndim, axis, NIMCP_REDUCE_MAX);
    return (result == 0);
}

bool nimcp_gpu_min(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims)
{
    if (!ctx || !x || !out) return false;

    if (axis < 0) {
        float init = FLT_MAX;
        NIMCP_CUDA_RECOVER(cudaMemcpy(out->data, &init, sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
        int grid = (x->numel + BLOCK_SIZE - 1) / BLOCK_SIZE;
        grid = grid > 256 ? 256 : grid;
        kernel_reduce_min<<<grid, BLOCK_SIZE>>>((const float*)x->data, (float*)out->data, x->numel);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
        return true;
    }

    // Axis-specific min reduction
    int result = nimcp_tensor_reduce_axis(ctx, (const float*)x->data, (float*)out->data,
                                          x->dims, (int)x->ndim, axis, NIMCP_REDUCE_MIN);
    return (result == 0);
}

//=============================================================================
// Argmax/Argmin Kernels
//=============================================================================

__global__ void kernel_argmax(const float* x, int64_t* out, size_t n)
{
    __shared__ float shared_val[BLOCK_SIZE];
    __shared__ int shared_idx[BLOCK_SIZE];

    float max_val = -FLT_MAX;
    int max_idx = 0;

    for (size_t i = threadIdx.x; i < n; i += blockDim.x) {
        if (x[i] > max_val) {
            max_val = x[i];
            max_idx = i;
        }
    }

    shared_val[threadIdx.x] = max_val;
    shared_idx[threadIdx.x] = max_idx;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            if (shared_val[threadIdx.x + stride] > shared_val[threadIdx.x]) {
                shared_val[threadIdx.x] = shared_val[threadIdx.x + stride];
                shared_idx[threadIdx.x] = shared_idx[threadIdx.x + stride];
            }
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        *out = shared_idx[0];
    }
}

bool nimcp_gpu_argmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                      nimcp_gpu_tensor_t* out, int axis)
{
    if (!ctx || !x || !out) return false;

    if (axis < 0) {
        kernel_argmax<<<1, BLOCK_SIZE>>>((const float*)x->data, (int64_t*)out->data, x->numel);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
        return true;
    }

    /* P2-T1: Return error instead of silently falling back to global argmax,
     * which would produce incorrect results for axis-specific reduction */
    LOG_ERROR("Axis-specific argmax not yet implemented for axis=%d", axis);
    return false;
}

bool nimcp_gpu_argmin(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                      nimcp_gpu_tensor_t* out, int axis)
{
    // Similar to argmax but with min comparison
    // TODO: Implement properly
    LOG_WARN("argmin not fully implemented");
    return false;
}

//=============================================================================
// Variance and Standard Deviation
//=============================================================================

bool nimcp_gpu_var(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims, bool unbiased)
{
    // Var = E[X^2] - E[X]^2
    // For now, compute on CPU side with GPU data
    LOG_WARN("Variance not fully optimized for GPU");

    // Get mean
    float mean_val = 0.0f;
    size_t one_dim[1] = {1};
    nimcp_gpu_tensor_t* mean_tensor = nimcp_gpu_tensor_create(ctx, one_dim, 1, x->precision);
    if (!mean_tensor) return false;

    nimcp_gpu_mean(ctx, x, mean_tensor, -1, false);
    nimcp_gpu_tensor_to_host(mean_tensor, &mean_val);

    /* P2-T2: Guard against division by zero with unbiased=true and numel<=1 */
    if (unbiased && x->numel <= 1) {
        float zero = 0.0f;
        cudaMemcpy(out->data, &zero, sizeof(float), cudaMemcpyHostToDevice);
        nimcp_gpu_tensor_destroy(mean_tensor);
        return true;
    }

    // Compute variance on CPU (temporary solution)
    float* host_data = (float*)nimcp_malloc(x->numel * sizeof(float));
    /* P1-T2: Check malloc result for host_data */
    if (!host_data) {
        nimcp_gpu_tensor_destroy(mean_tensor);
        return false;
    }
    nimcp_gpu_tensor_to_host(x, host_data);

    float var = 0.0f;
    for (size_t i = 0; i < x->numel; i++) {
        float diff = host_data[i] - mean_val;
        var += diff * diff;
    }
    var /= unbiased ? (x->numel - 1) : x->numel;

    cudaMemcpy(out->data, &var, sizeof(float), cudaMemcpyHostToDevice);

    nimcp_free(host_data);
    nimcp_gpu_tensor_destroy(mean_tensor);
    return true;
}

bool nimcp_gpu_std(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims, bool unbiased)
{
    if (!nimcp_gpu_var(ctx, x, out, axis, keepdims, unbiased)) return false;
    kernel_sqrt<<<1, 1>>>((const float*)out->data, (float*)out->data, 1);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Norm Operations
//=============================================================================

bool nimcp_gpu_norm_l1(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result)
{
    if (!ctx || !x || !result) return false;

    // Create temp tensor for abs values
    nimcp_gpu_tensor_t* abs_tensor = nimcp_gpu_tensor_clone(x);
    if (!abs_tensor) return false;

    nimcp_gpu_abs(ctx, x, abs_tensor);

    size_t one_dim[1] = {1};
    nimcp_gpu_tensor_t* sum_tensor = nimcp_gpu_tensor_create(ctx, one_dim, 1, x->precision);
    nimcp_gpu_sum(ctx, abs_tensor, sum_tensor, -1, false);
    nimcp_gpu_tensor_to_host(sum_tensor, result);

    nimcp_gpu_tensor_destroy(abs_tensor);
    nimcp_gpu_tensor_destroy(sum_tensor);
    return true;
}

bool nimcp_gpu_norm_l2(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result)
{
    if (!ctx || !x || !result) return false;

    /* P2-T4: Validate cuBLAS handle before use */
    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    if (!handle) {
        LOG_ERROR("cuBLAS handle is NULL, cannot compute L2 norm");
        return false;
    }
    CUBLAS_CHECK(cublasSnrm2(handle, x->numel, (const float*)x->data, 1, result));
    return true;
}

bool nimcp_gpu_norm_linf(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result)
{
    if (!ctx || !x || !result) return false;

    nimcp_gpu_tensor_t* abs_tensor = nimcp_gpu_tensor_clone(x);
    if (!abs_tensor) return false;

    nimcp_gpu_abs(ctx, x, abs_tensor);

    size_t one_dim[1] = {1};
    nimcp_gpu_tensor_t* max_tensor = nimcp_gpu_tensor_create(ctx, one_dim, 1, x->precision);
    nimcp_gpu_max(ctx, abs_tensor, max_tensor, -1, false);
    nimcp_gpu_tensor_to_host(max_tensor, result);

    nimcp_gpu_tensor_destroy(abs_tensor);
    nimcp_gpu_tensor_destroy(max_tensor);
    return true;
}

bool nimcp_gpu_norm_frobenius(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result)
{
    // Frobenius norm is same as L2 norm for flattened tensor
    return nimcp_gpu_norm_l2(ctx, x, result);
}

//=============================================================================
// Comparison Operations
//=============================================================================

__global__ void kernel_eq(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = (a[idx] == b[idx]) ? 1.0f : 0.0f;
    }
}

__global__ void kernel_gt(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = (a[idx] > b[idx]) ? 1.0f : 0.0f;
    }
}

__global__ void kernel_lt(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = (a[idx] < b[idx]) ? 1.0f : 0.0f;
    }
}

__global__ void kernel_ge(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = (a[idx] >= b[idx]) ? 1.0f : 0.0f;
    }
}

__global__ void kernel_le(const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = (a[idx] <= b[idx]) ? 1.0f : 0.0f;
    }
}

__global__ void kernel_where(const float* cond, const float* a, const float* b, float* out, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        out[idx] = (cond[idx] != 0.0f) ? a[idx] : b[idx];
    }
}

bool nimcp_gpu_eq(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    kernel_eq<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_gt(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    kernel_gt<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_lt(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    kernel_lt<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_ge(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    kernel_ge<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_le(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !a || !b || !out) return false;
    kernel_le<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_where(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* cond,
                     const nimcp_gpu_tensor_t* a, const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !cond || !a || !b || !out) return false;
    kernel_where<<<GRID_SIZE(a->numel), BLOCK_SIZE>>>(
        (const float*)cond->data, (const float*)a->data, (const float*)b->data, (float*)out->data, a->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Memory Operations
//=============================================================================

__global__ void kernel_fill(float* data, float value, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        data[idx] = value;
    }
}

bool nimcp_gpu_fill(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* tensor, float value)
{
    if (!ctx || !tensor) return false;
    kernel_fill<<<GRID_SIZE(tensor->numel), BLOCK_SIZE>>>((float*)tensor->data, value, tensor->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_zeros(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* tensor)
{
    if (!ctx || !tensor) return false;
    NIMCP_CUDA_RECOVER(cudaMemset(tensor->data, 0, tensor->numel * tensor->elem_size), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

bool nimcp_gpu_ones(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* tensor)
{
    return nimcp_gpu_fill(ctx, tensor, 1.0f);
}

bool nimcp_gpu_copy(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src, nimcp_gpu_tensor_t* dst)
{
    if (!ctx || !src || !dst) return false;
    if (src->numel != dst->numel) return false;
    NIMCP_CUDA_RECOVER(cudaMemcpy(dst->data, src->data, src->numel * src->elem_size, cudaMemcpyDeviceToDevice), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

bool nimcp_gpu_transpose(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return false;
    if (x->ndim < 2) return false;

    // Use cuBLAS for 2D transpose
    size_t rows = x->dims[x->ndim - 2];
    size_t cols = x->dims[x->ndim - 1];

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
    float alpha = 1.0f, beta = 0.0f;

    CUBLAS_CHECK(cublasSgeam(handle,
        CUBLAS_OP_T, CUBLAS_OP_N,
        rows, cols,
        &alpha, (const float*)x->data, cols,
        &beta, (const float*)out->data, rows,
        (float*)out->data, rows));

    return true;
}

bool nimcp_gpu_reshape(nimcp_gpu_tensor_t* tensor, const size_t* new_dims, uint32_t new_ndim)
{
    if (!tensor || !new_dims || new_ndim == 0) return false;

    size_t new_numel = compute_numel(new_dims, new_ndim);
    if (new_numel != tensor->numel) {
        LOG_ERROR("Cannot reshape: numel mismatch (%zu vs %zu)", tensor->numel, new_numel);
        return false;
    }

    /* P1-T3: Assign realloc result to tensor->dims immediately to prevent
     * use-after-free if the second realloc fails (the first realloc may have
     * freed the original pointer, leaving tensor->dims dangling) */
    size_t* new_dims_array = (size_t*)nimcp_realloc(tensor->dims, new_ndim * sizeof(size_t));
    if (!new_dims_array) return false;
    tensor->dims = new_dims_array;  /* Update immediately */

    size_t* new_strides = (size_t*)nimcp_realloc(tensor->strides, new_ndim * sizeof(size_t));
    if (!new_strides) return false;
    tensor->strides = new_strides;
    tensor->ndim = new_ndim;

    // Recompute strides
    size_t stride = 1;
    for (int i = new_ndim - 1; i >= 0; i--) {
        tensor->dims[i] = new_dims[i];
        tensor->strides[i] = stride;
        stride *= new_dims[i];
    }

    return true;
}

//=============================================================================
// CPU-GPU Tensor Integration Functions
//=============================================================================

nimcp_gpu_precision_t nimcp_dtype_to_gpu_precision(nimcp_dtype_t dtype)
{
    switch (dtype) {
        case NIMCP_DTYPE_F32: return NIMCP_GPU_PRECISION_FP32;
        case NIMCP_DTYPE_F64: return NIMCP_GPU_PRECISION_FP32;  // Downcast to FP32
        case NIMCP_DTYPE_F16: return NIMCP_GPU_PRECISION_FP16;
        case NIMCP_DTYPE_BF16: return NIMCP_GPU_PRECISION_BF16;
        case NIMCP_DTYPE_I8: return NIMCP_GPU_PRECISION_INT8;
        case NIMCP_DTYPE_I32:
        case NIMCP_DTYPE_I64:
        case NIMCP_DTYPE_U8:
        case NIMCP_DTYPE_BOOL:
        default: return NIMCP_GPU_PRECISION_FP32;
    }
}

nimcp_dtype_t nimcp_gpu_precision_to_dtype(nimcp_gpu_precision_t precision)
{
    switch (precision) {
        case NIMCP_GPU_PRECISION_FP32: return NIMCP_DTYPE_F32;
        case NIMCP_GPU_PRECISION_FP16: return NIMCP_DTYPE_F16;
        case NIMCP_GPU_PRECISION_BF16: return NIMCP_DTYPE_BF16;
        case NIMCP_GPU_PRECISION_INT8: return NIMCP_DTYPE_I8;
        case NIMCP_GPU_PRECISION_TF32: return NIMCP_DTYPE_F32;
        default: return NIMCP_DTYPE_F32;
    }
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_cpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_tensor_t* cpu_tensor)
{
    if (!ctx || !cpu_tensor) {
        LOG_ERROR("Invalid parameters for CPU->GPU tensor conversion");
        return NULL;
    }

    // Get CPU tensor properties
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(cpu_tensor);
    if (!shape) {
        LOG_ERROR("Failed to get CPU tensor shape");
        return NULL;
    }

    nimcp_dtype_t dtype = nimcp_tensor_dtype(cpu_tensor);
    nimcp_gpu_precision_t precision = nimcp_dtype_to_gpu_precision(dtype);

    // Convert dims from uint32_t to size_t
    size_t dims[NIMCP_TENSOR_MAX_RANK];
    for (uint32_t i = 0; i < shape->rank; i++) {
        dims[i] = (size_t)shape->dims[i];
    }

    // Create GPU tensor
    nimcp_gpu_tensor_t* gpu_tensor = nimcp_gpu_tensor_create(ctx, dims, shape->rank, precision);
    if (!gpu_tensor) {
        LOG_ERROR("Failed to create GPU tensor");
        return NULL;
    }

    // Copy data to GPU
    const void* cpu_data = nimcp_tensor_data_const(cpu_tensor);
    size_t data_size = shape->numel * nimcp_dtype_size(dtype);

    cudaError_t err = cudaMemcpy(gpu_tensor->data, cpu_data, data_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy data to GPU: %s", cudaGetErrorString(err));
        nimcp_gpu_tensor_destroy(gpu_tensor);
        return NULL;
    }

    LOG_DEBUG("Converted CPU tensor to GPU: numel=%zu, dtype=%d->precision=%d",
              shape->numel, dtype, precision);

    return gpu_tensor;
}

nimcp_tensor_t* nimcp_cpu_tensor_from_gpu(const nimcp_gpu_tensor_t* gpu_tensor)
{
    if (!gpu_tensor) {
        LOG_ERROR("Invalid GPU tensor for GPU->CPU conversion");
        return NULL;
    }

    // Convert precision to dtype
    nimcp_dtype_t dtype = nimcp_gpu_precision_to_dtype(gpu_tensor->precision);

    // Convert dims from size_t to uint32_t
    uint32_t dims[NIMCP_TENSOR_MAX_RANK];
    for (uint32_t i = 0; i < gpu_tensor->ndim; i++) {
        dims[i] = (uint32_t)gpu_tensor->dims[i];
    }

    // Create CPU tensor
    nimcp_tensor_t* cpu_tensor = nimcp_tensor_create(dims, gpu_tensor->ndim, dtype);
    if (!cpu_tensor) {
        LOG_ERROR("Failed to create CPU tensor");
        return NULL;
    }

    // Copy data from GPU
    void* cpu_data = nimcp_tensor_data(cpu_tensor);
    size_t data_size = gpu_tensor->numel * gpu_tensor->elem_size;

    cudaError_t err = cudaMemcpy(cpu_data, gpu_tensor->data, data_size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy data from GPU: %s", cudaGetErrorString(err));
        nimcp_tensor_destroy(cpu_tensor);
        return NULL;
    }

    LOG_DEBUG("Converted GPU tensor to CPU: numel=%zu, precision=%d->dtype=%d",
              gpu_tensor->numel, gpu_tensor->precision, dtype);

    return cpu_tensor;
}

bool nimcp_gpu_tensor_copy_to_cpu(
    const nimcp_gpu_tensor_t* gpu_tensor,
    nimcp_tensor_t* cpu_tensor)
{
    if (!gpu_tensor || !cpu_tensor) {
        LOG_ERROR("Invalid tensors for GPU->CPU copy");
        return false;
    }

    // Verify compatible sizes
    size_t cpu_numel = nimcp_tensor_numel(cpu_tensor);
    if (cpu_numel != gpu_tensor->numel) {
        LOG_ERROR("Tensor size mismatch: GPU=%zu, CPU=%zu", gpu_tensor->numel, cpu_numel);
        return false;
    }

    void* cpu_data = nimcp_tensor_data(cpu_tensor);
    size_t data_size = gpu_tensor->numel * gpu_tensor->elem_size;

    cudaError_t err = cudaMemcpy(cpu_data, gpu_tensor->data, data_size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy data from GPU: %s", cudaGetErrorString(err));
        return false;
    }

    return true;
}

bool nimcp_cpu_tensor_copy_to_gpu(
    const nimcp_tensor_t* cpu_tensor,
    nimcp_gpu_tensor_t* gpu_tensor)
{
    if (!cpu_tensor || !gpu_tensor) {
        LOG_ERROR("Invalid tensors for CPU->GPU copy");
        return false;
    }

    // Verify compatible sizes
    size_t cpu_numel = nimcp_tensor_numel(cpu_tensor);
    if (cpu_numel != gpu_tensor->numel) {
        LOG_ERROR("Tensor size mismatch: CPU=%zu, GPU=%zu", cpu_numel, gpu_tensor->numel);
        return false;
    }

    const void* cpu_data = nimcp_tensor_data_const(cpu_tensor);
    size_t data_size = gpu_tensor->numel * gpu_tensor->elem_size;

    cudaError_t err = cudaMemcpy(gpu_tensor->data, cpu_data, data_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to copy data to GPU: %s", cudaGetErrorString(err));
        return false;
    }

    return true;
}

nimcp_tensor_t* nimcp_gpu_accelerate_matmul(
    nimcp_gpu_context_t* ctx,
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b)
{
    if (!ctx || !a || !b) {
        LOG_ERROR("Invalid parameters for GPU-accelerated matmul");
        return NULL;
    }

    // Upload tensors to GPU
    nimcp_gpu_tensor_t* gpu_a = nimcp_gpu_tensor_from_cpu(ctx, a);
    nimcp_gpu_tensor_t* gpu_b = nimcp_gpu_tensor_from_cpu(ctx, b);

    if (!gpu_a || !gpu_b) {
        nimcp_gpu_tensor_destroy(gpu_a);
        nimcp_gpu_tensor_destroy(gpu_b);
        return NULL;
    }

    // Compute output dimensions (M x N from M x K @ K x N)
    size_t M = gpu_a->dims[gpu_a->ndim - 2];
    size_t N = gpu_b->dims[gpu_b->ndim - 1];
    size_t output_dims[] = {M, N};

    nimcp_gpu_tensor_t* gpu_c = nimcp_gpu_tensor_create(ctx, output_dims, 2, gpu_a->precision);
    if (!gpu_c) {
        nimcp_gpu_tensor_destroy(gpu_a);
        nimcp_gpu_tensor_destroy(gpu_b);
        return NULL;
    }

    // Perform GPU matmul
    bool success = nimcp_gpu_gemm(ctx, gpu_a, gpu_b, gpu_c, 1.0f, 0.0f, false, false);

    // Download result
    nimcp_tensor_t* result = NULL;
    if (success) {
        result = nimcp_cpu_tensor_from_gpu(gpu_c);
    }

    // Cleanup GPU tensors
    nimcp_gpu_tensor_destroy(gpu_a);
    nimcp_gpu_tensor_destroy(gpu_b);
    nimcp_gpu_tensor_destroy(gpu_c);

    return result;
}

bool nimcp_gpu_tensor_available(void)
{
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

bool nimcp_gpu_tensor_memory_info(
    nimcp_gpu_context_t* ctx,
    size_t* free_bytes,
    size_t* total_bytes)
{
    if (!ctx || !free_bytes || !total_bytes) return false;

    cudaError_t err = cudaMemGetInfo(free_bytes, total_bytes);
    return (err == cudaSuccess);
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "TENSOR_GPU"

nimcp_gpu_tensor_t* nimcp_gpu_tensor_create(nimcp_gpu_context_t* ctx, const size_t* dims,
                                             uint32_t ndim, nimcp_gpu_precision_t precision)
{
    LOG_WARN("CUDA not available - tensor operations will use CPU fallback");
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_host(nimcp_gpu_context_t* ctx, const void* host_data,
                                                const size_t* dims, uint32_t ndim,
                                                nimcp_gpu_precision_t precision)
{
    return NULL;
}

bool nimcp_gpu_tensor_to_host(const nimcp_gpu_tensor_t* tensor, void* host_data) { return false; }
void nimcp_gpu_tensor_destroy(nimcp_gpu_tensor_t* tensor) {}
nimcp_gpu_tensor_t* nimcp_gpu_tensor_clone(const nimcp_gpu_tensor_t* tensor) { return NULL; }

bool nimcp_gpu_gemm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                    const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                    float alpha, float beta, bool trans_a, bool trans_b) { return false; }

bool nimcp_gpu_gemv(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                    const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* y,
                    float alpha, float beta, bool trans_a) { return false; }

bool nimcp_gpu_gemm_batched(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A,
                            const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                            float alpha, float beta, bool trans_a, bool trans_b) { return false; }

bool nimcp_gpu_add(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_sub(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_mul(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_div(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                   const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_add_scalar(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                          float scalar, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_mul_scalar(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                          float scalar, nimcp_gpu_tensor_t* out) { return false; }

bool nimcp_gpu_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_leaky_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                          nimcp_gpu_tensor_t* out, float alpha) { return false; }
bool nimcp_gpu_sigmoid(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_tanh(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_gelu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_silu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_softmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_log_softmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }

bool nimcp_gpu_exp(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_log(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_sqrt(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_pow(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float exp, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_abs(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_clamp(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                     float min_val, float max_val, nimcp_gpu_tensor_t* out) { return false; }

bool nimcp_gpu_sum(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims) { return false; }
bool nimcp_gpu_mean(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                    nimcp_gpu_tensor_t* out, int axis, bool keepdims) { return false; }
bool nimcp_gpu_max(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims) { return false; }
bool nimcp_gpu_min(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims) { return false; }
bool nimcp_gpu_argmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                      nimcp_gpu_tensor_t* out, int axis) { return false; }
bool nimcp_gpu_argmin(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                      nimcp_gpu_tensor_t* out, int axis) { return false; }
bool nimcp_gpu_var(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims, bool unbiased) { return false; }
bool nimcp_gpu_std(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                   nimcp_gpu_tensor_t* out, int axis, bool keepdims, bool unbiased) { return false; }

bool nimcp_gpu_norm_l1(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result) { return false; }
bool nimcp_gpu_norm_l2(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result) { return false; }
bool nimcp_gpu_norm_linf(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result) { return false; }
bool nimcp_gpu_norm_frobenius(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float* result) { return false; }

bool nimcp_gpu_eq(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_gt(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_lt(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_ge(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_le(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a,
                  const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_where(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* cond,
                     const nimcp_gpu_tensor_t* a, const nimcp_gpu_tensor_t* b,
                     nimcp_gpu_tensor_t* out) { return false; }

bool nimcp_gpu_fill(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* tensor, float value) { return false; }
bool nimcp_gpu_zeros(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* tensor) { return false; }
bool nimcp_gpu_ones(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* tensor) { return false; }
bool nimcp_gpu_copy(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src,
                    nimcp_gpu_tensor_t* dst) { return false; }
bool nimcp_gpu_transpose(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
                         nimcp_gpu_tensor_t* out) { return false; }
bool nimcp_gpu_reshape(nimcp_gpu_tensor_t* tensor, const size_t* new_dims, uint32_t new_ndim) { return false; }

// Note: FFT functions are implemented in nimcp_fft_kernels.cu

// CPU-GPU Tensor Integration Stubs (non-CUDA)
nimcp_gpu_precision_t nimcp_dtype_to_gpu_precision(nimcp_dtype_t dtype)
{
    (void)dtype;
    return NIMCP_GPU_PRECISION_FP32;
}

nimcp_dtype_t nimcp_gpu_precision_to_dtype(nimcp_gpu_precision_t precision)
{
    (void)precision;
    return NIMCP_DTYPE_F32;
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_cpu(nimcp_gpu_context_t* ctx, const nimcp_tensor_t* cpu_tensor)
{
    (void)ctx; (void)cpu_tensor;
    LOG_WARN("CUDA not available - cannot create GPU tensor from CPU tensor");
    return NULL;
}

nimcp_tensor_t* nimcp_cpu_tensor_from_gpu(const nimcp_gpu_tensor_t* gpu_tensor)
{
    (void)gpu_tensor;
    return NULL;
}

bool nimcp_gpu_tensor_copy_to_cpu(const nimcp_gpu_tensor_t* gpu_tensor, nimcp_tensor_t* cpu_tensor)
{
    (void)gpu_tensor; (void)cpu_tensor;
    return false;
}

bool nimcp_cpu_tensor_copy_to_gpu(const nimcp_tensor_t* cpu_tensor, nimcp_gpu_tensor_t* gpu_tensor)
{
    (void)cpu_tensor; (void)gpu_tensor;
    return false;
}

nimcp_tensor_t* nimcp_gpu_accelerate_matmul(nimcp_gpu_context_t* ctx,
                                            const nimcp_tensor_t* a, const nimcp_tensor_t* b)
{
    (void)ctx; (void)a; (void)b;
    LOG_WARN("CUDA not available - using CPU tensor matmul instead");
    // Fall back to CPU tensor matmul
    return nimcp_tensor_matmul(a, b);
}

bool nimcp_gpu_tensor_available(void)
{
    return false;
}

bool nimcp_gpu_tensor_memory_info(nimcp_gpu_context_t* ctx, size_t* free_bytes, size_t* total_bytes)
{
    (void)ctx; (void)free_bytes; (void)total_bytes;
    return false;
}

#endif // NIMCP_ENABLE_CUDA
