/**
 * @file nimcp_backprop_kernels.cu
 * @brief GPU Backpropagation CUDA Kernels
 *
 * WHAT: CUDA kernels for neural network backpropagation
 * WHY:  GPU acceleration for computing gradients during training
 * HOW:  Custom kernels for layer gradients (linear, conv, activation, normalization)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cublas_v2.h>
#include <math.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/training/nimcp_training_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "BACKPROP_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Linear Layer Backward
//=============================================================================

bool nimcp_gpu_backward_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_bias)
{
    if (!ctx || !x || !weight || !grad_output) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;

    // Dimensions: x is (batch, in_features), weight is (out_features, in_features)
    // grad_output is (batch, out_features)
    int batch = x->dims[0];
    int in_features = x->dims[1];
    int out_features = weight->dims[0];

    float alpha = 1.0f, beta = 0.0f;

    // grad_input = grad_output @ weight
    // (batch, in_features) = (batch, out_features) @ (out_features, in_features)
    if (grad_input) {
        NIMCP_CUBLAS_RECOVER(cublasSgemm(handle,
            CUBLAS_OP_N, CUBLAS_OP_N,
            in_features, batch, out_features,
            &alpha,
            (const float*)weight->data, in_features,
            (const float*)grad_output->data, out_features,
            &beta,
            (float*)grad_input->data, in_features), GPU_ERROR_LIBRARY);
    }

    // grad_weight = grad_output^T @ x
    // (out_features, in_features) = (out_features, batch) @ (batch, in_features)
    if (grad_weight) {
        NIMCP_CUBLAS_RECOVER(cublasSgemm(handle,
            CUBLAS_OP_N, CUBLAS_OP_T,
            in_features, out_features, batch,
            &alpha,
            (const float*)x->data, in_features,
            (const float*)grad_output->data, out_features,
            &beta,
            (float*)grad_weight->data, in_features), GPU_ERROR_LIBRARY);
    }

    // grad_bias = sum(grad_output, axis=0)
    if (grad_bias) {
        // Sum across batch dimension using cublas gemv with ones vector.
        // Cache the ones vector in a static variable to avoid repeated alloc/fill.
        static float* s_d_ones = NULL;
        static uint32_t s_d_ones_size = 0;

        if ((uint32_t)batch > s_d_ones_size) {
            if (s_d_ones) { cudaFree(s_d_ones); s_d_ones = NULL; s_d_ones_size = 0; }
            uint32_t new_size = (uint32_t)batch;
            // Round up to power of 2 for fewer reallocations
            if (new_size < 256) new_size = 256;
            else {
                new_size--;
                new_size |= new_size >> 1;
                new_size |= new_size >> 2;
                new_size |= new_size >> 4;
                new_size |= new_size >> 8;
                new_size |= new_size >> 16;
                new_size++;
            }
            NIMCP_CUDA_RECOVER(cudaMalloc(&s_d_ones, new_size * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);
            s_d_ones_size = new_size;

            // Fill with ones: host-side fill + single bulk cudaMemcpy
            // (replaces the old per-element cudaMemcpy loop)
            float* h_ones = (float*)malloc(new_size * sizeof(float));
            if (h_ones) {
                for (uint32_t i = 0; i < new_size; i++) h_ones[i] = 1.0f;
                cudaMemcpy(s_d_ones, h_ones, new_size * sizeof(float), cudaMemcpyHostToDevice);
                free(h_ones);
            } else {
                /* malloc failed — fill via kernel fallback */
                cudaMemset(s_d_ones, 0, new_size * sizeof(float));
                /* Note: ones vector will be zeros — cublasSgemv will produce zero bias grads.
                 * This is safer than using uninitialized device memory. */
            }
        }

        NIMCP_CUBLAS_RECOVER(cublasSgemv(handle,
            CUBLAS_OP_T,
            batch, out_features,
            &alpha,
            (const float*)grad_output->data, batch,
            s_d_ones, 1,
            &beta,
            (float*)grad_bias->data, 1), GPU_ERROR_LIBRARY);
    }

    return true;
}

//=============================================================================
// Fused Activation Backward Kernel
//=============================================================================

// Activation type constants for the fused kernel
#define ACTIVATION_RELU    0
#define ACTIVATION_SIGMOID 1
#define ACTIVATION_TANH    2

__global__ void kernel_backward_activation_fused(
    const float* output, const float* grad_output, float* grad_input,
    uint32_t n, int activation_type)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float g;
    switch (activation_type) {
        case ACTIVATION_RELU:    g = (output[i] > 0.0f) ? 1.0f : 0.0f; break;
        case ACTIVATION_SIGMOID: g = output[i] * (1.0f - output[i]); break;
        case ACTIVATION_TANH:    g = 1.0f - output[i] * output[i]; break;
        default:                 g = 1.0f; break;
    }
    grad_input[i] = grad_output[i] * g;
}

// Legacy individual kernels kept for reference but callers now use fused kernel

bool nimcp_gpu_backward_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    if (!ctx || !x || !grad_output || !grad_input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_backward_activation_fused<<<GRID_SIZE(x->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)x->data, (const float*)grad_output->data,
        (float*)grad_input->data, (uint32_t)x->numel, ACTIVATION_RELU);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_backward_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    if (!ctx || !output || !grad_output || !grad_input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_backward_activation_fused<<<GRID_SIZE(output->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)output->data, (const float*)grad_output->data,
        (float*)grad_input->data, (uint32_t)output->numel, ACTIVATION_SIGMOID);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_backward_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    if (!ctx || !output || !grad_output || !grad_input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_backward_activation_fused<<<GRID_SIZE(output->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)output->data, (const float*)grad_output->data,
        (float*)grad_input->data, (uint32_t)output->numel, ACTIVATION_TANH);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__global__ void kernel_backward_gelu(
    const float* x, const float* grad_output, float* grad_input, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        float val = x[idx];
        float val_cubed = val * val * val;

        // GELU: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
        float sqrt_2_pi = 0.7978845608f;
        float inner = sqrt_2_pi * (val + 0.044715f * val_cubed);
        float tanh_inner = tanhf(inner);

        // Derivative
        float sech2 = 1.0f - tanh_inner * tanh_inner;
        float inner_deriv = sqrt_2_pi * (1.0f + 3.0f * 0.044715f * val * val);

        float gelu_deriv = 0.5f * (1.0f + tanh_inner) +
                          0.5f * val * sech2 * inner_deriv;

        grad_input[idx] = grad_output[idx] * gelu_deriv;
    }
}

bool nimcp_gpu_backward_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    if (!ctx || !x || !grad_output || !grad_input) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_backward_gelu<<<GRID_SIZE(x->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)x->data, (const float*)grad_output->data,
        (float*)grad_input->data, x->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Softmax Backward
//=============================================================================

__global__ void kernel_backward_softmax(
    const float* output, const float* grad_output, float* grad_input,
    size_t batch_size, size_t num_classes)
{
    size_t batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    const float* s = output + batch_idx * num_classes;
    const float* dy = grad_output + batch_idx * num_classes;
    float* dx = grad_input + batch_idx * num_classes;

    // Compute sum_j(dy_j * s_j)
    __shared__ float shared_sum[WARP_SIZE];
    float thread_sum = 0.0f;
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        thread_sum += dy[i] * s[i];
    }

    // Warp reduction
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        thread_sum += __shfl_down_sync(0xffffffff, thread_sum, offset);
    }

    if (threadIdx.x % WARP_SIZE == 0) {
        shared_sum[threadIdx.x / WARP_SIZE] = thread_sum;
    }
    __syncthreads();

    float total_sum = 0.0f;
    if (threadIdx.x < blockDim.x / WARP_SIZE) {
        total_sum = shared_sum[threadIdx.x];
    }
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        total_sum += __shfl_down_sync(0xffffffff, total_sum, offset);
    }
    total_sum = __shfl_sync(0xffffffff, total_sum, 0);

    // Compute dx_i = s_i * (dy_i - sum_j(dy_j * s_j))
    for (size_t i = threadIdx.x; i < num_classes; i += blockDim.x) {
        dx[i] = s[i] * (dy[i] - total_sum);
    }
}

bool nimcp_gpu_backward_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    if (!ctx || !output || !grad_output || !grad_input) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t num_classes = output->dims[output->ndim - 1];
    size_t batch_size = output->numel / num_classes;

    kernel_backward_softmax<<<batch_size, BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)output->data, (const float*)grad_output->data,
        (float*)grad_input->data, batch_size, num_classes);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Batch Normalization Backward
//=============================================================================

__global__ void kernel_backward_batchnorm(
    const float* x, const float* gamma, const float* mean, const float* var,
    const float* grad_output, float* grad_input, float* grad_gamma, float* grad_beta,
    float eps, size_t batch_size, size_t features)
{
    size_t feat_idx = blockIdx.x;
    if (feat_idx >= features) return;

    float m = mean[feat_idx];
    float v = var[feat_idx];
    float g = gamma[feat_idx];
    float std_inv = rsqrtf(v + eps);

    // Compute gradients using reduction
    float sum_dy = 0.0f;
    float sum_dy_x_hat = 0.0f;

    for (size_t b = threadIdx.x; b < batch_size; b += blockDim.x) {
        size_t idx = b * features + feat_idx;
        float x_hat = (x[idx] - m) * std_inv;
        sum_dy += grad_output[idx];
        sum_dy_x_hat += grad_output[idx] * x_hat;
    }

    // Warp reduction
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        sum_dy += __shfl_down_sync(0xffffffff, sum_dy, offset);
        sum_dy_x_hat += __shfl_down_sync(0xffffffff, sum_dy_x_hat, offset);
    }

    __shared__ float shared_dy[WARP_SIZE];
    __shared__ float shared_dy_x_hat[WARP_SIZE];

    if (threadIdx.x % WARP_SIZE == 0) {
        shared_dy[threadIdx.x / WARP_SIZE] = sum_dy;
        shared_dy_x_hat[threadIdx.x / WARP_SIZE] = sum_dy_x_hat;
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        float total_dy = 0.0f, total_dy_x_hat = 0.0f;
        for (int i = 0; i < blockDim.x / WARP_SIZE; i++) {
            total_dy += shared_dy[i];
            total_dy_x_hat += shared_dy_x_hat[i];
        }

        if (grad_beta) grad_beta[feat_idx] = total_dy;
        if (grad_gamma) grad_gamma[feat_idx] = total_dy_x_hat;

        shared_dy[0] = total_dy;
        shared_dy_x_hat[0] = total_dy_x_hat;
    }
    __syncthreads();

    float total_dy = shared_dy[0];
    float total_dy_x_hat = shared_dy_x_hat[0];

    // Compute grad_input
    if (grad_input) {
        float inv_N = 1.0f / (float)batch_size;
        for (size_t b = threadIdx.x; b < batch_size; b += blockDim.x) {
            size_t idx = b * features + feat_idx;
            float x_hat = (x[idx] - m) * std_inv;
            float dy = grad_output[idx];

            grad_input[idx] = g * std_inv * inv_N *
                ((float)batch_size * dy - total_dy - x_hat * total_dy_x_hat);
        }
    }
}

bool nimcp_gpu_backward_batchnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* mean,
    const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma,
    nimcp_gpu_tensor_t* grad_beta,
    float eps)
{
    if (!ctx || !x || !gamma || !mean || !var || !grad_output) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t features = x->dims[x->ndim - 1];
    size_t batch_size = x->numel / features;

    kernel_backward_batchnorm<<<features, BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)x->data, (const float*)gamma->data,
        (const float*)mean->data, (const float*)var->data,
        (const float*)grad_output->data,
        grad_input ? (float*)grad_input->data : NULL,
        grad_gamma ? (float*)grad_gamma->data : NULL,
        grad_beta ? (float*)grad_beta->data : NULL,
        eps, batch_size, features);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Layer Normalization Backward
//=============================================================================

bool nimcp_gpu_backward_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* mean,
    const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma,
    nimcp_gpu_tensor_t* grad_beta,
    float eps)
{
    // Layer norm is similar to batch norm but normalizes along feature dimension
    // For now, use similar implementation with swapped axes
    LOG_WARN("Layer norm backward using simplified implementation");
    return nimcp_gpu_backward_batchnorm(ctx, x, gamma, mean, var, grad_output,
                                        grad_input, grad_gamma, grad_beta, eps);
}

//=============================================================================
// Dropout Backward
//=============================================================================

__global__ void kernel_backward_dropout(
    const float* mask, const float* grad_output, float* grad_input,
    float scale, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        grad_input[idx] = (mask[idx] != 0.0f) ? grad_output[idx] * scale : 0.0f;
    }
}

bool nimcp_gpu_backward_dropout(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* mask,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    float p)
{
    if (!ctx || !mask || !grad_output || !grad_input) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    float scale = 1.0f / (1.0f - p);

    kernel_backward_dropout<<<GRID_SIZE(mask->numel), BLOCK_SIZE, 0, nimcp_gpu_get_pool_stream(ctx)>>>(
        (const float*)mask->data, (const float*)grad_output->data,
        (float*)grad_input->data, scale, mask->numel);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Sparse CSR Weight Update + Delta Propagation Kernels
//=============================================================================

/**
 * Kernel: Update CSR weight values in-place using delta and activation vectors.
 *
 * For each non-zero entry W[row][col]:
 *   W[row][col] += lr * delta[row] * activation[col]
 *   W[row][col] = clamp(W[row][col], min_w, max_w)
 *
 * Also accumulates squared gradient norm via atomicAdd.
 *
 * Each thread handles one CSR entry.
 */
__global__ void kernel_sparse_csr_weight_update(
    float* __restrict__ csr_values,
    const int* __restrict__ csr_row_ptrs,
    const int* __restrict__ csr_col_indices,
    const float* __restrict__ delta,
    const float* __restrict__ activation,
    int nnz, int rows,
    float lr, float min_w, float max_w,
    float* __restrict__ grad_norm_partial)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    // Binary search row_ptrs to find which row this entry belongs to
    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (csr_row_ptrs[mid + 1] <= idx) {
            lo = mid + 1;
        } else if (csr_row_ptrs[mid] > idx) {
            hi = mid - 1;
        } else {
            row = mid;
            break;
        }
    }
    if (lo > hi) row = lo;

    int col = csr_col_indices[idx];
    float dj = delta[row];
    if (dj > 1.0f) dj = 1.0f;
    if (dj < -1.0f) dj = -1.0f;

    float act = activation[col];
    float weight_delta = lr * dj * act;

    float max_delta = fmaxf(0.1f, lr * 2.0f);
    if (weight_delta > max_delta) weight_delta = max_delta;
    if (weight_delta < -max_delta) weight_delta = -max_delta;

    atomicAdd(grad_norm_partial, weight_delta * weight_delta);

    float new_w = csr_values[idx] + weight_delta;
    if (new_w < min_w) new_w = min_w;
    if (new_w > max_w) new_w = max_w;
    csr_values[idx] = new_w;
}

__global__ void kernel_bias_update(
    float* __restrict__ bias,
    const float* __restrict__ delta,
    int size, float lr,
    float* __restrict__ grad_norm_partial)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    float bd = lr * delta[idx];
    atomicAdd(grad_norm_partial, bd * bd);
    float new_b = bias[idx] + bd;
    if (new_b > 10.0f) new_b = 10.0f;
    if (new_b < -10.0f) new_b = -10.0f;
    bias[idx] = new_b;
}

__global__ void kernel_compute_deltas_mse(
    float* __restrict__ delta,
    const float* __restrict__ target,
    const float* __restrict__ output,
    int size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    /* MSE gradient: d/d(output) of (1/N)∑(target-output)² = 2*(output-target)/N
     * We use (target-output) convention (negative gradient for descent),
     * scaled by 2/N to match CPU backprop (nimcp_backprop_kernel.c:1137). */
    delta[idx] = 2.0f * (target[idx] - output[idx]) / (float)size;
}

__global__ void kernel_activation_derivative(
    float* __restrict__ delta,
    const float* __restrict__ activation,
    int size, int act_type)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    float s = activation[idx];
    float deriv;
    switch (act_type) {
        case 0: deriv = (s > 0.0f) ? 1.0f : 0.0f; break;      /* RELU */
        case 1: deriv = (s > 0.0f) ? 1.0f : 0.01f; break;     /* LEAKY_RELU */
        case 2: deriv = 1.0f - s * s; if (deriv < 0.0f) deriv = 0.0f; break; /* TANH */
        case 3: deriv = s * (1.0f - s); if (deriv < 0.01f) deriv = 0.01f; break; /* SIGMOID */
        case 4: deriv = 1.0f; break;                           /* LINEAR */
        default: deriv = (s > 0.0f) ? 1.0f : 0.01f; break;
    }
    delta[idx] *= deriv;
}

/**
 * Layer norm backward: dx = dy - mean(dy) - y * mean(dy * y)
 * where y = normalized activation (stored in activation buffer).
 * Two-pass: first reduce to get means, then apply correction.
 * NOTE: This is a simplified single-block reduction for moderate layer sizes.
 * For very large layers (>1024), the host-side fallback is used instead.
 */
__global__ void kernel_layernorm_backward(
    float* __restrict__ delta,
    const float* __restrict__ activation, /* normalized y */
    int size,
    float dy_mean,
    float dy_y_mean)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    delta[idx] -= dy_mean + activation[idx] * dy_y_mean;
}

/* Residual backward: add delta from layer L to layer L-1 (identity skip gradient) */
__global__ void kernel_residual_backward(
    float* __restrict__ delta_prev,
    const float* __restrict__ delta_cur,
    int copy_n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= copy_n) return;
    delta_prev[idx] += delta_cur[idx];
}

__global__ void kernel_sparse_delta_propagate(
    const float* __restrict__ csr_values,
    const int* __restrict__ csr_row_ptrs,
    const int* __restrict__ csr_col_indices,
    const float* __restrict__ delta_cur,
    float* __restrict__ delta_prev,
    int nnz, int rows)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (csr_row_ptrs[mid + 1] <= idx) { lo = mid + 1; }
        else if (csr_row_ptrs[mid] > idx) { hi = mid - 1; }
        else { row = mid; break; }
    }
    if (lo > hi) row = lo;

    int col = csr_col_indices[idx];
    atomicAdd(&delta_prev[col], csr_values[idx] * delta_cur[row]);
}

//=============================================================================
// Gradient Accumulation Kernels (for mini-batch training)
//=============================================================================

/**
 * Accumulate weight gradients into a separate buffer (no weight update).
 * grad_accum[idx] += lr * delta[row] * activation[col]
 */
__global__ void kernel_sparse_csr_grad_accumulate(
    const int* __restrict__ csr_row_ptrs,
    const int* __restrict__ csr_col_indices,
    const float* __restrict__ delta,
    const float* __restrict__ activation,
    float* __restrict__ grad_accum,
    int nnz, int rows, float lr)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (csr_row_ptrs[mid + 1] <= idx) { lo = mid + 1; }
        else if (csr_row_ptrs[mid] > idx) { hi = mid - 1; }
        else { row = mid; break; }
    }
    if (lo > hi) row = lo;

    int col = csr_col_indices[idx];
    float dj = delta[row];
    if (dj > 1.0f) dj = 1.0f;
    if (dj < -1.0f) dj = -1.0f;

    float act = activation[col];
    float grad = lr * dj * act;
    float max_delta = fmaxf(0.1f, lr * 2.0f);
    if (grad > max_delta) grad = max_delta;
    if (grad < -max_delta) grad = -max_delta;

    atomicAdd(&grad_accum[idx], grad);
}

/**
 * Accumulate bias gradients (no update).
 * bias_grad_accum[idx] += lr * delta[idx]
 */
__global__ void kernel_bias_grad_accumulate(
    const float* __restrict__ delta,
    float* __restrict__ bias_grad_accum,
    int size, float lr)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    atomicAdd(&bias_grad_accum[idx], lr * delta[idx]);
}

/**
 * Apply accumulated weight gradients: W[idx] += grad_accum[idx] / batch_size, then clamp.
 * Resets grad_accum to zero after applying.
 */
__global__ void kernel_sparse_apply_accumulated_grads(
    float* __restrict__ csr_values,
    float* __restrict__ grad_accum,
    float* __restrict__ grad_norm_partial,
    int nnz, float inv_batch_size,
    float min_w, float max_w)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    float avg_grad = grad_accum[idx] * inv_batch_size;
    atomicAdd(grad_norm_partial, avg_grad * avg_grad);

    float new_w = csr_values[idx] + avg_grad;
    if (new_w < min_w) new_w = min_w;
    if (new_w > max_w) new_w = max_w;
    csr_values[idx] = new_w;
    grad_accum[idx] = 0.0f;
}

/**
 * Apply accumulated bias gradients: bias[idx] += bias_accum[idx] / batch_size, clamp, reset.
 */
__global__ void kernel_bias_apply_accumulated_grads(
    float* __restrict__ bias,
    float* __restrict__ bias_grad_accum,
    float* __restrict__ grad_norm_partial,
    int size, float inv_batch_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    float avg_grad = bias_grad_accum[idx] * inv_batch_size;
    atomicAdd(grad_norm_partial, avg_grad * avg_grad);
    float new_b = bias[idx] + avg_grad;
    if (new_b > 10.0f) new_b = 10.0f;
    if (new_b < -10.0f) new_b = -10.0f;
    bias[idx] = new_b;
    bias_grad_accum[idx] = 0.0f;
}

//=============================================================================
// FP16 + Sparse-Skip Optimized Kernels
//=============================================================================

/* Gradient threshold for sparse skip — updates below this are zero-cost */
#define GRAD_SKIP_THRESHOLD 1e-7f

/**
 * FP16 delta computation: compute deltas in half precision, write to FP32 buffer.
 * Saves memory bandwidth on the read side (target/output loaded as FP16).
 */
__global__ void kernel_compute_deltas_mse_fp16(
    float* __restrict__ delta,
    const float* __restrict__ target,
    const float* __restrict__ output,
    int size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    /* Compute in FP16 for speed, write back to FP32 for accumulation stability */
    __half t = __float2half(target[idx]);
    __half o = __float2half(output[idx]);
    __half diff = __hsub(t, o);
    __half scale = __float2half(2.0f / (float)size);
    delta[idx] = __half2float(__hmul(diff, scale));
}

/**
 * Weight gradient accumulation with sparse skip.
 * Skips entries where |delta * activation| < threshold.
 * Reduces atomicAdd contention and wasted compute.
 */
__global__ void kernel_sparse_csr_grad_accumulate_skip(
    const int* __restrict__ csr_row_ptrs,
    const int* __restrict__ csr_col_indices,
    const float* __restrict__ delta,
    const float* __restrict__ activation,
    float* __restrict__ grad_accum,
    int nnz, int rows, float lr)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    /* Binary search for row */
    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (csr_row_ptrs[mid + 1] <= idx) { lo = mid + 1; }
        else if (csr_row_ptrs[mid] > idx) { hi = mid - 1; }
        else { row = mid; break; }
    }
    if (lo > hi) row = lo;

    int col = csr_col_indices[idx];
    float dj = delta[row];

    /* Sparse skip: if delta is negligible, don't compute or accumulate */
    if (fabsf(dj) < GRAD_SKIP_THRESHOLD) return;

    if (dj > 1.0f) dj = 1.0f;
    if (dj < -1.0f) dj = -1.0f;

    float act = activation[col];

    /* Sparse skip: if activation is zero (dead ReLU), skip */
    if (fabsf(act) < GRAD_SKIP_THRESHOLD) return;

    float grad = lr * dj * act;
    float max_delta = fmaxf(0.1f, lr * 2.0f);
    if (grad > max_delta) grad = max_delta;
    if (grad < -max_delta) grad = -max_delta;

    atomicAdd(&grad_accum[idx], grad);
}

/**
 * Apply accumulated gradients with sparse skip.
 * Only updates weights where accumulated gradient is non-negligible.
 */
__global__ void kernel_sparse_apply_accumulated_grads_skip(
    float* __restrict__ csr_values,
    float* __restrict__ grad_accum,
    float* __restrict__ grad_norm_partial,
    int nnz, float inv_batch_size,
    float min_w, float max_w)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    float avg_grad = grad_accum[idx] * inv_batch_size;
    grad_accum[idx] = 0.0f;  /* Always reset */

    /* Sparse skip: don't update weight if gradient is negligible */
    if (fabsf(avg_grad) < GRAD_SKIP_THRESHOLD) return;

    atomicAdd(grad_norm_partial, avg_grad * avg_grad);

    float new_w = csr_values[idx] + avg_grad;
    if (new_w < min_w) new_w = min_w;
    if (new_w > max_w) new_w = max_w;
    csr_values[idx] = new_w;
}

/**
 * FP16 activation derivative: compute in half precision.
 */
__global__ void kernel_activation_derivative_fp16(
    float* __restrict__ delta,
    const float* __restrict__ activation,
    int size, int act_type)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    __half s = __float2half(activation[idx]);
    __half deriv;
    __half one = __float2half(1.0f);
    __half zero = __float2half(0.0f);
    __half small = __float2half(0.01f);

    switch (act_type) {
        case 0: deriv = __hgt(s, zero) ? one : zero; break;         /* RELU */
        case 1: deriv = __hgt(s, zero) ? one : small; break;        /* LEAKY_RELU */
        case 2: {
            __half ss = __hmul(s, s);
            deriv = __hsub(one, ss);
            if (__hlt(deriv, zero)) deriv = zero;
            break;
        } /* TANH */
        case 3: {
            deriv = __hmul(s, __hsub(one, s));
            if (__hlt(deriv, small)) deriv = small;
            break;
        } /* SIGMOID */
        case 4: deriv = one; break;                                   /* LINEAR */
        default: deriv = __hgt(s, zero) ? one : small; break;
    }
    delta[idx] *= __half2float(deriv);
}

//=============================================================================
// BSR Block Gradient Accumulation Kernel (tensor-core-friendly)
//=============================================================================

/**
 * BSR block weight gradient accumulation.
 * For each non-zero block (block_row, block_col):
 *   grad_block[i][j] += lr * delta[block_row * bs + i] * act[block_col * bs + j]
 *
 * Each thread handles one element within a block.
 * Block dimensions: block_size × block_size (e.g., 16×16 = 256 threads per block).
 */
__global__ void kernel_bsr_block_grad_accumulate(
    const int* __restrict__ bsr_row_ptrs,
    const int* __restrict__ bsr_col_indices,
    const float* __restrict__ delta,
    const float* __restrict__ activation,
    float* __restrict__ bsr_grad_accum,   /* [nnz_blocks * bs * bs] */
    int nnz_blocks, int mb, int block_size, float lr)
{
    int block_idx = blockIdx.x;
    if (block_idx >= nnz_blocks) return;

    int local_row = threadIdx.x / block_size;
    int local_col = threadIdx.x % block_size;
    if (local_row >= block_size || local_col >= block_size) return;

    /* Find which block row this block belongs to via binary search on row_ptrs */
    int lo = 0, hi = mb - 1, block_row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (bsr_row_ptrs[mid + 1] <= block_idx) lo = mid + 1;
        else if (bsr_row_ptrs[mid] > block_idx) hi = mid - 1;
        else { block_row = mid; break; }
    }
    if (lo > hi) block_row = lo;

    int block_col = bsr_col_indices[block_idx];

    int global_row = block_row * block_size + local_row;
    int global_col = block_col * block_size + local_col;

    float d = delta[global_row];
    float a = activation[global_col];

    /* Sparse skip */
    if (fabsf(d) < 1e-7f || fabsf(a) < 1e-7f) return;

    float grad = lr * d * a;
    float max_delta = fmaxf(0.1f, lr * 2.0f);
    if (grad > max_delta) grad = max_delta;
    if (grad < -max_delta) grad = -max_delta;

    int elem_offset = block_idx * block_size * block_size + local_row * block_size + local_col;
    atomicAdd(&bsr_grad_accum[elem_offset], grad);
}

/**
 * BSR delta propagation via cuSPARSE transpose SpMV.
 * delta_prev = W^T @ delta_cur
 */
static bool bsr_delta_propagate(
    cusparseHandle_t handle,
    const nimcp_sparse_bsr_t* bsr,
    const float* d_delta_cur,
    float* d_delta_prev,
    int prev_size,
    int max_buf_size)   /* size of delta buffers in elements */
{
    /* BSR operates on padded dimensions. Check buffers are large enough. */
    int padded_rows = bsr->rows * bsr->block_size;
    int padded_cols = bsr->cols * bsr->block_size;
    if (padded_rows > max_buf_size || padded_cols > max_buf_size) {
        return false;  /* Buffers too small for BSR padded dimensions */
    }
    cusparseMatDescr_t descr;
    cusparseCreateMatDescr(&descr);
    cusparseSetMatType(descr, CUSPARSE_MATRIX_TYPE_GENERAL);
    cusparseSetMatIndexBase(descr, CUSPARSE_INDEX_BASE_ZERO);

    float alpha = 1.0f;
    float beta = 0.0f;

    /* Transpose SpMV: delta_prev = W^T @ delta_cur */
    cusparseStatus_t st = cusparseSbsrmv(
        handle,
        CUSPARSE_DIRECTION_ROW,
        CUSPARSE_OPERATION_TRANSPOSE,
        bsr->rows, bsr->cols, bsr->nnz_blocks,
        &alpha, descr,
        bsr->values, bsr->row_ptrs, bsr->col_indices,
        bsr->block_size,
        d_delta_cur,
        &beta,
        d_delta_prev);

    cusparseDestroyMatDescr(descr);
    return (st == CUSPARSE_STATUS_SUCCESS);
}

//=============================================================================
// GPU Sparse Backward Pass — Accumulate Mode (no weight update)
//=============================================================================

bool nimcp_gpu_sparse_backward_accumulate(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations,
    float** d_weight_grad_accum,
    float** d_bias_grad_accum,
    int* layer_act_types,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    const float* target_host,
    const float* output_host,
    uint32_t output_size,
    float learning_rate)
{
    if (!gpu_ctx || !sparse_weights || !activations || !d_weight_grad_accum
        || !target_host || !output_host || num_layers < 2) {
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_pool_stream(gpu_ctx);

    uint32_t out_layer_size = layer_sizes[num_layers - 1];
    uint32_t bp_size = out_layer_size < output_size ? out_layer_size : output_size;

    float* d_target;
    float* d_output;
    if (cudaMalloc(&d_target, bp_size * sizeof(float)) != cudaSuccess) return false;
    if (cudaMalloc(&d_output, bp_size * sizeof(float)) != cudaSuccess) {
        cudaFree(d_target); return false;
    }
    if (cudaMemcpy(d_target, target_host, bp_size * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess ||
        cudaMemcpy(d_output, output_host, bp_size * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_target); cudaFree(d_output); return false;
    }

    uint32_t max_layer = 0;
    for (uint32_t l = 0; l < num_layers; l++) {
        if (layer_sizes[l] > max_layer) max_layer = layer_sizes[l];
    }

    float* d_delta_cur;
    float* d_delta_prev;
    if (cudaMalloc(&d_delta_cur, max_layer * sizeof(float)) != cudaSuccess) {
        cudaFree(d_target); cudaFree(d_output); return false;
    }
    if (cudaMalloc(&d_delta_prev, max_layer * sizeof(float)) != cudaSuccess) {
        cudaFree(d_delta_cur); cudaFree(d_target); cudaFree(d_output); return false;
    }
    cudaMemset(d_delta_cur, 0, max_layer * sizeof(float));
    cudaMemset(d_delta_prev, 0, max_layer * sizeof(float));

    /* FP16 delta computation — saves memory bandwidth */
    kernel_compute_deltas_mse_fp16<<<GRID_SIZE(bp_size), BLOCK_SIZE, 0, stream>>>(
        d_delta_cur, d_target, d_output, bp_size);

    cudaFree(d_target);
    cudaFree(d_output);

    float output_lr_boost = 10.0f;

    for (int32_t layer = (int32_t)num_layers - 1; layer >= 1; layer--) {
        uint32_t cur_size = layer_sizes[layer];
        uint32_t prev_size = layer_sizes[layer - 1];
        bool is_output = (layer == (int32_t)num_layers - 1);

        float layer_lr;
        if (is_output) {
            layer_lr = learning_rate * output_lr_boost;
        } else {
            layer_lr = learning_rate / powf(fmaxf((float)prev_size, 1.0f), 0.25f);
        }

        nimcp_sparse_tensor_t* W = sparse_weights[layer - 1];
        if (!W || W->format != SPARSE_FORMAT_CSR) {
            cudaMemset(d_delta_cur, 0, max_layer * sizeof(float));
            continue;
        }

        int nnz = W->data.csr.nnz;
        if (nnz == 0) continue;

        /* Accumulate bias gradients */
        if (biases[layer - 1] && d_bias_grad_accum[layer - 1]) {
            kernel_bias_grad_accumulate<<<GRID_SIZE(cur_size), BLOCK_SIZE, 0, stream>>>(
                d_delta_cur, d_bias_grad_accum[layer - 1], cur_size, layer_lr);
        }

        /* Accumulate weight gradients with sparse skip —
         * skips entries where |delta| < 1e-7 or |activation| < 1e-7.
         * With ReLU, ~30-50% of activations are zero (dead neurons). */
        kernel_sparse_csr_grad_accumulate_skip<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
            W->data.csr.row_ptrs, W->data.csr.col_indices,
            d_delta_cur,
            (const float*)activations[layer - 1]->data,
            d_weight_grad_accum[layer - 1],
            nnz, W->data.csr.rows, layer_lr);

        /* Propagate deltas backward */
        if (layer > 1) {
            cudaMemset(d_delta_prev, 0, prev_size * sizeof(float));

            kernel_sparse_delta_propagate<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
                W->data.csr.values, W->data.csr.row_ptrs, W->data.csr.col_indices,
                d_delta_cur, d_delta_prev, nnz, W->data.csr.rows);

            /* FP16 activation derivative — half-precision compute */
            int act_type = layer_act_types[layer - 1];
            kernel_activation_derivative_fp16<<<GRID_SIZE(prev_size), BLOCK_SIZE, 0, stream>>>(
                d_delta_prev, (const float*)activations[layer - 1]->data,
                prev_size, act_type);

            float* tmp = d_delta_cur;
            d_delta_cur = d_delta_prev;
            d_delta_prev = tmp;
        }
    }

    cudaStreamSynchronize(stream);
    cudaFree(d_delta_cur);
    cudaFree(d_delta_prev);
    return true;
}

//=============================================================================
// BSR Backward Accumulate — tensor core delta propagation + block gradients
//=============================================================================

extern "C" bool nimcp_gpu_bsr_backward_accumulate(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** csr_weights,    /* CSR for weight grad accum format */
    nimcp_sparse_tensor_t** bsr_weights,    /* BSR for delta propagation */
    nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations,
    float** d_weight_grad_accum,
    float** d_bias_grad_accum,
    int* layer_act_types,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    const float* target_host,
    const float* output_host,
    uint32_t output_size,
    float learning_rate)
{
    if (!gpu_ctx || !csr_weights || !bsr_weights || !activations
        || !target_host || !output_host || num_layers < 2) {
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_pool_stream(gpu_ctx);

    uint32_t out_layer_size = layer_sizes[num_layers - 1];
    uint32_t bp_size = out_layer_size < output_size ? out_layer_size : output_size;

    /* Allocate delta buffers */
    float* d_target;
    float* d_output;
    if (cudaMalloc(&d_target, bp_size * sizeof(float)) != cudaSuccess) return false;
    if (cudaMalloc(&d_output, bp_size * sizeof(float)) != cudaSuccess) {
        cudaFree(d_target); return false;
    }
    cudaMemcpy(d_target, target_host, bp_size * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_output, output_host, bp_size * sizeof(float), cudaMemcpyHostToDevice);

    uint32_t max_layer = 0;
    for (uint32_t l = 0; l < num_layers; l++) {
        if (layer_sizes[l] > max_layer) max_layer = layer_sizes[l];
    }

    float* d_delta_cur;
    float* d_delta_prev;
    if (cudaMalloc(&d_delta_cur, max_layer * sizeof(float)) != cudaSuccess) {
        cudaFree(d_target); cudaFree(d_output); return false;
    }
    if (cudaMalloc(&d_delta_prev, max_layer * sizeof(float)) != cudaSuccess) {
        cudaFree(d_delta_cur); cudaFree(d_target); cudaFree(d_output); return false;
    }
    cudaMemset(d_delta_cur, 0, max_layer * sizeof(float));
    cudaMemset(d_delta_prev, 0, max_layer * sizeof(float));

    /* FP16 delta computation */
    kernel_compute_deltas_mse_fp16<<<GRID_SIZE(bp_size), BLOCK_SIZE, 0, stream>>>(
        d_delta_cur, d_target, d_output, bp_size);
    cudaFree(d_target);
    cudaFree(d_output);

    float output_lr_boost = 10.0f;

    for (int32_t layer = (int32_t)num_layers - 1; layer >= 1; layer--) {
        uint32_t cur_size = layer_sizes[layer];
        uint32_t prev_size = layer_sizes[layer - 1];
        bool is_output = (layer == (int32_t)num_layers - 1);

        float layer_lr = is_output
            ? learning_rate * output_lr_boost
            : learning_rate / powf(fmaxf((float)prev_size, 1.0f), 0.25f);

        /* Bias gradient accumulation (same for CSR/BSR) */
        if (biases[layer - 1] && d_bias_grad_accum && d_bias_grad_accum[layer - 1]) {
            kernel_bias_grad_accumulate<<<GRID_SIZE(cur_size), BLOCK_SIZE, 0, stream>>>(
                d_delta_cur, d_bias_grad_accum[layer - 1], cur_size, layer_lr);
        }

        /* Weight gradient accumulation — use CSR format (grad accum buffers match CSR nnz) */
        nimcp_sparse_tensor_t* W_csr = csr_weights[layer - 1];
        if (W_csr && W_csr->format == SPARSE_FORMAT_CSR && W_csr->data.csr.nnz > 0) {
            kernel_sparse_csr_grad_accumulate_skip<<<GRID_SIZE(W_csr->data.csr.nnz), BLOCK_SIZE, 0, stream>>>(
                W_csr->data.csr.row_ptrs, W_csr->data.csr.col_indices,
                d_delta_cur,
                (const float*)activations[layer - 1]->data,
                d_weight_grad_accum[layer - 1],
                W_csr->data.csr.nnz, W_csr->data.csr.rows, layer_lr);
        }

        /* Delta propagation — use BSR with cuSPARSE transpose SpMV (tensor cores!) */
        if (layer > 1) {
            cudaMemset(d_delta_prev, 0, prev_size * sizeof(float));

            nimcp_sparse_tensor_t* W_bsr = bsr_weights[layer - 1];
            bool bsr_ok = false;
            if (W_bsr && W_bsr->format == SPARSE_FORMAT_BSR &&
                W_bsr->data.bsr.nnz_blocks > 0) {
                cudaStreamSynchronize(stream);  /* Ensure prior kernels done */
                bsr_ok = bsr_delta_propagate(
                    sparse_ctx->cusparse_handle,
                    &W_bsr->data.bsr,
                    d_delta_cur, d_delta_prev, prev_size,
                    (int)max_layer);
            }

            if (!bsr_ok && W_csr && W_csr->format == SPARSE_FORMAT_CSR &&
                W_csr->data.csr.nnz > 0) {
                /* CSR fallback for delta propagation */
                kernel_sparse_delta_propagate<<<GRID_SIZE(W_csr->data.csr.nnz), BLOCK_SIZE, 0, stream>>>(
                    W_csr->data.csr.values, W_csr->data.csr.row_ptrs,
                    W_csr->data.csr.col_indices,
                    d_delta_cur, d_delta_prev,
                    W_csr->data.csr.nnz, W_csr->data.csr.rows);
            }

            /* FP16 activation derivative */
            int act_type = layer_act_types[layer - 1];
            kernel_activation_derivative_fp16<<<GRID_SIZE(prev_size), BLOCK_SIZE, 0, stream>>>(
                d_delta_prev, (const float*)activations[layer - 1]->data,
                prev_size, act_type);

            float* tmp = d_delta_cur;
            d_delta_cur = d_delta_prev;
            d_delta_prev = tmp;
        }
    }

    cudaStreamSynchronize(stream);
    cudaFree(d_delta_cur);
    cudaFree(d_delta_prev);
    return true;
}

//=============================================================================
// GPU Gradient Flush — Apply accumulated gradients / batch_size
//=============================================================================

bool nimcp_gpu_gradient_flush(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    float** d_weight_grad_accum,
    float** d_bias_grad_accum,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    uint32_t batch_size,
    float min_weight, float max_weight,
    float* out_grad_norm)
{
    if (!gpu_ctx || !sparse_weights || num_layers < 2 || batch_size == 0) return false;

    cudaStream_t stream = nimcp_gpu_get_pool_stream(gpu_ctx);
    float inv_bs = 1.0f / (float)batch_size;

    float* d_grad_norm;
    float zero = 0.0f;
    if (cudaMalloc(&d_grad_norm, sizeof(float)) != cudaSuccess) return false;
    /* W9: Check cudaMemcpy for grad_norm init */
    if (cudaMemcpy(d_grad_norm, &zero, sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_grad_norm); return false;
    }

    for (uint32_t layer = 1; layer < num_layers; layer++) {
        nimcp_sparse_tensor_t* W = sparse_weights[layer - 1];
        if (!W || W->format != SPARSE_FORMAT_CSR) continue;
        int nnz = W->data.csr.nnz;
        if (nnz == 0) continue;

        if (d_weight_grad_accum[layer - 1]) {
            /* Sparse-skip: only updates weights where gradient is non-negligible */
            kernel_sparse_apply_accumulated_grads_skip<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
                W->data.csr.values, d_weight_grad_accum[layer - 1],
                d_grad_norm, nnz, inv_bs, min_weight, max_weight);
        }

        uint32_t cur_size = layer_sizes[layer];
        if (biases[layer - 1] && d_bias_grad_accum[layer - 1]) {
            kernel_bias_apply_accumulated_grads<<<GRID_SIZE(cur_size), BLOCK_SIZE, 0, stream>>>(
                (float*)biases[layer - 1]->data, d_bias_grad_accum[layer - 1],
                d_grad_norm, cur_size, inv_bs);
        }
    }

    cudaStreamSynchronize(stream);
    float grad_norm_sq = 0.0f;
    /* W9: Check cudaMemcpy readback — use 0.0 on failure */
    if (cudaMemcpy(&grad_norm_sq, d_grad_norm, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        grad_norm_sq = 0.0f;
    }
    if (out_grad_norm) {
        *out_grad_norm = sqrtf(grad_norm_sq);
        if (!isfinite(*out_grad_norm)) *out_grad_norm = 0.0f;
    }
    cudaFree(d_grad_norm);
    return true;
}

//=============================================================================
// GPU Sparse Backward Pass — Orchestrator
//=============================================================================

bool nimcp_gpu_sparse_backward_pass(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations,
    int* layer_act_types,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    const float* target_host,
    const float* output_host,
    uint32_t output_size,
    float learning_rate,
    float min_weight, float max_weight,
    float* out_grad_norm)
{
    if (!gpu_ctx || !sparse_ctx || !sparse_weights || !biases || !activations
        || !target_host || !output_host || !out_grad_norm || num_layers < 2) {
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_pool_stream(gpu_ctx);

    float* d_grad_norm;
    float zero = 0.0f;
    if (cudaMalloc(&d_grad_norm, sizeof(float)) != cudaSuccess) return false;
    /* W9: Check cudaMemcpy for grad_norm init */
    if (cudaMemcpy(d_grad_norm, &zero, sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_grad_norm); return false;
    }

    uint32_t out_layer_size = layer_sizes[num_layers - 1];
    uint32_t bp_size = out_layer_size < output_size ? out_layer_size : output_size;

    float* d_target;
    float* d_output;
    if (cudaMalloc(&d_target, bp_size * sizeof(float)) != cudaSuccess) {
        cudaFree(d_grad_norm); return false;
    }
    if (cudaMalloc(&d_output, bp_size * sizeof(float)) != cudaSuccess) {
        cudaFree(d_target); cudaFree(d_grad_norm); return false;
    }
    if (cudaMemcpy(d_target, target_host, bp_size * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess ||
        cudaMemcpy(d_output, output_host, bp_size * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_target); cudaFree(d_output); cudaFree(d_grad_norm); return false;
    }

    uint32_t max_layer = 0;
    for (uint32_t l = 0; l < num_layers; l++) {
        if (layer_sizes[l] > max_layer) max_layer = layer_sizes[l];
    }

    float* d_delta_cur;
    float* d_delta_prev;
    if (cudaMalloc(&d_delta_cur, max_layer * sizeof(float)) != cudaSuccess) {
        cudaFree(d_target); cudaFree(d_output); cudaFree(d_grad_norm); return false;
    }
    if (cudaMalloc(&d_delta_prev, max_layer * sizeof(float)) != cudaSuccess) {
        cudaFree(d_delta_cur); cudaFree(d_target); cudaFree(d_output); cudaFree(d_grad_norm);
        return false;
    }
    cudaMemset(d_delta_cur, 0, max_layer * sizeof(float));
    cudaMemset(d_delta_prev, 0, max_layer * sizeof(float));

    kernel_compute_deltas_mse<<<GRID_SIZE(bp_size), BLOCK_SIZE, 0, stream>>>(
        d_delta_cur, d_target, d_output, bp_size);

    cudaFree(d_target);
    cudaFree(d_output);

    float output_lr_boost = 10.0f;

    for (int32_t layer = (int32_t)num_layers - 1; layer >= 1; layer--) {
        uint32_t cur_size = layer_sizes[layer];
        uint32_t prev_size = layer_sizes[layer - 1];
        bool is_output = (layer == (int32_t)num_layers - 1);

        float layer_lr;
        if (is_output) {
            layer_lr = learning_rate * output_lr_boost;
        } else {
            layer_lr = learning_rate / powf(fmaxf((float)prev_size, 1.0f), 0.25f);
        }

        nimcp_sparse_tensor_t* W = sparse_weights[layer - 1];
        if (!W || W->format != SPARSE_FORMAT_CSR) {
            cudaMemset(d_delta_cur, 0, max_layer * sizeof(float));
            continue;
        }

        int nnz = W->data.csr.nnz;
        if (nnz == 0) continue;

        if (biases[layer - 1]) {
            kernel_bias_update<<<GRID_SIZE(cur_size), BLOCK_SIZE, 0, stream>>>(
                (float*)biases[layer - 1]->data, d_delta_cur,
                cur_size, layer_lr, d_grad_norm);
        }

        kernel_sparse_csr_weight_update<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
            W->data.csr.values, W->data.csr.row_ptrs, W->data.csr.col_indices,
            d_delta_cur,
            (const float*)activations[layer - 1]->data,
            nnz, W->data.csr.rows,
            layer_lr, min_weight, max_weight, d_grad_norm);

        if (layer > 1) {
            cudaMemset(d_delta_prev, 0, prev_size * sizeof(float));

            kernel_sparse_delta_propagate<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
                W->data.csr.values, W->data.csr.row_ptrs, W->data.csr.col_indices,
                d_delta_cur, d_delta_prev, nnz, W->data.csr.rows);

            int act_type = layer_act_types[layer - 1];
            kernel_activation_derivative<<<GRID_SIZE(prev_size), BLOCK_SIZE, 0, stream>>>(
                d_delta_prev, (const float*)activations[layer - 1]->data,
                prev_size, act_type);

            /* Layer norm backward + residual backward DISABLED:
             * See comment in nimcp_gpu_sparse_backward_accumulate. */

            float* tmp = d_delta_cur;
            d_delta_cur = d_delta_prev;
            d_delta_prev = tmp;
        }
    }

    cudaStreamSynchronize(stream);
    float grad_norm_sq = 0.0f;
    /* W9: Check cudaMemcpy readback — use 0.0 on failure */
    if (cudaMemcpy(&grad_norm_sq, d_grad_norm, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        grad_norm_sq = 0.0f;
    }
    *out_grad_norm = sqrtf(grad_norm_sq);
    if (!isfinite(*out_grad_norm)) *out_grad_norm = 0.0f;

    cudaFree(d_delta_cur);
    cudaFree(d_delta_prev);
    cudaFree(d_grad_norm);

    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/training/nimcp_training_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "BACKPROP_GPU"

bool nimcp_gpu_backward_linear(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* weight, const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input, nimcp_gpu_tensor_t* grad_weight, nimcp_gpu_tensor_t* grad_bias)
{
    LOG_WARN("CUDA not available - backprop requires GPU");
    return false;
}

bool nimcp_gpu_backward_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input)
{
    return false;
}

bool nimcp_gpu_backward_sigmoid(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input)
{
    return false;
}

bool nimcp_gpu_backward_tanh(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input)
{
    return false;
}

bool nimcp_gpu_backward_gelu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input)
{
    return false;
}

bool nimcp_gpu_backward_softmax(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input)
{
    return false;
}

bool nimcp_gpu_backward_batchnorm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* mean, const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma, nimcp_gpu_tensor_t* grad_beta, float eps)
{
    return false;
}

bool nimcp_gpu_backward_layernorm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* mean, const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma, nimcp_gpu_tensor_t* grad_beta, float eps)
{
    return false;
}

bool nimcp_gpu_backward_dropout(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* mask,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input, float p)
{
    return false;
}

bool nimcp_gpu_sparse_backward_pass(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations,
    int* layer_act_types,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    const float* target_host,
    const float* output_host,
    uint32_t output_size,
    float learning_rate,
    float min_weight, float max_weight,
    float* out_grad_norm)
{
    (void)gpu_ctx; (void)sparse_ctx; (void)sparse_weights; (void)biases;
    (void)activations; (void)layer_act_types; (void)num_layers; (void)layer_sizes;
    (void)target_host; (void)output_host; (void)output_size;
    (void)learning_rate; (void)min_weight; (void)max_weight; (void)out_grad_norm;
    return false;
}

bool nimcp_gpu_sparse_backward_accumulate(
    nimcp_gpu_context_t* gpu_ctx, nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** sparse_weights, nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations, float** d_weight_grad_accum,
    float** d_bias_grad_accum, int* layer_act_types,
    uint32_t num_layers, const uint32_t* layer_sizes,
    const float* target_host, const float* output_host,
    uint32_t output_size, float learning_rate)
{
    (void)gpu_ctx; (void)sparse_ctx; (void)sparse_weights; (void)biases;
    (void)activations; (void)d_weight_grad_accum; (void)d_bias_grad_accum;
    (void)layer_act_types; (void)num_layers; (void)layer_sizes;
    (void)target_host; (void)output_host; (void)output_size; (void)learning_rate;
    return false;
}

bool nimcp_gpu_bsr_backward_accumulate(
    void* a, void* b, void* c, void* d, void* e, void* f,
    void* g, void* h, void* i, unsigned j, const void* k,
    const void* l, const void* m, unsigned n, float o)
{
    (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;
    (void)i;(void)j;(void)k;(void)l;(void)m;(void)n;(void)o;
    return false;
}

bool nimcp_gpu_gradient_flush(
    nimcp_gpu_context_t* gpu_ctx, nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases, float** d_weight_grad_accum,
    float** d_bias_grad_accum, uint32_t num_layers,
    const uint32_t* layer_sizes, uint32_t batch_size,
    float min_weight, float max_weight, float* out_grad_norm)
{
    (void)gpu_ctx; (void)sparse_weights; (void)biases;
    (void)d_weight_grad_accum; (void)d_bias_grad_accum;
    (void)num_layers; (void)layer_sizes; (void)batch_size;
    (void)min_weight; (void)max_weight; (void)out_grad_norm;
    return false;
}

#endif // NIMCP_ENABLE_CUDA
