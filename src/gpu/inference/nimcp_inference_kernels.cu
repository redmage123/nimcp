//=============================================================================
// nimcp_inference_kernels.cu - GPU Inference Kernels Implementation
//=============================================================================
/**
 * @file nimcp_inference_kernels.cu
 * @brief CUDA and CPU implementations for optimized neural network inference
 *
 * WHAT: High-performance inference operations
 * WHY:  Enable real-time neural network inference
 * HOW:  Fused operations, quantization, CUDA graph capture
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

// Standard library headers (no extern "C" conflicts)
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LOG_MODULE "INFERENCE_GPU"

//=============================================================================
// CUDA Implementation
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cuda_fp16.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/inference/nimcp_inference_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "utils/logging/nimcp_logging.h"

//-----------------------------------------------------------------------------
// CUDA Kernels
//-----------------------------------------------------------------------------

/**
 * @brief Fused Linear + ReLU kernel
 */
__global__ void kernel_linear_relu_epilogue(
    float* output,
    const float* bias,
    size_t n_outputs,
    size_t batch_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_outputs * batch_size) return;

    size_t out_idx = idx % n_outputs;
    float val = output[idx];

    // Add bias if present
    if (bias) {
        val += bias[out_idx];
    }

    // ReLU activation
    output[idx] = (val > 0.0f) ? val : 0.0f;
}

/**
 * @brief Fused Linear + GELU kernel
 */
__global__ void kernel_linear_gelu_epilogue(
    float* output,
    const float* bias,
    size_t n_outputs,
    size_t batch_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_outputs * batch_size) return;

    size_t out_idx = idx % n_outputs;
    float x = output[idx];

    // Add bias if present
    if (bias) {
        x += bias[out_idx];
    }

    // Approximate GELU: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    const float sqrt_2_over_pi = 0.7978845608f;
    float x_cubed = x * x * x;
    float inner = sqrt_2_over_pi * (x + 0.044715f * x_cubed);
    output[idx] = 0.5f * x * (1.0f + tanhf(inner));
}

/**
 * @brief Fused Linear + SiLU/Swish kernel
 */
__global__ void kernel_linear_silu_epilogue(
    float* output,
    const float* bias,
    size_t n_outputs,
    size_t batch_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_outputs * batch_size) return;

    size_t out_idx = idx % n_outputs;
    float x = output[idx];

    // Add bias if present
    if (bias) {
        x += bias[out_idx];
    }

    // SiLU: x * sigmoid(x)
    float sigmoid_x = 1.0f / (1.0f + expf(-x));
    output[idx] = x * sigmoid_x;
}

/**
 * @brief INT8 quantization kernel
 */
__global__ void kernel_quantize_int8(
    const float* input,
    int8_t* output,
    float scale,
    int32_t zero_point,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float val = input[idx];
    int32_t quantized = __float2int_rn(val / scale) + zero_point;

    // Clamp to INT8 range
    quantized = max(-128, min(127, quantized));
    output[idx] = (int8_t)quantized;
}

/**
 * @brief INT8 dequantization kernel
 */
__global__ void kernel_dequantize_int8(
    const int8_t* input,
    float* output,
    float scale,
    int32_t zero_point,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    int32_t val = (int32_t)input[idx];
    output[idx] = scale * (float)(val - zero_point);
}

/**
 * @brief FP32 to FP16 conversion kernel
 */
__global__ void kernel_fp32_to_fp16(
    const float* __restrict__ input,
    half* __restrict__ output,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    output[idx] = __float2half(input[idx]);
}

/**
 * @brief FP16 to FP32 conversion kernel
 */
__global__ void kernel_fp16_to_fp32(
    const half* __restrict__ input,
    float* __restrict__ output,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    output[idx] = __half2float(input[idx]);
}

/**
 * @brief In-place activation kernel
 */
__global__ void kernel_activation_inplace(
    float* data,
    size_t n,
    int activation)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float x = data[idx];

    switch (activation) {
        case 0:  // ReLU
            data[idx] = (x > 0.0f) ? x : 0.0f;
            break;
        case 1:  // Sigmoid
            data[idx] = 1.0f / (1.0f + expf(-x));
            break;
        case 2:  // Tanh
            data[idx] = tanhf(x);
            break;
        case 3:  // GELU
            {
                const float sqrt_2_over_pi = 0.7978845608f;
                float x_cubed = x * x * x;
                float inner = sqrt_2_over_pi * (x + 0.044715f * x_cubed);
                data[idx] = 0.5f * x * (1.0f + tanhf(inner));
            }
            break;
        case 4:  // SiLU
            data[idx] = x / (1.0f + expf(-x));
            break;
    }
}

/**
 * @brief Residual add kernel
 */
__global__ void kernel_residual_add(
    const float* x,
    const float* residual,
    float* y,
    float alpha,
    float beta,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    y[idx] = alpha * x[idx] + beta * residual[idx];
}

/**
 * @brief Layer normalization kernel
 */
__global__ void kernel_layernorm(
    const float* input,
    const float* gamma,
    const float* beta,
    float* output,
    size_t batch_size,
    size_t hidden_size,
    float eps)
{
    extern __shared__ float shared[];
    float* s_sum = shared;
    float* s_sum_sq = shared + blockDim.x;

    size_t batch_idx = blockIdx.x;
    size_t tid = threadIdx.x;

    // Each block handles one sample
    const float* sample = input + batch_idx * hidden_size;
    float* out_sample = output + batch_idx * hidden_size;

    // Compute local sum and sum of squares
    float local_sum = 0.0f;
    float local_sum_sq = 0.0f;
    for (size_t i = tid; i < hidden_size; i += blockDim.x) {
        float val = sample[i];
        local_sum += val;
        local_sum_sq += val * val;
    }

    s_sum[tid] = local_sum;
    s_sum_sq[tid] = local_sum_sq;
    __syncthreads();

    // Parallel reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            s_sum[tid] += s_sum[tid + stride];
            s_sum_sq[tid] += s_sum_sq[tid + stride];
        }
        __syncthreads();
    }

    // Compute mean and variance
    float mean = s_sum[0] / hidden_size;
    float var = s_sum_sq[0] / hidden_size - mean * mean;
    float inv_std = rsqrtf(var + eps);

    // Normalize and apply gamma/beta
    for (size_t i = tid; i < hidden_size; i += blockDim.x) {
        float normalized = (sample[i] - mean) * inv_std;
        out_sample[i] = gamma[i] * normalized + beta[i];
    }
}

/**
 * @brief RMS normalization kernel
 */
__global__ void kernel_rmsnorm(
    const float* input,
    const float* gamma,
    float* output,
    size_t batch_size,
    size_t hidden_size,
    float eps)
{
    extern __shared__ float s_sum_sq[];

    size_t batch_idx = blockIdx.x;
    size_t tid = threadIdx.x;

    const float* sample = input + batch_idx * hidden_size;
    float* out_sample = output + batch_idx * hidden_size;

    // Compute sum of squares
    float local_sum_sq = 0.0f;
    for (size_t i = tid; i < hidden_size; i += blockDim.x) {
        float val = sample[i];
        local_sum_sq += val * val;
    }

    s_sum_sq[tid] = local_sum_sq;
    __syncthreads();

    // Parallel reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            s_sum_sq[tid] += s_sum_sq[tid + stride];
        }
        __syncthreads();
    }

    // Compute RMS and inverse
    float rms = sqrtf(s_sum_sq[0] / hidden_size + eps);
    float inv_rms = 1.0f / rms;

    // Normalize and apply gamma
    for (size_t i = tid; i < hidden_size; i += blockDim.x) {
        out_sample[i] = gamma[i] * sample[i] * inv_rms;
    }
}

/**
 * @brief Calibration kernel - compute min/max
 */
/**
 * @brief Atomic min for floats using CAS loop
 * Handles negative floats correctly unlike naive __float_as_int approach
 */
__device__ void atomicMinFloat(float* addr, float value) {
    int* addr_as_int = (int*)addr;
    int old = *addr_as_int;
    int assumed;
    do {
        assumed = old;
        float old_val = __int_as_float(assumed);
        if (old_val <= value) return;  // Already smaller
        old = atomicCAS(addr_as_int, assumed, __float_as_int(value));
    } while (assumed != old);
}

/**
 * @brief Atomic max for floats using CAS loop
 */
__device__ void atomicMaxFloat(float* addr, float value) {
    int* addr_as_int = (int*)addr;
    int old = *addr_as_int;
    int assumed;
    do {
        assumed = old;
        float old_val = __int_as_float(assumed);
        if (old_val >= value) return;  // Already larger
        old = atomicCAS(addr_as_int, assumed, __float_as_int(value));
    } while (assumed != old);
}

__global__ void kernel_compute_minmax(
    const float* input,
    float* min_val,
    float* max_val,
    size_t n)
{
    extern __shared__ float shared[];
    float* s_min = shared;
    float* s_max = shared + blockDim.x;

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Initialize with extreme values
    float local_min = INFINITY;
    float local_max = -INFINITY;

    // Stride through input
    for (size_t i = idx; i < n; i += gridDim.x * blockDim.x) {
        float val = input[i];
        local_min = fminf(local_min, val);
        local_max = fmaxf(local_max, val);
    }

    s_min[tid] = local_min;
    s_max[tid] = local_max;
    __syncthreads();

    // Parallel reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            s_min[tid] = fminf(s_min[tid], s_min[tid + stride]);
            s_max[tid] = fmaxf(s_max[tid], s_max[tid + stride]);
        }
        __syncthreads();
    }

    // Write to global memory atomically (correct for floats)
    if (tid == 0) {
        atomicMinFloat(min_val, s_min[0]);
        atomicMaxFloat(max_val, s_max[0]);
    }
}

//-----------------------------------------------------------------------------
// CUDA API Implementation
//-----------------------------------------------------------------------------

bool nimcp_gpu_infer_linear_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !weights || !output) {
        LOG_ERROR("NULL parameter");
        return false;
    }

    // Dimensions: input[batch, in_features], weights[out_features, in_features]
    size_t batch = input->dims[0];
    size_t in_features = input->dims[1];
    size_t out_features = weights->dims[0];

    // Step 1: Matrix multiply using cuBLAS (C = alpha * A * B + beta * C)
    // We need: output = input @ weights^T
    float alpha = 1.0f, beta_val = 0.0f;
    cublasStatus_t status = cublasSgemm(
        ctx->cublas_handle,
        CUBLAS_OP_T, CUBLAS_OP_N,  // Transpose weights
        out_features, batch, in_features,
        &alpha,
        (float*)weights->data, in_features,
        (float*)input->data, in_features,
        &beta_val,
        (float*)output->data, out_features
    );

    if (status != CUBLAS_STATUS_SUCCESS) {
        LOG_ERROR("cuBLAS SGEMM failed: %d", status);
        return false;
    }

    // Step 2: Fused bias + ReLU
    size_t total = batch * out_features;
    int threads = 256;
    int blocks = (total + threads - 1) / threads;

    kernel_linear_relu_epilogue<<<blocks, threads, 0, ctx->compute_stream>>>(
        (float*)output->data,
        bias ? (float*)bias->data : NULL,
        out_features,
        batch
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_linear_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !weights || !output) return false;

    size_t batch = input->dims[0];
    size_t in_features = input->dims[1];
    size_t out_features = weights->dims[0];

    // Matrix multiply
    float alpha = 1.0f, beta_val = 0.0f;
    cublasStatus_t status = cublasSgemm(
        ctx->cublas_handle,
        CUBLAS_OP_T, CUBLAS_OP_N,
        out_features, batch, in_features,
        &alpha,
        (float*)weights->data, in_features,
        (float*)input->data, in_features,
        &beta_val,
        (float*)output->data, out_features
    );

    if (status != CUBLAS_STATUS_SUCCESS) return false;

    // Fused bias + GELU
    size_t total = batch * out_features;
    int threads = 256;
    int blocks = (total + threads - 1) / threads;

    kernel_linear_gelu_epilogue<<<blocks, threads, 0, ctx->compute_stream>>>(
        (float*)output->data,
        bias ? (float*)bias->data : NULL,
        out_features,
        batch
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_linear_silu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !weights || !output) return false;

    size_t batch = input->dims[0];
    size_t in_features = input->dims[1];
    size_t out_features = weights->dims[0];

    // Matrix multiply
    float alpha = 1.0f, beta_val = 0.0f;
    cublasStatus_t status = cublasSgemm(
        ctx->cublas_handle,
        CUBLAS_OP_T, CUBLAS_OP_N,
        out_features, batch, in_features,
        &alpha,
        (float*)weights->data, in_features,
        (float*)input->data, in_features,
        &beta_val,
        (float*)output->data, out_features
    );

    if (status != CUBLAS_STATUS_SUCCESS) return false;

    // Fused bias + SiLU
    size_t total = batch * out_features;
    int threads = 256;
    int blocks = (total + threads - 1) / threads;

    kernel_linear_silu_epilogue<<<blocks, threads, 0, ctx->compute_stream>>>(
        (float*)output->data,
        bias ? (float*)bias->data : NULL,
        out_features,
        batch
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_conv_bn_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bn_gamma,
    const nimcp_gpu_tensor_t* bn_beta,
    const nimcp_gpu_tensor_t* bn_mean,
    const nimcp_gpu_tensor_t* bn_var,
    nimcp_gpu_tensor_t* output,
    uint32_t stride,
    uint32_t padding,
    float eps)
{
    // Full implementation would use cuDNN for optimal convolution
    LOG_DEBUG("CUDA conv_bn_relu - requires cuDNN integration");
    return false;
}

bool nimcp_gpu_infer_attention_fused(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* W_qkv,
    const nimcp_gpu_tensor_t* W_o,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* mask,
    uint32_t n_heads,
    float scale)
{
    // Full Flash Attention implementation would be complex
    LOG_DEBUG("CUDA fused attention - requires Flash Attention kernel");
    return false;
}

bool nimcp_gpu_infer_quantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_quant_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output || !params) return false;

    size_t n = input->numel;
    int threads = 256;
    int blocks = (n + threads - 1) / threads;

    kernel_quantize_int8<<<blocks, threads, 0, ctx->compute_stream>>>(
        (float*)input->data,
        (int8_t*)output->data,
        params->scale,
        params->zero_point,
        n
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_dequantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_quant_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output || !params) return false;

    size_t n = input->numel;
    int threads = 256;
    int blocks = (n + threads - 1) / threads;

    kernel_dequantize_int8<<<blocks, threads, 0, ctx->compute_stream>>>(
        (int8_t*)input->data,
        (float*)output->data,
        params->scale,
        params->zero_point,
        n
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_gemm_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* c,
    const nimcp_quant_params_t* params_a,
    const nimcp_quant_params_t* params_b,
    const nimcp_quant_params_t* params_c)
{
    // Would use cublasLtMatmul for INT8 GEMM
    LOG_DEBUG("INT8 GEMM - requires cuBLASLt");
    return false;
}

bool nimcp_gpu_infer_gemm_fp16(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* c,
    bool accumulate_fp32)
{
    // Would use cublasHgemm or cublasGemmEx
    LOG_DEBUG("FP16 GEMM - requires half precision support");
    return false;
}

bool nimcp_gpu_infer_calibrate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* tensor,
    nimcp_quant_params_t* params,
    bool symmetric)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !tensor || !params) return false;

    /* W9: Check all CUDA allocations and memcpy for quantization */
    float* d_min;
    float* d_max;
    if (cudaMalloc(&d_min, sizeof(float)) != cudaSuccess) return false;
    if (cudaMalloc(&d_max, sizeof(float)) != cudaSuccess) {
        cudaFree(d_min); return false;
    }

    float h_min = INFINITY;
    float h_max = -INFINITY;
    if (cudaMemcpy(d_min, &h_min, sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess ||
        cudaMemcpy(d_max, &h_max, sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_min); cudaFree(d_max); return false;
    }

    // Find min/max
    size_t n = tensor->numel;
    int threads = 256;
    int blocks = min((int)((n + threads - 1) / threads), 1024);
    size_t smem = 2 * threads * sizeof(float);

    kernel_compute_minmax<<<blocks, threads, smem, ctx->compute_stream>>>(
        (float*)tensor->data, d_min, d_max, n
    );

    // Synchronize stream before copying results
    cudaStreamSynchronize(ctx->compute_stream);

    /* W9: Check readback + guard against scale=0 */
    if (cudaMemcpy(&h_min, d_min, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess ||
        cudaMemcpy(&h_max, d_max, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        cudaFree(d_min); cudaFree(d_max); return false;
    }

    cudaFree(d_min);
    cudaFree(d_max);

    // Compute quantization parameters
    if (symmetric) {
        float absmax = fmaxf(fabsf(h_min), fabsf(h_max));
        params->scale = absmax / 127.0f;
        params->zero_point = 0;
        params->min_val = -absmax;
        params->max_val = absmax;
    } else {
        float range = h_max - h_min;
        params->scale = (range > 1e-7f) ? range / 255.0f : 1e-7f;
        params->zero_point = (int32_t)roundf(-h_min / params->scale) - 128;
        params->min_val = h_min;
        params->max_val = h_max;
    }

    return true;
}

bool nimcp_gpu_infer_batch_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t** inputs,
    nimcp_gpu_tensor_t** outputs,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    size_t n_batches)
{
    // Process each batch
    for (size_t i = 0; i < n_batches; i++) {
        if (!nimcp_gpu_infer_linear_relu(ctx, inputs[i], weights, bias, outputs[i])) {
            return false;
        }
    }
    return true;
}

bool nimcp_gpu_infer_dynamic_batch(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const uint32_t* lengths,
    nimcp_gpu_tensor_t* outputs,
    const nimcp_gpu_tensor_t* weights,
    size_t n_samples)
{
    LOG_DEBUG("Dynamic batching - requires advanced kernel");
    return false;
}

bool nimcp_gpu_infer_activation_inplace(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    int activation)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !tensor) return false;

    size_t n = tensor->numel;
    int threads = 256;
    int blocks = (n + threads - 1) / threads;

    kernel_activation_inplace<<<blocks, threads, 0, ctx->compute_stream>>>(
        (float*)tensor->data,
        n,
        activation
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_residual_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* residual,
    nimcp_gpu_tensor_t* y,
    float alpha,
    float beta)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !x || !residual || !y) return false;

    size_t n = x->numel;
    int threads = 256;
    int blocks = (n + threads - 1) / threads;

    kernel_residual_add<<<blocks, threads, 0, ctx->compute_stream>>>(
        (float*)x->data,
        (float*)residual->data,
        (float*)y->data,
        alpha,
        beta,
        n
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !gamma || !beta || !output) return false;

    size_t batch_size = input->dims[0];
    size_t hidden_size = input->dims[1];

    int threads = min((int)hidden_size, 256);
    int blocks = batch_size;
    size_t smem = 2 * threads * sizeof(float);

    kernel_layernorm<<<blocks, threads, smem, ctx->compute_stream>>>(
        (float*)input->data,
        (float*)gamma->data,
        (float*)beta->data,
        (float*)output->data,
        batch_size,
        hidden_size,
        eps
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_infer_rmsnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !gamma || !output) return false;

    size_t batch_size = input->dims[0];
    size_t hidden_size = input->dims[1];

    int threads = min((int)hidden_size, 256);
    int blocks = batch_size;
    size_t smem = threads * sizeof(float);

    kernel_rmsnorm<<<blocks, threads, smem, ctx->compute_stream>>>(
        (float*)input->data,
        (float*)gamma->data,
        (float*)output->data,
        batch_size,
        hidden_size,
        eps
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

// Session management

nimcp_infer_session_t* nimcp_infer_session_create(
    nimcp_gpu_context_t* ctx,
    nimcp_infer_precision_t precision,
    size_t workspace_size)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return NULL;

    nimcp_infer_session_t* session = (nimcp_infer_session_t*)nimcp_calloc(1, sizeof(nimcp_infer_session_t));
    if (!session) return NULL;

    session->ctx = ctx;
    session->precision = precision;
    session->graph_captured = false;

    // Allocate workspace
    if (workspace_size == 0) {
        workspace_size = 64 * 1024 * 1024;  // 64 MB default
    }
    session->workspace_size = workspace_size;
    /* W9: Check workspace allocation */
    if (cudaMalloc(&session->workspace, workspace_size) != cudaSuccess) {
        nimcp_free(session);
        return NULL;
    }

    return session;
}

void nimcp_infer_session_destroy(nimcp_infer_session_t* session)
{
    if (!session) return;

    if (session->workspace) {
        cudaFree(session->workspace);
    }
    if (session->cuda_graph) {
        // cuda_graph is actually a cudaGraphExec_t, not cudaGraph_t
        cudaGraphExecDestroy((cudaGraphExec_t)session->cuda_graph);
    }
    nimcp_free(session);
}

bool nimcp_infer_session_begin_capture(nimcp_infer_session_t* session)
{
    if (!session) return false;

    cudaError_t err = cudaStreamBeginCapture(
        session->ctx->compute_stream,
        cudaStreamCaptureModeGlobal
    );

    return err == cudaSuccess;
}

bool nimcp_infer_session_end_capture(nimcp_infer_session_t* session)
{
    if (!session) return false;

    cudaGraph_t graph;
    cudaError_t err = cudaStreamEndCapture(session->ctx->compute_stream, &graph);
    if (err != cudaSuccess) return false;

    cudaGraphExec_t graphExec;
    err = cudaGraphInstantiate(&graphExec, graph, NULL, NULL, 0);
    if (err != cudaSuccess) {
        cudaGraphDestroy(graph);
        return false;
    }

    session->cuda_graph = (void*)graphExec;
    session->graph_captured = true;

    cudaGraphDestroy(graph);  // Original graph no longer needed
    return true;
}

bool nimcp_infer_session_replay(nimcp_infer_session_t* session)
{
    if (!session || !session->graph_captured) return false;

    cudaError_t err = cudaGraphLaunch(
        (cudaGraphExec_t)session->cuda_graph,
        session->ctx->compute_stream
    );

    return err == cudaSuccess;
}

bool nimcp_gpu_infer_convert_precision(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_infer_precision_t target_precision)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output) {
        LOG_ERROR("NULL parameter in precision conversion");
        return false;
    }

    if (input->numel != output->numel) {
        LOG_ERROR("Input/output size mismatch: %zu vs %zu", input->numel, output->numel);
        return false;
    }

    size_t n = input->numel;
    int threads = 256;
    int blocks = (n + threads - 1) / threads;
    cudaStream_t stream = ctx->compute_stream;

    // Determine conversion path
    nimcp_gpu_precision_t src_prec = input->precision;
    nimcp_gpu_precision_t dst_prec = output->precision;

    // Map NIMCP_INFER precision to NIMCP_GPU precision for validation
    nimcp_gpu_precision_t target_gpu_prec;
    switch (target_precision) {
        case NIMCP_INFER_FP32:
            target_gpu_prec = NIMCP_GPU_PRECISION_FP32;
            break;
        case NIMCP_INFER_FP16:
            target_gpu_prec = NIMCP_GPU_PRECISION_FP16;
            break;
        case NIMCP_INFER_BF16:
            target_gpu_prec = NIMCP_GPU_PRECISION_BF16;
            break;
        case NIMCP_INFER_INT8:
            target_gpu_prec = NIMCP_GPU_PRECISION_INT8;
            break;
        default:
            LOG_ERROR("Unsupported target precision: %d", target_precision);
            return false;
    }

    // Verify output tensor has correct precision
    if (dst_prec != target_gpu_prec) {
        LOG_ERROR("Output tensor precision mismatch: expected %d, got %d",
                  target_gpu_prec, dst_prec);
        return false;
    }

    // Perform conversion
    if (src_prec == NIMCP_GPU_PRECISION_FP32 && dst_prec == NIMCP_GPU_PRECISION_FP16) {
        // FP32 -> FP16
        kernel_fp32_to_fp16<<<blocks, threads, 0, stream>>>(
            (const float*)input->data,
            (half*)output->data,
            n);
    } else if (src_prec == NIMCP_GPU_PRECISION_FP16 && dst_prec == NIMCP_GPU_PRECISION_FP32) {
        // FP16 -> FP32
        kernel_fp16_to_fp32<<<blocks, threads, 0, stream>>>(
            (const half*)input->data,
            (float*)output->data,
            n);
    } else if (src_prec == dst_prec) {
        // Same precision - just copy
        size_t elem_size = (src_prec == NIMCP_GPU_PRECISION_FP32) ? sizeof(float) :
                           (src_prec == NIMCP_GPU_PRECISION_FP16) ? sizeof(half) :
                           sizeof(int8_t);
        cudaMemcpyAsync(output->data, input->data, n * elem_size,
                        cudaMemcpyDeviceToDevice, stream);
    } else {
        LOG_ERROR("Unsupported conversion: %d -> %d", src_prec, dst_prec);
        return false;
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    LOG_DEBUG("Converted %zu elements from precision %d to %d", n, src_prec, dst_prec);
    return true;
}

nimcp_infer_precision_t nimcp_gpu_infer_recommended_precision(nimcp_gpu_context_t* ctx)
{
    if (!ctx) return NIMCP_INFER_FP32;

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, ctx->device_id);

    // Ampere+ supports TF32 natively
    if (prop.major >= 8) {
        return NIMCP_INFER_TF32;
    }
    // Volta+ has good FP16 tensor core support
    if (prop.major >= 7) {
        return NIMCP_INFER_FP16;
    }

    return NIMCP_INFER_FP32;
}

bool nimcp_gpu_infer_warmup(nimcp_gpu_context_t* ctx, nimcp_infer_precision_t precision)
{
    if (!ctx) return false;

    // Launch a dummy kernel to warm up the GPU
    size_t n = 1024;
    float* d_data;
    /* W9: Check warmup allocation */
    if (cudaMalloc(&d_data, n * sizeof(float)) != cudaSuccess) return false;
    cudaMemset(d_data, 0, n * sizeof(float));

    int threads = 256;
    int blocks = (n + threads - 1) / threads;
    kernel_activation_inplace<<<blocks, threads>>>(d_data, n, 0);

    cudaDeviceSynchronize();
    cudaFree(d_data);

    LOG_INFO("GPU inference warmup complete");
    return true;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Implementation
//=============================================================================

bool nimcp_gpu_infer_linear_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !weights || !output) return false;

    size_t batch = input->dims[0];
    size_t in_features = input->dims[1];
    size_t out_features = weights->dims[0];

    float* in = (float*)input->data;
    float* w = (float*)weights->data;
    float* b = bias ? (float*)bias->data : NULL;
    float* out = (float*)output->data;

    // Compute output = input @ weights^T + bias, then ReLU
    for (size_t i = 0; i < batch; i++) {
        for (size_t j = 0; j < out_features; j++) {
            float sum = b ? b[j] : 0.0f;
            for (size_t k = 0; k < in_features; k++) {
                sum += in[i * in_features + k] * w[j * in_features + k];
            }
            // ReLU
            out[i * out_features + j] = sum > 0.0f ? sum : 0.0f;
        }
    }

    return true;
}

bool nimcp_gpu_infer_linear_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !weights || !output) return false;

    size_t batch = input->dims[0];
    size_t in_features = input->dims[1];
    size_t out_features = weights->dims[0];

    float* in = (float*)input->data;
    float* w = (float*)weights->data;
    float* b = bias ? (float*)bias->data : NULL;
    float* out = (float*)output->data;

    const float sqrt_2_over_pi = 0.7978845608f;

    for (size_t i = 0; i < batch; i++) {
        for (size_t j = 0; j < out_features; j++) {
            float sum = b ? b[j] : 0.0f;
            for (size_t k = 0; k < in_features; k++) {
                sum += in[i * in_features + k] * w[j * in_features + k];
            }
            // GELU
            float x = sum;
            float x_cubed = x * x * x;
            float inner = sqrt_2_over_pi * (x + 0.044715f * x_cubed);
            out[i * out_features + j] = 0.5f * x * (1.0f + tanhf(inner));
        }
    }

    return true;
}

bool nimcp_gpu_infer_linear_silu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;
    if (!input || !weights || !output) return false;

    size_t batch = input->dims[0];
    size_t in_features = input->dims[1];
    size_t out_features = weights->dims[0];

    float* in = (float*)input->data;
    float* w = (float*)weights->data;
    float* b = bias ? (float*)bias->data : NULL;
    float* out = (float*)output->data;

    for (size_t i = 0; i < batch; i++) {
        for (size_t j = 0; j < out_features; j++) {
            float sum = b ? b[j] : 0.0f;
            for (size_t k = 0; k < in_features; k++) {
                sum += in[i * in_features + k] * w[j * in_features + k];
            }
            // SiLU
            out[i * out_features + j] = sum / (1.0f + expf(-sum));
        }
    }

    return true;
}

bool nimcp_gpu_infer_conv_bn_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bn_gamma,
    const nimcp_gpu_tensor_t* bn_beta,
    const nimcp_gpu_tensor_t* bn_mean,
    const nimcp_gpu_tensor_t* bn_var,
    nimcp_gpu_tensor_t* output,
    uint32_t stride,
    uint32_t padding,
    float eps)
{
    (void)ctx;
    (void)input;
    (void)weights;
    (void)bn_gamma;
    (void)bn_beta;
    (void)bn_mean;
    (void)bn_var;
    (void)output;
    (void)stride;
    (void)padding;
    (void)eps;

    LOG_DEBUG("CPU conv_bn_relu - full implementation needed");
    return false;
}

bool nimcp_gpu_infer_attention_fused(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* W_qkv,
    const nimcp_gpu_tensor_t* W_o,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* mask,
    uint32_t n_heads,
    float scale)
{
    (void)ctx;
    (void)input;
    (void)W_qkv;
    (void)W_o;
    (void)output;
    (void)mask;
    (void)n_heads;
    (void)scale;

    LOG_DEBUG("CPU fused attention - full implementation needed");
    return false;
}

bool nimcp_gpu_infer_quantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_quant_params_t* params)
{
    (void)ctx;
    if (!input || !output || !params) return false;

    float* in = (float*)input->data;
    int8_t* out = (int8_t*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        int32_t q = (int32_t)roundf(in[i] / params->scale) + params->zero_point;
        q = q < -128 ? -128 : (q > 127 ? 127 : q);
        out[i] = (int8_t)q;
    }

    return true;
}

bool nimcp_gpu_infer_dequantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_quant_params_t* params)
{
    (void)ctx;
    if (!input || !output || !params) return false;

    int8_t* in = (int8_t*)input->data;
    float* out = (float*)output->data;

    for (size_t i = 0; i < input->numel; i++) {
        out[i] = params->scale * ((float)in[i] - params->zero_point);
    }

    return true;
}

bool nimcp_gpu_infer_gemm_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* c,
    const nimcp_quant_params_t* params_a,
    const nimcp_quant_params_t* params_b,
    const nimcp_quant_params_t* params_c)
{
    (void)ctx;
    (void)a;
    (void)b;
    (void)c;
    (void)params_a;
    (void)params_b;
    (void)params_c;

    LOG_DEBUG("CPU INT8 GEMM - full implementation needed");
    return false;
}

bool nimcp_gpu_infer_gemm_fp16(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* c,
    bool accumulate_fp32)
{
    (void)ctx;
    (void)a;
    (void)b;
    (void)c;
    (void)accumulate_fp32;

    LOG_DEBUG("CPU FP16 GEMM - full implementation needed");
    return false;
}

bool nimcp_gpu_infer_calibrate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* tensor,
    nimcp_quant_params_t* params,
    bool symmetric)
{
    (void)ctx;
    if (!tensor || !params) return false;

    float* data = (float*)tensor->data;
    float min_val = data[0];
    float max_val = data[0];

    for (size_t i = 1; i < tensor->numel; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }

    if (symmetric) {
        float absmax = fmaxf(fabsf(min_val), fabsf(max_val));
        params->scale = absmax / 127.0f;
        params->zero_point = 0;
        params->min_val = -absmax;
        params->max_val = absmax;
    } else {
        params->scale = (max_val - min_val) / 255.0f;
        params->zero_point = (int32_t)roundf(-min_val / params->scale) - 128;
        params->min_val = min_val;
        params->max_val = max_val;
    }

    return true;
}

bool nimcp_gpu_infer_batch_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t** inputs,
    nimcp_gpu_tensor_t** outputs,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    size_t n_batches)
{
    for (size_t i = 0; i < n_batches; i++) {
        if (!nimcp_gpu_infer_linear_relu(ctx, inputs[i], weights, bias, outputs[i])) {
            return false;
        }
    }
    return true;
}

bool nimcp_gpu_infer_dynamic_batch(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const uint32_t* lengths,
    nimcp_gpu_tensor_t* outputs,
    const nimcp_gpu_tensor_t* weights,
    size_t n_samples)
{
    (void)ctx;
    (void)inputs;
    (void)lengths;
    (void)outputs;
    (void)weights;
    (void)n_samples;

    LOG_DEBUG("CPU dynamic batch - full implementation needed");
    return false;
}

bool nimcp_gpu_infer_activation_inplace(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    int activation)
{
    (void)ctx;
    if (!tensor) return false;

    float* data = (float*)tensor->data;
    const float sqrt_2_over_pi = 0.7978845608f;

    for (size_t i = 0; i < tensor->numel; i++) {
        float x = data[i];

        switch (activation) {
            case 0:  // ReLU
                data[i] = (x > 0.0f) ? x : 0.0f;
                break;
            case 1:  // Sigmoid
                data[i] = 1.0f / (1.0f + expf(-x));
                break;
            case 2:  // Tanh
                data[i] = tanhf(x);
                break;
            case 3:  // GELU
                {
                    float x_cubed = x * x * x;
                    float inner = sqrt_2_over_pi * (x + 0.044715f * x_cubed);
                    data[i] = 0.5f * x * (1.0f + tanhf(inner));
                }
                break;
            case 4:  // SiLU
                data[i] = x / (1.0f + expf(-x));
                break;
        }
    }

    return true;
}

bool nimcp_gpu_infer_residual_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* residual,
    nimcp_gpu_tensor_t* y,
    float alpha,
    float beta)
{
    (void)ctx;
    if (!x || !residual || !y) return false;

    float* x_data = (float*)x->data;
    float* r_data = (float*)residual->data;
    float* y_data = (float*)y->data;

    for (size_t i = 0; i < x->numel; i++) {
        y_data[i] = alpha * x_data[i] + beta * r_data[i];
    }

    return true;
}

bool nimcp_gpu_infer_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    (void)ctx;
    if (!input || !gamma || !beta || !output) return false;

    size_t batch_size = input->dims[0];
    size_t hidden_size = input->dims[1];

    float* in = (float*)input->data;
    float* g = (float*)gamma->data;
    float* b = (float*)beta->data;
    float* out = (float*)output->data;

    for (size_t i = 0; i < batch_size; i++) {
        float* sample = in + i * hidden_size;
        float* out_sample = out + i * hidden_size;

        // Compute mean
        float mean = 0.0f;
        for (size_t j = 0; j < hidden_size; j++) {
            mean += sample[j];
        }
        mean /= hidden_size;

        // Compute variance
        float var = 0.0f;
        for (size_t j = 0; j < hidden_size; j++) {
            float diff = sample[j] - mean;
            var += diff * diff;
        }
        var /= hidden_size;

        // Normalize
        float inv_std = 1.0f / sqrtf(var + eps);
        for (size_t j = 0; j < hidden_size; j++) {
            out_sample[j] = g[j] * (sample[j] - mean) * inv_std + b[j];
        }
    }

    return true;
}

bool nimcp_gpu_infer_rmsnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    (void)ctx;
    if (!input || !gamma || !output) return false;

    size_t batch_size = input->dims[0];
    size_t hidden_size = input->dims[1];

    float* in = (float*)input->data;
    float* g = (float*)gamma->data;
    float* out = (float*)output->data;

    for (size_t i = 0; i < batch_size; i++) {
        float* sample = in + i * hidden_size;
        float* out_sample = out + i * hidden_size;

        // Compute sum of squares
        float sum_sq = 0.0f;
        for (size_t j = 0; j < hidden_size; j++) {
            sum_sq += sample[j] * sample[j];
        }

        // Compute RMS and normalize
        float rms = sqrtf(sum_sq / hidden_size + eps);
        float inv_rms = 1.0f / rms;

        for (size_t j = 0; j < hidden_size; j++) {
            out_sample[j] = g[j] * sample[j] * inv_rms;
        }
    }

    return true;
}

nimcp_infer_session_t* nimcp_infer_session_create(
    nimcp_gpu_context_t* ctx,
    nimcp_infer_precision_t precision,
    size_t workspace_size)
{
    nimcp_infer_session_t* session = (nimcp_infer_session_t*)nimcp_calloc(1, sizeof(nimcp_infer_session_t));
    if (!session) return NULL;

    session->ctx = ctx;
    session->precision = precision;
    session->graph_captured = false;
    session->workspace_size = workspace_size ? workspace_size : 64 * 1024 * 1024;
    session->workspace = nimcp_malloc(session->workspace_size);

    return session;
}

void nimcp_infer_session_destroy(nimcp_infer_session_t* session)
{
    if (!session) return;

    if (session->workspace) {
        nimcp_free(session->workspace);
    }
    nimcp_free(session);
}

bool nimcp_infer_session_begin_capture(nimcp_infer_session_t* session)
{
    (void)session;
    LOG_DEBUG("CPU mode - graph capture not supported");
    return false;
}

bool nimcp_infer_session_end_capture(nimcp_infer_session_t* session)
{
    (void)session;
    LOG_DEBUG("CPU mode - graph capture not supported");
    return false;
}

bool nimcp_infer_session_replay(nimcp_infer_session_t* session)
{
    (void)session;
    LOG_DEBUG("CPU mode - graph replay not supported");
    return false;
}

bool nimcp_gpu_infer_convert_precision(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_infer_precision_t target_precision)
{
    (void)ctx;
    (void)input;
    (void)output;
    (void)target_precision;

    LOG_DEBUG("CPU precision conversion not implemented");
    return false;
}

nimcp_infer_precision_t nimcp_gpu_infer_recommended_precision(nimcp_gpu_context_t* ctx)
{
    (void)ctx;
    return NIMCP_INFER_FP32;  // CPU always uses FP32
}

bool nimcp_gpu_infer_warmup(nimcp_gpu_context_t* ctx, nimcp_infer_precision_t precision)
{
    (void)ctx;
    (void)precision;
    LOG_INFO("CPU inference warmup complete (no-op)");
    return true;
}

#endif // NIMCP_ENABLE_CUDA
