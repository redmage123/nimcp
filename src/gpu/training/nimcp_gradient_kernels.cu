/**
 * @file nimcp_gradient_kernels.cu
 * @brief GPU Gradient Computation CUDA Kernels
 *
 * WHAT: CUDA kernels for loss computation and gradient operations
 * WHY:  GPU acceleration for training neural networks
 * HOW:  Custom kernels for loss functions and gradient manipulation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <math.h>
#include <float.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/training/nimcp_training_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "GRADIENT_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Warp-level Reductions
//=============================================================================

__device__ inline float warp_reduce_sum(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

//=============================================================================
// MSE Loss Kernel
//=============================================================================

__global__ void kernel_mse_loss_forward(
    const float* pred, const float* target, float* loss_sum, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float sum = 0.0f;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        float diff = pred[i] - target[i];
        sum += diff * diff;
    }

    sum = warp_reduce_sum(sum);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) {
        shared[warp_id] = sum;
    }
    __syncthreads();

    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
        sum = warp_reduce_sum(sum);
        if (lane == 0) {
            atomicAdd(loss_sum, sum);
        }
    }
}

__global__ void kernel_mse_loss_backward(
    const float* pred, const float* target, float* grad, float scale, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        grad[idx] = scale * (pred[idx] - target[idx]);
    }
}

bool nimcp_gpu_loss_mse(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad)
{
    if (!ctx || !pred || !target || !loss) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = pred->numel;

    // Allocate device memory for loss sum
    float* d_loss_sum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_loss_sum, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_loss_sum, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    // Compute forward loss
    cudaStream_t mse_stream = nimcp_gpu_get_pool_stream(ctx);
    int grid = GRID_SIZE(n);
    grid = grid > 256 ? 256 : grid;
    kernel_mse_loss_forward<<<grid, BLOCK_SIZE, 0, mse_stream>>>(
        (const float*)pred->data, (const float*)target->data, d_loss_sum, n);

    // Async readback — allows overlap with other GPU work on different streams
    float loss_sum;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(&loss_sum, d_loss_sum, sizeof(float), cudaMemcpyDeviceToHost, mse_stream), GPU_ERROR_CUDA_RUNTIME);
    cudaStreamSynchronize(mse_stream);
    *loss = loss_sum / (float)n;

    cudaFree(d_loss_sum);

    // Compute gradient if requested
    if (grad) {
        float scale = 2.0f / (float)n;
        kernel_mse_loss_backward<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (const float*)pred->data, (const float*)target->data,
            (float*)grad->data, scale, n);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

//=============================================================================
// MAE Loss Kernel
//=============================================================================

__global__ void kernel_mae_loss_forward(
    const float* pred, const float* target, float* loss_sum, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float sum = 0.0f;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        sum += fabsf(pred[i] - target[i]);
    }

    sum = warp_reduce_sum(sum);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) {
        shared[warp_id] = sum;
    }
    __syncthreads();

    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
        sum = warp_reduce_sum(sum);
        if (lane == 0) {
            atomicAdd(loss_sum, sum);
        }
    }
}

__global__ void kernel_mae_loss_backward(
    const float* pred, const float* target, float* grad, float scale, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float diff = pred[idx] - target[idx];
        grad[idx] = scale * (diff > 0.0f ? 1.0f : (diff < 0.0f ? -1.0f : 0.0f));
    }
}

bool nimcp_gpu_loss_mae(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad)
{
    if (!ctx || !pred || !target || !loss) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = pred->numel;
    float* d_loss_sum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_loss_sum, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_loss_sum, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    cudaStream_t mae_stream = nimcp_gpu_get_pool_stream(ctx);
    int grid = GRID_SIZE(n);
    grid = grid > 256 ? 256 : grid;
    kernel_mae_loss_forward<<<grid, BLOCK_SIZE, 0, mae_stream>>>(
        (const float*)pred->data, (const float*)target->data, d_loss_sum, n);

    float loss_sum;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(&loss_sum, d_loss_sum, sizeof(float), cudaMemcpyDeviceToHost, mae_stream), GPU_ERROR_CUDA_RUNTIME);
    cudaStreamSynchronize(mae_stream);
    *loss = loss_sum / (float)n;

    cudaFree(d_loss_sum);

    if (grad) {
        float scale = 1.0f / (float)n;
        kernel_mae_loss_backward<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (const float*)pred->data, (const float*)target->data,
            (float*)grad->data, scale, n);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

//=============================================================================
// Cross Entropy Loss Kernel
//=============================================================================

__global__ void kernel_cross_entropy_forward(
    const float* logits, const float* target, float* loss_sum,
    size_t batch_size, size_t num_classes)
{
    size_t batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    const float* logit_row = logits + batch_idx * num_classes;
    const float* target_row = target + batch_idx * num_classes;

    // Compute log-softmax and cross entropy
    __shared__ float shared_max[1];
    __shared__ float shared_sum[1];

    // Step 1: Find max
    float thread_max = -FLT_MAX;
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        thread_max = fmaxf(thread_max, logit_row[i]);
    }

    // Reduce max
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        thread_max = fmaxf(thread_max, __shfl_down_sync(0xffffffff, thread_max, offset));
    }
    if (threadIdx.x == 0) shared_max[0] = thread_max;
    __syncthreads();
    float max_val = shared_max[0];

    // Step 2: Compute sum(exp(x - max))
    float thread_sum = 0.0f;
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        thread_sum += expf(logit_row[i] - max_val);
    }

    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        thread_sum += __shfl_down_sync(0xffffffff, thread_sum, offset);
    }
    if (threadIdx.x == 0) shared_sum[0] = thread_sum;
    __syncthreads();
    float log_sum = logf(shared_sum[0]);

    // Step 3: Compute loss = -sum(target * log_softmax)
    float thread_loss = 0.0f;
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        float log_softmax = logit_row[i] - max_val - log_sum;
        thread_loss -= target_row[i] * log_softmax;
    }

    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        thread_loss += __shfl_down_sync(0xffffffff, thread_loss, offset);
    }
    if (threadIdx.x == 0) {
        atomicAdd(loss_sum, thread_loss);
    }
}

__global__ void kernel_cross_entropy_backward(
    const float* logits, const float* target, float* grad,
    size_t batch_size, size_t num_classes, float scale)
{
    size_t batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    const float* logit_row = logits + batch_idx * num_classes;
    const float* target_row = target + batch_idx * num_classes;
    float* grad_row = grad + batch_idx * num_classes;

    __shared__ float shared_max[1];
    __shared__ float shared_sum[1];

    // Compute softmax
    float thread_max = -FLT_MAX;
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        thread_max = fmaxf(thread_max, logit_row[i]);
    }
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        thread_max = fmaxf(thread_max, __shfl_down_sync(0xffffffff, thread_max, offset));
    }
    if (threadIdx.x == 0) shared_max[0] = thread_max;
    __syncthreads();
    float max_val = shared_max[0];

    float thread_sum = 0.0f;
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        thread_sum += expf(logit_row[i] - max_val);
    }
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        thread_sum += __shfl_down_sync(0xffffffff, thread_sum, offset);
    }
    if (threadIdx.x == 0) shared_sum[0] = thread_sum;
    __syncthreads();
    float sum_exp = shared_sum[0];

    // Gradient = softmax - target
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        float softmax = expf(logit_row[i] - max_val) / sum_exp;
        grad_row[i] = scale * (softmax - target_row[i]);
    }
}

bool nimcp_gpu_loss_cross_entropy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* logits,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    int reduction)
{
    if (!ctx || !logits || !target || !loss) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t num_classes = logits->dims[logits->ndim - 1];
    size_t batch_size = logits->numel / num_classes;

    float* d_loss_sum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_loss_sum, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_loss_sum, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    cudaStream_t ce_stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_cross_entropy_forward<<<batch_size, BLOCK_SIZE, 0, ce_stream>>>(
        (const float*)logits->data, (const float*)target->data,
        d_loss_sum, batch_size, num_classes);

    float loss_sum;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(&loss_sum, d_loss_sum, sizeof(float), cudaMemcpyDeviceToHost, ce_stream), GPU_ERROR_CUDA_RUNTIME);
    cudaStreamSynchronize(ce_stream);

    if (reduction == 1) {  // mean
        *loss = loss_sum / (float)batch_size;
    } else if (reduction == 2) {  // sum
        *loss = loss_sum;
    } else {  // none
        *loss = loss_sum;
    }

    cudaFree(d_loss_sum);

    if (grad) {
        float scale = (reduction == 1) ? 1.0f / (float)batch_size : 1.0f;
        kernel_cross_entropy_backward<<<batch_size, BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (const float*)logits->data, (const float*)target->data,
            (float*)grad->data, batch_size, num_classes, scale);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

//=============================================================================
// Binary Cross Entropy Loss
//=============================================================================

__global__ void kernel_bce_loss_forward(
    const float* pred, const float* target, float* loss_sum, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float sum = 0.0f;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        float p = fmaxf(fminf(pred[i], 1.0f - 1e-7f), 1e-7f);  // Clamp for stability
        float t = target[i];
        sum -= t * logf(p) + (1.0f - t) * logf(1.0f - p);
    }

    sum = warp_reduce_sum(sum);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) shared[warp_id] = sum;
    __syncthreads();

    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
        sum = warp_reduce_sum(sum);
        if (lane == 0) atomicAdd(loss_sum, sum);
    }
}

__global__ void kernel_bce_loss_backward(
    const float* pred, const float* target, float* grad, float scale, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float p = fmaxf(fminf(pred[idx], 1.0f - 1e-7f), 1e-7f);
        float t = target[idx];
        grad[idx] = scale * ((p - t) / (p * (1.0f - p)));
    }
}

bool nimcp_gpu_loss_bce(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad)
{
    if (!ctx || !pred || !target || !loss) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = pred->numel;
    float* d_loss_sum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_loss_sum, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_loss_sum, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    int grid = GRID_SIZE(n);
    grid = grid > 256 ? 256 : grid;
    cudaStream_t bce_stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_bce_loss_forward<<<grid, BLOCK_SIZE, 0, bce_stream>>>(
        (const float*)pred->data, (const float*)target->data, d_loss_sum, n);

    float loss_sum;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(&loss_sum, d_loss_sum, sizeof(float), cudaMemcpyDeviceToHost, bce_stream), GPU_ERROR_CUDA_RUNTIME);
    cudaStreamSynchronize(bce_stream);
    *loss = loss_sum / (float)n;

    cudaFree(d_loss_sum);

    if (grad) {
        float scale = 1.0f / (float)n;
        kernel_bce_loss_backward<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (const float*)pred->data, (const float*)target->data,
            (float*)grad->data, scale, n);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

//=============================================================================
// Focal Loss
//=============================================================================

__global__ void kernel_focal_loss_forward(
    const float* pred, const float* target, float* loss_sum,
    float alpha, float gamma, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float sum = 0.0f;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        float p = fmaxf(fminf(pred[i], 1.0f - 1e-7f), 1e-7f);
        float t = target[i];
        float p_t = t * p + (1.0f - t) * (1.0f - p);
        float focal_weight = powf(1.0f - p_t, gamma);
        sum -= alpha * focal_weight * logf(p_t);
    }

    sum = warp_reduce_sum(sum);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) shared[warp_id] = sum;
    __syncthreads();

    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
        sum = warp_reduce_sum(sum);
        if (lane == 0) atomicAdd(loss_sum, sum);
    }
}

bool nimcp_gpu_loss_focal(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    float alpha,
    float gamma)
{
    if (!ctx || !pred || !target || !loss) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = pred->numel;
    float* d_loss_sum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_loss_sum, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_loss_sum, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    int grid = GRID_SIZE(n);
    grid = grid > 256 ? 256 : grid;
    cudaStream_t focal_stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_focal_loss_forward<<<grid, BLOCK_SIZE, 0, focal_stream>>>(
        (const float*)pred->data, (const float*)target->data,
        d_loss_sum, alpha, gamma, n);

    float loss_sum;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(&loss_sum, d_loss_sum, sizeof(float), cudaMemcpyDeviceToHost, focal_stream), GPU_ERROR_CUDA_RUNTIME);
    cudaStreamSynchronize(focal_stream);
    *loss = loss_sum / (float)n;

    cudaFree(d_loss_sum);

    // TODO: Implement focal loss gradient
    if (grad) {
        LOG_WARN("Focal loss gradient not implemented, using BCE gradient approximation");
        return nimcp_gpu_loss_bce(ctx, pred, target, loss, grad);
    }

    return true;
}

//=============================================================================
// Huber Loss
//=============================================================================

__global__ void kernel_huber_loss_forward(
    const float* pred, const float* target, float* loss_sum, float delta, size_t n)
{
    __shared__ float shared[BLOCK_SIZE / WARP_SIZE];

    float sum = 0.0f;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        float diff = pred[i] - target[i];
        float abs_diff = fabsf(diff);
        if (abs_diff <= delta) {
            sum += 0.5f * diff * diff;
        } else {
            sum += delta * (abs_diff - 0.5f * delta);
        }
    }

    sum = warp_reduce_sum(sum);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) shared[warp_id] = sum;
    __syncthreads();

    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0.0f;
        sum = warp_reduce_sum(sum);
        if (lane == 0) atomicAdd(loss_sum, sum);
    }
}

__global__ void kernel_huber_loss_backward(
    const float* pred, const float* target, float* grad, float delta, float scale, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float diff = pred[idx] - target[idx];
        float abs_diff = fabsf(diff);
        if (abs_diff <= delta) {
            grad[idx] = scale * diff;
        } else {
            grad[idx] = scale * delta * (diff > 0.0f ? 1.0f : -1.0f);
        }
    }
}

bool nimcp_gpu_loss_huber(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    float delta)
{
    if (!ctx || !pred || !target || !loss) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = pred->numel;
    float* d_loss_sum;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_loss_sum, sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_loss_sum, 0, sizeof(float)), GPU_ERROR_CUDA_RUNTIME);

    int grid = GRID_SIZE(n);
    grid = grid > 256 ? 256 : grid;
    cudaStream_t huber_stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_huber_loss_forward<<<grid, BLOCK_SIZE, 0, huber_stream>>>(
        (const float*)pred->data, (const float*)target->data, d_loss_sum, delta, n);

    float loss_sum;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(&loss_sum, d_loss_sum, sizeof(float), cudaMemcpyDeviceToHost, huber_stream), GPU_ERROR_CUDA_RUNTIME);
    cudaStreamSynchronize(huber_stream);
    *loss = loss_sum / (float)n;

    cudaFree(d_loss_sum);

    if (grad) {
        float scale = 1.0f / (float)n;
        kernel_huber_loss_backward<<<GRID_SIZE(n), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
            (const float*)pred->data, (const float*)target->data,
            (float*)grad->data, delta, scale, n);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    }

    return true;
}

//=============================================================================
// Gradient Operations
//=============================================================================

__global__ void kernel_gradient_accumulate(const float* grad, float* accum, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        accum[idx] += grad[idx];
    }
}

bool nimcp_gpu_gradient_accumulate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad,
    nimcp_gpu_tensor_t* accum)
{
    if (!ctx || !grad || !accum) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_gradient_accumulate<<<GRID_SIZE(grad->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)grad->data, (float*)accum->data, grad->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__global__ void kernel_gradient_scale(float* grad, float scale, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        grad[idx] *= scale;
    }
}

bool nimcp_gpu_gradient_scale(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad,
    float scale)
{
    if (!ctx || !grad) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_gradient_scale<<<GRID_SIZE(grad->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (float*)grad->data, scale, grad->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_gradient_clip_norm(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t** grads,
    size_t n_grads,
    float max_norm,
    float* total_norm)
{
    if (!ctx || !grads || n_grads == 0 || !total_norm) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Compute total norm across all gradients
    float norm_sq = 0.0f;
    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;

    for (size_t i = 0; i < n_grads; i++) {
        float grad_norm;
        cublasSnrm2(handle, grads[i]->numel, (const float*)grads[i]->data, 1, &grad_norm);
        norm_sq += grad_norm * grad_norm;
    }

    *total_norm = sqrtf(norm_sq);

    // Scale if exceeds max_norm
    if (*total_norm > max_norm) {
        float scale = max_norm / (*total_norm + 1e-6f);
        for (size_t i = 0; i < n_grads; i++) {
            nimcp_gpu_gradient_scale(ctx, grads[i], scale);
        }
    }

    return true;
}

__global__ void kernel_gradient_clip_value(float* grad, float clip_value, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        grad[idx] = fminf(fmaxf(grad[idx], -clip_value), clip_value);
    }
}

bool nimcp_gpu_gradient_clip_value(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad,
    float clip_value)
{
    if (!ctx || !grad) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_gradient_clip_value<<<GRID_SIZE(grad->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (float*)grad->data, clip_value, grad->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_gradient_zero(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad)
{
    if (!ctx || !grad) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    NIMCP_CUDA_RECOVER(cudaMemset(grad->data, 0, grad->numel * grad->elem_size), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/training/nimcp_training_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "GRADIENT_GPU"

bool nimcp_gpu_loss_mse(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad)
{
    LOG_WARN("CUDA not available - loss computation requires GPU");
    return false;
}

bool nimcp_gpu_loss_mae(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad)
{
    return false;
}

bool nimcp_gpu_loss_cross_entropy(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* logits,
    const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad, int reduction)
{
    return false;
}

bool nimcp_gpu_loss_bce(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad)
{
    return false;
}

bool nimcp_gpu_loss_focal(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad,
    float alpha, float gamma)
{
    return false;
}

bool nimcp_gpu_loss_huber(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target, float* loss, nimcp_gpu_tensor_t* grad, float delta)
{
    return false;
}

bool nimcp_gpu_gradient_accumulate(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* grad,
    nimcp_gpu_tensor_t* accum)
{
    return false;
}

bool nimcp_gpu_gradient_scale(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* grad, float scale)
{
    return false;
}

bool nimcp_gpu_gradient_clip_norm(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t** grads,
    size_t n_grads, float max_norm, float* total_norm)
{
    return false;
}

bool nimcp_gpu_gradient_clip_value(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* grad,
    float clip_value)
{
    return false;
}

bool nimcp_gpu_gradient_zero(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* grad)
{
    return false;
}

#endif // NIMCP_ENABLE_CUDA
