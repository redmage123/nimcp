/**
 * @file nimcp_conv_kernels.cu
 * @brief GPU CNN CUDA Kernels (Convolution, Pooling, Normalization)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <math.h>
#include <float.h>
#include <vector>
#include <cstdlib>

// Now include our headers (which have extern "C" blocks)
#include "gpu/cnn/nimcp_cnn_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#define LOG_MODULE "CNN_GPU"

#define BLOCK_SIZE 16
#define WARP_SIZE 32

//=============================================================================
// Conv2D Forward Kernel
//=============================================================================

__global__ void kernel_conv2d_forward(
    const float* input, const float* weight, const float* bias, float* output,
    int N, int C_in, int H_in, int W_in,
    int C_out, int H_out, int W_out,
    int kH, int kW, int stride_h, int stride_w,
    int pad_h, int pad_w, int dilation_h, int dilation_w, int groups)
{
    int n = blockIdx.z;
    int c_out = blockIdx.y * blockDim.y + threadIdx.y;
    int h_out = blockIdx.x * blockDim.x + threadIdx.x / W_out;
    int w_out = blockIdx.x * blockDim.x + threadIdx.x % W_out;

    if (c_out >= C_out || h_out >= H_out || w_out >= W_out) return;

    int group = c_out / (C_out / groups);
    int C_per_group = C_in / groups;

    float sum = 0.0f;

    for (int c_in = 0; c_in < C_per_group; c_in++) {
        int c = group * C_per_group + c_in;
        for (int kh = 0; kh < kH; kh++) {
            for (int kw = 0; kw < kW; kw++) {
                int h_in = h_out * stride_h - pad_h + kh * dilation_h;
                int w_in = w_out * stride_w - pad_w + kw * dilation_w;

                if (h_in >= 0 && h_in < H_in && w_in >= 0 && w_in < W_in) {
                    int in_idx = n * C_in * H_in * W_in + c * H_in * W_in + h_in * W_in + w_in;
                    int w_idx = c_out * C_per_group * kH * kW + c_in * kH * kW + kh * kW + kw;
                    sum += input[in_idx] * weight[w_idx];
                }
            }
        }
    }

    if (bias) {
        sum += bias[c_out];
    }

    int out_idx = n * C_out * H_out * W_out + c_out * H_out * W_out + h_out * W_out + w_out;
    output[out_idx] = sum;
}

bool nimcp_gpu_conv2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    const nimcp_conv_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !weight || !output || !params) return false;

    int N = input->dims[0];
    int C_in = input->dims[1];
    int H_in = input->dims[2];
    int W_in = input->dims[3];

    int C_out = weight->dims[0];
    int kH = params->kernel_h;
    int kW = params->kernel_w;

    int H_out = (H_in + 2 * params->pad_h - params->dilation_h * (kH - 1) - 1) / params->stride_h + 1;
    int W_out = (W_in + 2 * params->pad_w - params->dilation_w * (kW - 1) - 1) / params->stride_w + 1;

    dim3 block(BLOCK_SIZE, BLOCK_SIZE);
    dim3 grid((H_out * W_out + BLOCK_SIZE - 1) / BLOCK_SIZE,
              (C_out + BLOCK_SIZE - 1) / BLOCK_SIZE,
              N);

    kernel_conv2d_forward<<<grid, block>>>(
        (const float*)input->data, (const float*)weight->data,
        bias ? (const float*)bias->data : NULL, (float*)output->data,
        N, C_in, H_in, W_in, C_out, H_out, W_out,
        kH, kW, params->stride_h, params->stride_w,
        params->pad_h, params->pad_w, params->dilation_h, params->dilation_w,
        params->groups);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// im2col/col2im Kernels for Conv2D Backward
//=============================================================================

/**
 * @brief im2col kernel: Transform input patches to column format
 *
 * WHAT: Rearranges input image patches into columns for matrix multiplication
 * WHY:  Enables efficient convolution via GEMM
 * HOW:  Each thread handles one output element in the column buffer
 */
__global__ void kernel_im2col(
    const float* input, float* col,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int total = C * kH * kW * outH * outW;

    if (index >= total) return;

    // Decompose index into (c, kh, kw, oh, ow)
    int ow = index % outW;
    int oh = (index / outW) % outH;
    int kw = (index / (outW * outH)) % kW;
    int kh = (index / (outW * outH * kW)) % kH;
    int c = index / (outW * outH * kW * kH);

    int h_in = oh * sH - pH + kh;
    int w_in = ow * sW - pW + kw;

    float val = 0.0f;
    if (h_in >= 0 && h_in < H && w_in >= 0 && w_in < W) {
        val = input[c * H * W + h_in * W + w_in];
    }

    // col layout: (C*kH*kW, outH*outW)
    int col_idx = (c * kH * kW + kh * kW + kw) * outH * outW + oh * outW + ow;
    col[col_idx] = val;
}

/**
 * @brief col2im kernel: Transform column format back to image gradients
 *
 * WHAT: Accumulates gradients from column format back to spatial layout
 * WHY:  Needed for backward pass gradient computation
 * HOW:  Each thread handles one input pixel, accumulating from all patches
 */
__global__ void kernel_col2im(
    const float* col, float* input_grad,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int total = C * H * W;

    if (index >= total) return;

    int w = index % W;
    int h = (index / W) % H;
    int c = index / (H * W);

    float sum = 0.0f;

    // For each kernel position that could contribute to this input pixel
    for (int kh = 0; kh < kH; kh++) {
        for (int kw = 0; kw < kW; kw++) {
            // Find output positions that use this input pixel
            int h_padded = h + pH - kh;
            int w_padded = w + pW - kw;

            // Check if this is a valid output position
            if (h_padded >= 0 && h_padded % sH == 0 && w_padded >= 0 && w_padded % sW == 0) {
                int oh = h_padded / sH;
                int ow = w_padded / sW;

                if (oh < outH && ow < outW) {
                    int col_idx = (c * kH * kW + kh * kW + kw) * outH * outW + oh * outW + ow;
                    sum += col[col_idx];
                }
            }
        }
    }

    input_grad[index] = sum;
}

/**
 * @brief Compute weight gradients via GEMM
 *
 * weight_grad[cout, cin*kH*kW] = output_grad[cout, outH*outW] @ col[cin*kH*kW, outH*outW]^T
 */
__global__ void kernel_conv2d_weight_grad(
    const float* col, const float* output_grad,
    float* weight_grad,
    int cout, int cin_k, int spatial_out)
{
    // Each thread computes one element of weight_grad
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = cout * cin_k;

    if (idx >= total) return;

    int k = idx % cin_k;
    int co = idx / cin_k;

    float sum = 0.0f;
    for (int s = 0; s < spatial_out; s++) {
        // output_grad: (cout, outH*outW)
        // col: (cin*kH*kW, outH*outW)
        sum += output_grad[co * spatial_out + s] * col[k * spatial_out + s];
    }

    weight_grad[idx] = sum;
}

/**
 * @brief Compute bias gradients: sum over spatial and batch dimensions
 */
__global__ void kernel_conv2d_bias_grad(
    const float* output_grad, float* bias_grad,
    int N, int C, int spatial_size)
{
    int c = blockIdx.x;
    if (c >= C) return;

    __shared__ float shared_sum[256];
    float thread_sum = 0.0f;

    for (int n = 0; n < N; n++) {
        for (int s = threadIdx.x; s < spatial_size; s += blockDim.x) {
            thread_sum += output_grad[n * C * spatial_size + c * spatial_size + s];
        }
    }

    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    // Reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        bias_grad[c] = shared_sum[0];
    }
}

//=============================================================================
// im2col/col2im Public API
//=============================================================================

bool nimcp_gpu_im2col(
    nimcp_gpu_context_t* ctx,
    const float* input, float* col,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !col) return false;

    int total = C * kH * kW * outH * outW;
    int block_size = 256;
    int grid_size = (total + block_size - 1) / block_size;

    kernel_im2col<<<grid_size, block_size>>>(
        input, col, C, H, W, kH, kW, sH, sW, pH, pW, outH, outW);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_col2im(
    nimcp_gpu_context_t* ctx,
    const float* col, float* input_grad,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !col || !input_grad) return false;

    int total = C * H * W;
    int block_size = 256;
    int grid_size = (total + block_size - 1) / block_size;

    kernel_col2im<<<grid_size, block_size>>>(
        col, input_grad, C, H, W, kH, kW, sH, sW, pH, pW, outH, outW);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Conv2D Backward Context API
//=============================================================================

nimcp_conv2d_backward_ctx_t* nimcp_conv2d_backward_create(
    nimcp_gpu_context_t* ctx,
    int batch_size, int in_channels, int in_height, int in_width,
    int out_channels, int kernel_h, int kernel_w,
    int stride_h, int stride_w, int pad_h, int pad_w)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return NULL;

    nimcp_conv2d_backward_ctx_t* bwd_ctx =
        (nimcp_conv2d_backward_ctx_t*)nimcp_malloc(sizeof(nimcp_conv2d_backward_ctx_t));
    if (!bwd_ctx) return NULL;

    // Compute output dimensions
    int out_h = (in_height + 2 * pad_h - kernel_h) / stride_h + 1;
    int out_w = (in_width + 2 * pad_w - kernel_w) / stride_w + 1;

    // Store dimensions
    bwd_ctx->batch_size = batch_size;
    bwd_ctx->in_channels = in_channels;
    bwd_ctx->in_height = in_height;
    bwd_ctx->in_width = in_width;
    bwd_ctx->out_channels = out_channels;
    bwd_ctx->out_height = out_h;
    bwd_ctx->out_width = out_w;
    bwd_ctx->kernel_h = kernel_h;
    bwd_ctx->kernel_w = kernel_w;
    bwd_ctx->stride_h = stride_h;
    bwd_ctx->stride_w = stride_w;
    bwd_ctx->pad_h = pad_h;
    bwd_ctx->pad_w = pad_w;
    bwd_ctx->ctx = ctx;

    // Allocate GPU memory for gradients
    size_t input_size = batch_size * in_channels * in_height * in_width * sizeof(float);
    size_t weight_size = out_channels * in_channels * kernel_h * kernel_w * sizeof(float);
    size_t bias_size = out_channels * sizeof(float);
    size_t col_size = in_channels * kernel_h * kernel_w * out_h * out_w * sizeof(float);

    bwd_ctx->d_input_grad = (float*)nimcp_gpu_malloc(ctx, input_size);
    bwd_ctx->d_weight_grad = (float*)nimcp_gpu_malloc(ctx, weight_size);
    bwd_ctx->d_bias_grad = (float*)nimcp_gpu_malloc(ctx, bias_size);
    bwd_ctx->d_col_buffer = (float*)nimcp_gpu_malloc(ctx, col_size);
    bwd_ctx->d_input_cache = NULL;  // Optionally cache input

    if (!bwd_ctx->d_input_grad || !bwd_ctx->d_weight_grad ||
        !bwd_ctx->d_bias_grad || !bwd_ctx->d_col_buffer) {
        nimcp_conv2d_backward_destroy(bwd_ctx);
        return NULL;
    }

    // Zero initialize gradients
    nimcp_gpu_memset(ctx, bwd_ctx->d_input_grad, 0, input_size);
    nimcp_gpu_memset(ctx, bwd_ctx->d_weight_grad, 0, weight_size);
    nimcp_gpu_memset(ctx, bwd_ctx->d_bias_grad, 0, bias_size);

    return bwd_ctx;
}

void nimcp_conv2d_backward_destroy(nimcp_conv2d_backward_ctx_t* bwd_ctx)
{
    if (!bwd_ctx) return;

    if (bwd_ctx->ctx) {
        if (bwd_ctx->d_input_grad) nimcp_gpu_free(bwd_ctx->ctx, bwd_ctx->d_input_grad);
        if (bwd_ctx->d_weight_grad) nimcp_gpu_free(bwd_ctx->ctx, bwd_ctx->d_weight_grad);
        if (bwd_ctx->d_bias_grad) nimcp_gpu_free(bwd_ctx->ctx, bwd_ctx->d_bias_grad);
        if (bwd_ctx->d_col_buffer) nimcp_gpu_free(bwd_ctx->ctx, bwd_ctx->d_col_buffer);
        if (bwd_ctx->d_input_cache) nimcp_gpu_free(bwd_ctx->ctx, bwd_ctx->d_input_cache);
    }

    nimcp_free(bwd_ctx);
}

int nimcp_conv2d_backward(
    nimcp_conv2d_backward_ctx_t* bwd_ctx,
    const float* output_grad,
    const float* weights,
    const float* input)
{
    if (!bwd_ctx || !output_grad || !weights || !input) return -1;

    nimcp_gpu_context_t* ctx = bwd_ctx->ctx;
    int N = bwd_ctx->batch_size;
    int C_in = bwd_ctx->in_channels;
    int H = bwd_ctx->in_height;
    int W = bwd_ctx->in_width;
    int C_out = bwd_ctx->out_channels;
    int outH = bwd_ctx->out_height;
    int outW = bwd_ctx->out_width;
    int kH = bwd_ctx->kernel_h;
    int kW = bwd_ctx->kernel_w;
    int sH = bwd_ctx->stride_h;
    int sW = bwd_ctx->stride_w;
    int pH = bwd_ctx->pad_h;
    int pW = bwd_ctx->pad_w;

    int spatial_out = outH * outW;
    int col_size = C_in * kH * kW;

    // Zero gradients
    size_t input_grad_size = N * C_in * H * W * sizeof(float);
    size_t weight_grad_size = C_out * C_in * kH * kW * sizeof(float);
    size_t bias_grad_size = C_out * sizeof(float);
    nimcp_gpu_memset(ctx, bwd_ctx->d_input_grad, 0, input_grad_size);
    nimcp_gpu_memset(ctx, bwd_ctx->d_weight_grad, 0, weight_grad_size);
    nimcp_gpu_memset(ctx, bwd_ctx->d_bias_grad, 0, bias_grad_size);

    // Compute bias gradient: sum over N and spatial dimensions
    kernel_conv2d_bias_grad<<<C_out, 256>>>(
        output_grad, bwd_ctx->d_bias_grad, N, C_out, spatial_out);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // For each sample in batch
    for (int n = 0; n < N; n++) {
        const float* input_n = input + n * C_in * H * W;
        const float* grad_out_n = output_grad + n * C_out * spatial_out;
        float* grad_input_n = bwd_ctx->d_input_grad + n * C_in * H * W;

        // 1. im2col on input
        nimcp_gpu_im2col(ctx, input_n, bwd_ctx->d_col_buffer,
                         C_in, H, W, kH, kW, sH, sW, pH, pW, outH, outW);

        // 2. Weight gradient: dW += grad_out @ col^T
        //    weight_grad: (C_out, C_in*kH*kW)
        //    grad_out: (C_out, outH*outW)
        //    col: (C_in*kH*kW, outH*outW)
        int wg_total = C_out * col_size;
        int wg_blocks = (wg_total + 255) / 256;
        kernel_conv2d_weight_grad<<<wg_blocks, 256>>>(
            bwd_ctx->d_col_buffer, grad_out_n,
            bwd_ctx->d_weight_grad, C_out, col_size, spatial_out);
        NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

        // 3. Input gradient: col_grad = W^T @ grad_out
        //    Then col2im to get input gradient
        //    For now, use simplified direct approach
        //    col_grad[k, s] = sum_co(weight[co, k] * grad_out[co, s])

        // Allocate temporary col_grad buffer if needed
        // For simplicity, compute directly with kernel
        float* d_col_grad = bwd_ctx->d_col_buffer;  // Reuse buffer

        // Compute col_grad = W^T @ grad_out
        // Use cublas for efficiency
        cublasHandle_t handle = (cublasHandle_t)ctx->cublas_handle;
        float alpha = 1.0f, beta = 0.0f;

        // W^T: (C_in*kH*kW, C_out), grad_out: (C_out, spatial_out)
        // result: (C_in*kH*kW, spatial_out)
        cublasStatus_t status = cublasSgemm(handle,
            CUBLAS_OP_N, CUBLAS_OP_T,
            spatial_out, col_size, C_out,
            &alpha,
            grad_out_n, spatial_out,
            weights, col_size,
            &beta,
            d_col_grad, spatial_out);

        if (status != CUBLAS_STATUS_SUCCESS) {
            LOG_ERROR("cuBLAS SGEMM failed for input gradient");
            return -1;
        }

        // 4. col2im to convert col_grad back to input gradient
        nimcp_gpu_col2im(ctx, d_col_grad, grad_input_n,
                         C_in, H, W, kH, kW, sH, sW, pH, pW, outH, outW);
    }

    return 0;
}

bool nimcp_gpu_conv2d_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_bias,
    const nimcp_conv_params_t* params)
{
    if (!ctx || !input || !weight || !grad_output || !params) return false;

    int N = input->dims[0];
    int C_in = input->dims[1];
    int H_in = input->dims[2];
    int W_in = input->dims[3];
    int C_out = weight->dims[0];
    int kH = params->kernel_h;
    int kW = params->kernel_w;

    // Create backward context
    nimcp_conv2d_backward_ctx_t* bwd_ctx = nimcp_conv2d_backward_create(
        ctx, N, C_in, H_in, W_in, C_out, kH, kW,
        params->stride_h, params->stride_w, params->pad_h, params->pad_w);

    if (!bwd_ctx) {
        LOG_ERROR("Failed to create conv2d backward context");
        return false;
    }

    // Perform backward pass
    int result = nimcp_conv2d_backward(bwd_ctx,
        (const float*)grad_output->data,
        (const float*)weight->data,
        (const float*)input->data);

    if (result != 0) {
        nimcp_conv2d_backward_destroy(bwd_ctx);
        return false;
    }

    // Copy gradients to output tensors
    size_t input_size = N * C_in * H_in * W_in * sizeof(float);
    size_t weight_size = C_out * C_in * kH * kW * sizeof(float);
    size_t bias_size = C_out * sizeof(float);

    /* W9: Check gradient copy results */
    bool copy_ok = true;
    if (grad_input) {
        if (cudaMemcpy(grad_input->data, bwd_ctx->d_input_grad, input_size, cudaMemcpyDeviceToDevice) != cudaSuccess)
            copy_ok = false;
    }
    if (grad_weight) {
        if (cudaMemcpy(grad_weight->data, bwd_ctx->d_weight_grad, weight_size, cudaMemcpyDeviceToDevice) != cudaSuccess)
            copy_ok = false;
    }
    if (grad_bias) {
        if (cudaMemcpy(grad_bias->data, bwd_ctx->d_bias_grad, bias_size, cudaMemcpyDeviceToDevice) != cudaSuccess)
            copy_ok = false;
    }

    nimcp_conv2d_backward_destroy(bwd_ctx);
    return copy_ok;
}

//=============================================================================
// Conv1D Forward (Audio/Speech)
//=============================================================================

__global__ void kernel_conv1d_forward(
    const float* input, const float* weight, const float* bias, float* output,
    int N, int C_in, int L_in, int C_out, int L_out,
    int K, int stride, int padding, int dilation)
{
    int n = blockIdx.z;
    int c_out = blockIdx.y;
    int l_out = blockIdx.x * blockDim.x + threadIdx.x;

    if (l_out >= L_out) return;

    float sum = 0.0f;
    for (int c_in = 0; c_in < C_in; c_in++) {
        for (int k = 0; k < K; k++) {
            int l_in = l_out * stride - padding + k * dilation;
            if (l_in >= 0 && l_in < L_in) {
                sum += input[n * C_in * L_in + c_in * L_in + l_in] *
                       weight[c_out * C_in * K + c_in * K + k];
            }
        }
    }

    if (bias) sum += bias[c_out];
    output[n * C_out * L_out + c_out * L_out + l_out] = sum;
}

bool nimcp_gpu_conv1d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    uint32_t kernel_size, uint32_t stride, uint32_t padding, uint32_t dilation)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !weight || !output) return false;

    int N = input->dims[0];
    int C_in = input->dims[1];
    int L_in = input->dims[2];
    int C_out = weight->dims[0];
    int L_out = (L_in + 2 * padding - dilation * (kernel_size - 1) - 1) / stride + 1;

    dim3 block(256);
    dim3 grid((L_out + 255) / 256, C_out, N);

    kernel_conv1d_forward<<<grid, block>>>(
        (const float*)input->data, (const float*)weight->data,
        bias ? (const float*)bias->data : NULL, (float*)output->data,
        N, C_in, L_in, C_out, L_out, kernel_size, stride, padding, dilation);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Depthwise Conv2D
//=============================================================================

__global__ void kernel_depthwise_conv2d(
    const float* input, const float* weight, const float* bias, float* output,
    int N, int C, int H_in, int W_in, int H_out, int W_out,
    int kH, int kW, int stride_h, int stride_w, int pad_h, int pad_w)
{
    int n = blockIdx.z;
    int c = blockIdx.y;
    int hw = blockIdx.x * blockDim.x + threadIdx.x;
    int h_out = hw / W_out;
    int w_out = hw % W_out;

    if (h_out >= H_out || w_out >= W_out) return;

    float sum = 0.0f;
    for (int kh = 0; kh < kH; kh++) {
        for (int kw = 0; kw < kW; kw++) {
            int h_in = h_out * stride_h - pad_h + kh;
            int w_in = w_out * stride_w - pad_w + kw;
            if (h_in >= 0 && h_in < H_in && w_in >= 0 && w_in < W_in) {
                sum += input[n * C * H_in * W_in + c * H_in * W_in + h_in * W_in + w_in] *
                       weight[c * kH * kW + kh * kW + kw];
            }
        }
    }

    if (bias) sum += bias[c];
    output[n * C * H_out * W_out + c * H_out * W_out + h_out * W_out + w_out] = sum;
}

bool nimcp_gpu_depthwise_conv2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    const nimcp_conv_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !weight || !output || !params) return false;

    int N = input->dims[0];
    int C = input->dims[1];
    int H_in = input->dims[2];
    int W_in = input->dims[3];
    int kH = params->kernel_h;
    int kW = params->kernel_w;
    int H_out = (H_in + 2 * params->pad_h - kH) / params->stride_h + 1;
    int W_out = (W_in + 2 * params->pad_w - kW) / params->stride_w + 1;

    dim3 block(256);
    dim3 grid((H_out * W_out + 255) / 256, C, N);

    kernel_depthwise_conv2d<<<grid, block>>>(
        (const float*)input->data, (const float*)weight->data,
        bias ? (const float*)bias->data : NULL, (float*)output->data,
        N, C, H_in, W_in, H_out, W_out,
        kH, kW, params->stride_h, params->stride_w, params->pad_h, params->pad_w);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Pooling Operations
//=============================================================================

__global__ void kernel_maxpool2d(
    const float* input, float* output, int* indices,
    int N, int C, int H_in, int W_in, int H_out, int W_out,
    int kH, int kW, int stride_h, int stride_w, int pad_h, int pad_w)
{
    int n = blockIdx.z;
    int c = blockIdx.y;
    int hw = blockIdx.x * blockDim.x + threadIdx.x;
    int h_out = hw / W_out;
    int w_out = hw % W_out;

    if (h_out >= H_out || w_out >= W_out) return;

    float max_val = -FLT_MAX;
    int max_idx = 0;

    for (int kh = 0; kh < kH; kh++) {
        for (int kw = 0; kw < kW; kw++) {
            int h_in = h_out * stride_h - pad_h + kh;
            int w_in = w_out * stride_w - pad_w + kw;
            if (h_in >= 0 && h_in < H_in && w_in >= 0 && w_in < W_in) {
                int idx = n * C * H_in * W_in + c * H_in * W_in + h_in * W_in + w_in;
                float val = input[idx];
                if (val > max_val) {
                    max_val = val;
                    max_idx = h_in * W_in + w_in;
                }
            }
        }
    }

    int out_idx = n * C * H_out * W_out + c * H_out * W_out + h_out * W_out + w_out;
    output[out_idx] = max_val;
    if (indices) indices[out_idx] = max_idx;
}

bool nimcp_gpu_maxpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* indices,
    const nimcp_pool_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output || !params) return false;

    int N = input->dims[0];
    int C = input->dims[1];
    int H_in = input->dims[2];
    int W_in = input->dims[3];
    int H_out = (H_in + 2 * params->pad_h - params->kernel_h) / params->stride_h + 1;
    int W_out = (W_in + 2 * params->pad_w - params->kernel_w) / params->stride_w + 1;

    dim3 block(256);
    dim3 grid((H_out * W_out + 255) / 256, C, N);

    kernel_maxpool2d<<<grid, block>>>(
        (const float*)input->data, (float*)output->data,
        indices ? (int*)indices->data : NULL,
        N, C, H_in, W_in, H_out, W_out,
        params->kernel_h, params->kernel_w, params->stride_h, params->stride_w,
        params->pad_h, params->pad_w);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__global__ void kernel_avgpool2d(
    const float* input, float* output,
    int N, int C, int H_in, int W_in, int H_out, int W_out,
    int kH, int kW, int stride_h, int stride_w, int pad_h, int pad_w)
{
    int n = blockIdx.z;
    int c = blockIdx.y;
    int hw = blockIdx.x * blockDim.x + threadIdx.x;
    int h_out = hw / W_out;
    int w_out = hw % W_out;

    if (h_out >= H_out || w_out >= W_out) return;

    float sum = 0.0f;
    int count = 0;

    for (int kh = 0; kh < kH; kh++) {
        for (int kw = 0; kw < kW; kw++) {
            int h_in = h_out * stride_h - pad_h + kh;
            int w_in = w_out * stride_w - pad_w + kw;
            if (h_in >= 0 && h_in < H_in && w_in >= 0 && w_in < W_in) {
                sum += input[n * C * H_in * W_in + c * H_in * W_in + h_in * W_in + w_in];
                count++;
            }
        }
    }

    output[n * C * H_out * W_out + c * H_out * W_out + h_out * W_out + w_out] =
        count > 0 ? sum / count : 0.0f;
}

bool nimcp_gpu_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_pool_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output || !params) return false;

    int N = input->dims[0];
    int C = input->dims[1];
    int H_in = input->dims[2];
    int W_in = input->dims[3];
    int H_out = (H_in + 2 * params->pad_h - params->kernel_h) / params->stride_h + 1;
    int W_out = (W_in + 2 * params->pad_w - params->kernel_w) / params->stride_w + 1;

    dim3 block(256);
    dim3 grid((H_out * W_out + 255) / 256, C, N);

    kernel_avgpool2d<<<grid, block>>>(
        (const float*)input->data, (float*)output->data,
        N, C, H_in, W_in, H_out, W_out,
        params->kernel_h, params->kernel_w, params->stride_h, params->stride_w,
        params->pad_h, params->pad_w);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__global__ void kernel_global_avgpool(
    const float* input, float* output, int N, int C, int HW)
{
    int n = blockIdx.y;
    int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c >= C) return;

    float sum = 0.0f;
    for (int i = 0; i < HW; i++) {
        sum += input[n * C * HW + c * HW + i];
    }
    output[n * C + c] = sum / HW;
}

bool nimcp_gpu_global_avgpool(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output) return false;

    int N = input->dims[0];
    int C = input->dims[1];
    int HW = input->dims[2] * input->dims[3];

    dim3 block(256);
    dim3 grid((C + 255) / 256, N);

    kernel_global_avgpool<<<grid, block>>>(
        (const float*)input->data, (float*)output->data, N, C, HW);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_adaptive_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    uint32_t output_h, uint32_t output_w)
{
    /* W9: Guard against division by zero */
    if (output_h == 0 || output_w == 0) return false;

    // Compute adaptive pooling params
    nimcp_pool_params_t params;
    params.stride_h = input->dims[2] / output_h;
    params.stride_w = input->dims[3] / output_w;
    params.kernel_h = input->dims[2] - (output_h - 1) * params.stride_h;
    params.kernel_w = input->dims[3] - (output_w - 1) * params.stride_w;
    params.pad_h = 0;
    params.pad_w = 0;

    return nimcp_gpu_avgpool2d(ctx, input, output, &params);
}

//=============================================================================
// Batch Normalization
//=============================================================================

__global__ void kernel_batchnorm2d_forward(
    const float* input, const float* gamma, const float* beta,
    float* output, float* running_mean, float* running_var,
    int N, int C, int HW, float momentum, float eps, bool training)
{
    int c = blockIdx.x;
    if (c >= C) return;

    // Compute mean and variance if training
    float mean = 0.0f, var = 0.0f;
    int total = N * HW;

    if (training) {
        // Compute mean
        for (int n = 0; n < N; n++) {
            for (int hw = threadIdx.x; hw < HW; hw += blockDim.x) {
                mean += input[n * C * HW + c * HW + hw];
            }
        }
        __syncthreads();
        // Reduce mean (simplified)
        mean /= total;

        // Compute variance
        for (int n = 0; n < N; n++) {
            for (int hw = threadIdx.x; hw < HW; hw += blockDim.x) {
                float diff = input[n * C * HW + c * HW + hw] - mean;
                var += diff * diff;
            }
        }
        __syncthreads();
        var /= total;

        // Update running stats
        if (threadIdx.x == 0) {
            running_mean[c] = (1.0f - momentum) * running_mean[c] + momentum * mean;
            running_var[c] = (1.0f - momentum) * running_var[c] + momentum * var;
        }
    } else {
        mean = running_mean[c];
        var = running_var[c];
    }

    float inv_std = rsqrtf(var + eps);
    float g = gamma ? gamma[c] : 1.0f;
    float b = beta ? beta[c] : 0.0f;

    // Normalize
    for (int n = 0; n < N; n++) {
        for (int hw = threadIdx.x; hw < HW; hw += blockDim.x) {
            int idx = n * C * HW + c * HW + hw;
            output[idx] = g * (input[idx] - mean) * inv_std + b;
        }
    }
}

bool nimcp_gpu_batchnorm2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* running_mean,
    nimcp_gpu_tensor_t* running_var,
    float momentum, float eps, bool training)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input || !output) return false;

    int N = input->dims[0];
    int C = input->dims[1];
    int HW = input->dims[2] * input->dims[3];

    kernel_batchnorm2d_forward<<<C, 256>>>(
        (const float*)input->data,
        gamma ? (const float*)gamma->data : NULL,
        beta ? (const float*)beta->data : NULL,
        (float*)output->data,
        running_mean ? (float*)running_mean->data : NULL,
        running_var ? (float*)running_var->data : NULL,
        N, C, HW, momentum, eps, training);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Layer Normalization Kernels
//=============================================================================

/**
 * @brief Layer normalization forward kernel
 *
 * WHAT: Normalizes input across the normalized dimension per sample
 * WHY:  Used in transformers for training stability
 * HOW:  Compute mean/var per sample, normalize, apply affine transform
 */
__global__ void kernel_layer_norm_forward(
    const float* input, float* output,
    const float* gamma, const float* beta,
    float* mean, float* var,
    int N, int normalized_size, float eps)
{
    int n = blockIdx.x;
    if (n >= N) return;

    const float* x = input + n * normalized_size;
    float* y = output + n * normalized_size;

    // Compute mean
    __shared__ float shared_sum[256];
    float thread_sum = 0.0f;

    for (int i = threadIdx.x; i < normalized_size; i += blockDim.x) {
        thread_sum += x[i];
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    // Reduction for mean
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }

    float m = shared_sum[0] / normalized_size;
    if (threadIdx.x == 0 && mean) {
        mean[n] = m;
    }
    __syncthreads();

    // Compute variance
    thread_sum = 0.0f;
    for (int i = threadIdx.x; i < normalized_size; i += blockDim.x) {
        float diff = x[i] - m;
        thread_sum += diff * diff;
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }

    float v = shared_sum[0] / normalized_size;
    if (threadIdx.x == 0 && var) {
        var[n] = v;
    }
    __syncthreads();

    float inv_std = rsqrtf(v + eps);

    // Normalize and apply affine transform
    for (int i = threadIdx.x; i < normalized_size; i += blockDim.x) {
        float x_hat = (x[i] - m) * inv_std;
        float g = gamma ? gamma[i] : 1.0f;
        float b = beta ? beta[i] : 0.0f;
        y[i] = g * x_hat + b;
    }
}

/**
 * @brief Layer normalization backward kernel
 *
 * Computes gradients w.r.t. input, gamma, and beta
 */
__global__ void kernel_layer_norm_backward(
    const float* grad_out, const float* input,
    const float* mean, const float* var,
    const float* gamma,
    float* grad_input, float* grad_gamma, float* grad_beta,
    int N, int normalized_size, float eps)
{
    int n = blockIdx.x;
    if (n >= N) return;

    const float* dy = grad_out + n * normalized_size;
    const float* x = input + n * normalized_size;
    float* dx = grad_input + n * normalized_size;

    float m = mean[n];
    float v = var[n];
    float inv_std = rsqrtf(v + eps);

    __shared__ float shared_sum_dy[256];
    __shared__ float shared_sum_dy_xhat[256];

    // Compute sums for gradient formulas
    float sum_dy = 0.0f;
    float sum_dy_xhat = 0.0f;

    for (int i = threadIdx.x; i < normalized_size; i += blockDim.x) {
        float x_hat = (x[i] - m) * inv_std;
        float g = gamma ? gamma[i] : 1.0f;
        sum_dy += dy[i] * g;
        sum_dy_xhat += dy[i] * g * x_hat;

        // Accumulate gamma/beta gradients (atomically across samples)
        if (grad_gamma) {
            atomicAdd(&grad_gamma[i], dy[i] * x_hat);
        }
        if (grad_beta) {
            atomicAdd(&grad_beta[i], dy[i]);
        }
    }

    shared_sum_dy[threadIdx.x] = sum_dy;
    shared_sum_dy_xhat[threadIdx.x] = sum_dy_xhat;
    __syncthreads();

    // Reduction
    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum_dy[threadIdx.x] += shared_sum_dy[threadIdx.x + stride];
            shared_sum_dy_xhat[threadIdx.x] += shared_sum_dy_xhat[threadIdx.x + stride];
        }
        __syncthreads();
    }

    float total_dy = shared_sum_dy[0];
    float total_dy_xhat = shared_sum_dy_xhat[0];

    // Compute input gradient
    // Formula: dx = inv_std * (d_x_hat - mean(d_x_hat) - x_hat * mean(d_x_hat * x_hat))
    // where d_x_hat = gamma * dy
    float inv_N = 1.0f / normalized_size;
    for (int i = threadIdx.x; i < normalized_size; i += blockDim.x) {
        float x_hat = (x[i] - m) * inv_std;
        float g = gamma ? gamma[i] : 1.0f;
        float d_x_hat = g * dy[i];
        dx[i] = inv_std * (d_x_hat - total_dy * inv_N - x_hat * total_dy_xhat * inv_N);
    }
}

//=============================================================================
// Layer Normalization Context API
//=============================================================================

nimcp_layer_norm_ctx_t* nimcp_layer_norm_create(
    nimcp_gpu_context_t* ctx,
    int normalized_shape,
    float eps)
{
    if (!ctx || normalized_shape <= 0) return NULL;

    nimcp_layer_norm_ctx_t* ln_ctx =
        (nimcp_layer_norm_ctx_t*)nimcp_malloc(sizeof(nimcp_layer_norm_ctx_t));
    if (!ln_ctx) return NULL;

    ln_ctx->normalized_shape = normalized_shape;
    ln_ctx->epsilon = eps;
    ln_ctx->ctx = ctx;

    // Allocate gamma and beta (initialized to 1 and 0)
    size_t param_size = normalized_shape * sizeof(float);
    ln_ctx->d_gamma = (float*)nimcp_gpu_malloc(ctx, param_size);
    ln_ctx->d_beta = (float*)nimcp_gpu_malloc(ctx, param_size);
    ln_ctx->d_mean = NULL;  // Allocated during forward pass if needed
    ln_ctx->d_var = NULL;

    if (!ln_ctx->d_gamma || !ln_ctx->d_beta) {
        nimcp_layer_norm_destroy(ln_ctx);
        return NULL;
    }

    // Initialize gamma to 1, beta to 0
    std::vector<float> ones(normalized_shape, 1.0f);
    std::vector<float> zeros(normalized_shape, 0.0f);
    nimcp_gpu_memcpy(ctx, ln_ctx->d_gamma, ones.data(), param_size, GPU_MEMCPY_HOST_TO_DEVICE);
    nimcp_gpu_memcpy(ctx, ln_ctx->d_beta, zeros.data(), param_size, GPU_MEMCPY_HOST_TO_DEVICE);

    return ln_ctx;
}

void nimcp_layer_norm_destroy(nimcp_layer_norm_ctx_t* ln_ctx)
{
    if (!ln_ctx) return;

    if (ln_ctx->ctx) {
        if (ln_ctx->d_gamma) nimcp_gpu_free(ln_ctx->ctx, ln_ctx->d_gamma);
        if (ln_ctx->d_beta) nimcp_gpu_free(ln_ctx->ctx, ln_ctx->d_beta);
        if (ln_ctx->d_mean) nimcp_gpu_free(ln_ctx->ctx, ln_ctx->d_mean);
        if (ln_ctx->d_var) nimcp_gpu_free(ln_ctx->ctx, ln_ctx->d_var);
    }

    nimcp_free(ln_ctx);
}

int nimcp_layer_norm_forward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* input,
    float* output,
    int batch_size)
{
    if (!ln_ctx || !input || !output) return -1;

    nimcp_gpu_context_t* ctx = ln_ctx->ctx;
    int N = batch_size;
    int D = ln_ctx->normalized_shape;

    // Allocate mean/var cache if needed
    if (!ln_ctx->d_mean) {
        ln_ctx->d_mean = (float*)nimcp_gpu_malloc(ctx, N * sizeof(float));
    }
    if (!ln_ctx->d_var) {
        ln_ctx->d_var = (float*)nimcp_gpu_malloc(ctx, N * sizeof(float));
    }

    kernel_layer_norm_forward<<<N, 256>>>(
        input, output,
        ln_ctx->d_gamma, ln_ctx->d_beta,
        ln_ctx->d_mean, ln_ctx->d_var,
        N, D, ln_ctx->epsilon);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Layer norm forward kernel failed: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

int nimcp_layer_norm_backward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* grad_output,
    const float* input,
    float* grad_input,
    float* grad_gamma,
    float* grad_beta,
    int batch_size)
{
    if (!ln_ctx || !grad_output || !input || !grad_input) return -1;
    if (!ln_ctx->d_mean || !ln_ctx->d_var) {
        LOG_ERROR("Layer norm backward requires forward pass to be called first");
        return -1;
    }

    int N = batch_size;
    int D = ln_ctx->normalized_shape;

    // Zero grad_gamma and grad_beta if provided (atomicAdd will accumulate)
    if (grad_gamma) {
        nimcp_gpu_memset(ln_ctx->ctx, grad_gamma, 0, D * sizeof(float));
    }
    if (grad_beta) {
        nimcp_gpu_memset(ln_ctx->ctx, grad_beta, 0, D * sizeof(float));
    }

    kernel_layer_norm_backward<<<N, 256>>>(
        grad_output, input,
        ln_ctx->d_mean, ln_ctx->d_var,
        ln_ctx->d_gamma,
        grad_input, grad_gamma, grad_beta,
        N, D, ln_ctx->epsilon);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Layer norm backward kernel failed: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

bool nimcp_gpu_layernorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    if (!ctx || !input || !output) return false;

    // Compute normalized shape (last dimension)
    int normalized_size = input->dims[input->ndim - 1];
    int batch_size = input->numel / normalized_size;

    // Use kernel directly
    kernel_layer_norm_forward<<<batch_size, 256>>>(
        (const float*)input->data, (float*)output->data,
        gamma ? (const float*)gamma->data : NULL,
        beta ? (const float*)beta->data : NULL,
        NULL, NULL,  // Don't cache mean/var
        batch_size, normalized_size, eps);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Instance Normalization Kernels
//=============================================================================

/**
 * @brief Instance normalization forward kernel
 *
 * WHAT: Normalizes per (batch, channel) across spatial dimensions
 * WHY:  Used in style transfer and image generation
 * HOW:  Compute mean/var per instance-channel pair
 */
__global__ void kernel_instance_norm_forward(
    const float* input, float* output,
    const float* gamma, const float* beta,
    float* mean, float* var,
    int N, int C, int HW, float eps, bool affine)
{
    int idx = blockIdx.x;  // idx = n * C + c
    if (idx >= N * C) return;

    int n = idx / C;
    int c = idx % C;

    const float* x = input + (n * C + c) * HW;
    float* y = output + (n * C + c) * HW;

    // Compute mean
    __shared__ float shared_sum[256];
    float thread_sum = 0.0f;

    for (int i = threadIdx.x; i < HW; i += blockDim.x) {
        thread_sum += x[i];
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }

    float m = shared_sum[0] / HW;
    if (threadIdx.x == 0 && mean) {
        mean[idx] = m;
    }
    __syncthreads();

    // Compute variance
    thread_sum = 0.0f;
    for (int i = threadIdx.x; i < HW; i += blockDim.x) {
        float diff = x[i] - m;
        thread_sum += diff * diff;
    }
    shared_sum[threadIdx.x] = thread_sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum[threadIdx.x] += shared_sum[threadIdx.x + stride];
        }
        __syncthreads();
    }

    float v = shared_sum[0] / HW;
    if (threadIdx.x == 0 && var) {
        var[idx] = v;
    }
    __syncthreads();

    float inv_std = rsqrtf(v + eps);
    float g = (affine && gamma) ? gamma[c] : 1.0f;
    float b = (affine && beta) ? beta[c] : 0.0f;

    // Normalize and apply affine transform
    for (int i = threadIdx.x; i < HW; i += blockDim.x) {
        float x_hat = (x[i] - m) * inv_std;
        y[i] = g * x_hat + b;
    }
}

/**
 * @brief Instance normalization backward kernel
 */
__global__ void kernel_instance_norm_backward(
    const float* grad_out, const float* input,
    const float* mean, const float* var,
    const float* gamma,
    float* grad_input, float* grad_gamma, float* grad_beta,
    int N, int C, int HW, float eps, bool affine)
{
    int idx = blockIdx.x;  // idx = n * C + c
    if (idx >= N * C) return;

    int n = idx / C;
    int c = idx % C;

    const float* dy = grad_out + (n * C + c) * HW;
    const float* x = input + (n * C + c) * HW;
    float* dx = grad_input + (n * C + c) * HW;

    float m = mean[idx];
    float v = var[idx];
    float inv_std = rsqrtf(v + eps);
    float g = (affine && gamma) ? gamma[c] : 1.0f;

    __shared__ float shared_sum_dy[256];
    __shared__ float shared_sum_dy_xhat[256];

    float sum_dy = 0.0f;
    float sum_dy_xhat = 0.0f;

    for (int i = threadIdx.x; i < HW; i += blockDim.x) {
        float x_hat = (x[i] - m) * inv_std;
        sum_dy += dy[i] * g;
        sum_dy_xhat += dy[i] * g * x_hat;

        // Accumulate gamma/beta gradients
        if (affine && grad_gamma) {
            atomicAdd(&grad_gamma[c], dy[i] * x_hat);
        }
        if (affine && grad_beta) {
            atomicAdd(&grad_beta[c], dy[i]);
        }
    }

    shared_sum_dy[threadIdx.x] = sum_dy;
    shared_sum_dy_xhat[threadIdx.x] = sum_dy_xhat;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            shared_sum_dy[threadIdx.x] += shared_sum_dy[threadIdx.x + stride];
            shared_sum_dy_xhat[threadIdx.x] += shared_sum_dy_xhat[threadIdx.x + stride];
        }
        __syncthreads();
    }

    float total_dy = shared_sum_dy[0];
    float total_dy_xhat = shared_sum_dy_xhat[0];

    // Compute input gradient
    // Formula: dx = inv_std * (d_x_hat - mean(d_x_hat) - x_hat * mean(d_x_hat * x_hat))
    // where d_x_hat = gamma * dy
    float inv_HW = 1.0f / HW;
    for (int i = threadIdx.x; i < HW; i += blockDim.x) {
        float x_hat = (x[i] - m) * inv_std;
        float d_x_hat = g * dy[i];
        dx[i] = inv_std * (d_x_hat - total_dy * inv_HW - x_hat * total_dy_xhat * inv_HW);
    }
}

//=============================================================================
// Instance Normalization Context API
//=============================================================================

nimcp_instance_norm_ctx_t* nimcp_instance_norm_create(
    nimcp_gpu_context_t* ctx,
    int num_features,
    float eps,
    bool affine)
{
    if (!ctx || num_features <= 0) return NULL;

    nimcp_instance_norm_ctx_t* in_ctx =
        (nimcp_instance_norm_ctx_t*)nimcp_malloc(sizeof(nimcp_instance_norm_ctx_t));
    if (!in_ctx) return NULL;

    in_ctx->num_features = num_features;
    in_ctx->epsilon = eps;
    in_ctx->affine = affine;
    in_ctx->ctx = ctx;
    in_ctx->d_mean = NULL;
    in_ctx->d_var = NULL;

    if (affine) {
        size_t param_size = num_features * sizeof(float);
        in_ctx->d_gamma = (float*)nimcp_gpu_malloc(ctx, param_size);
        in_ctx->d_beta = (float*)nimcp_gpu_malloc(ctx, param_size);

        if (!in_ctx->d_gamma || !in_ctx->d_beta) {
            nimcp_instance_norm_destroy(in_ctx);
            return NULL;
        }

        // Initialize gamma to 1, beta to 0
        std::vector<float> ones(num_features, 1.0f);
        std::vector<float> zeros(num_features, 0.0f);
        nimcp_gpu_memcpy(ctx, in_ctx->d_gamma, ones.data(), param_size, GPU_MEMCPY_HOST_TO_DEVICE);
        nimcp_gpu_memcpy(ctx, in_ctx->d_beta, zeros.data(), param_size, GPU_MEMCPY_HOST_TO_DEVICE);
    } else {
        in_ctx->d_gamma = NULL;
        in_ctx->d_beta = NULL;
    }

    return in_ctx;
}

void nimcp_instance_norm_destroy(nimcp_instance_norm_ctx_t* in_ctx)
{
    if (!in_ctx) return;

    if (in_ctx->ctx) {
        if (in_ctx->d_gamma) nimcp_gpu_free(in_ctx->ctx, in_ctx->d_gamma);
        if (in_ctx->d_beta) nimcp_gpu_free(in_ctx->ctx, in_ctx->d_beta);
        if (in_ctx->d_mean) nimcp_gpu_free(in_ctx->ctx, in_ctx->d_mean);
        if (in_ctx->d_var) nimcp_gpu_free(in_ctx->ctx, in_ctx->d_var);
    }

    nimcp_free(in_ctx);
}

int nimcp_instance_norm_forward(
    nimcp_instance_norm_ctx_t* in_ctx,
    const float* input,
    float* output,
    int batch_size,
    int height,
    int width)
{
    if (!in_ctx || !input || !output) return -1;

    nimcp_gpu_context_t* ctx = in_ctx->ctx;
    int N = batch_size;
    int C = in_ctx->num_features;
    int HW = height * width;

    // Allocate mean/var cache if needed
    int total_instances = N * C;
    if (!in_ctx->d_mean) {
        in_ctx->d_mean = (float*)nimcp_gpu_malloc(ctx, total_instances * sizeof(float));
    }
    if (!in_ctx->d_var) {
        in_ctx->d_var = (float*)nimcp_gpu_malloc(ctx, total_instances * sizeof(float));
    }

    kernel_instance_norm_forward<<<total_instances, 256>>>(
        input, output,
        in_ctx->d_gamma, in_ctx->d_beta,
        in_ctx->d_mean, in_ctx->d_var,
        N, C, HW, in_ctx->epsilon, in_ctx->affine);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Instance norm forward kernel failed: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

int nimcp_instance_norm_backward(
    nimcp_instance_norm_ctx_t* in_ctx,
    const float* grad_output,
    const float* input,
    float* grad_input,
    float* grad_gamma,
    float* grad_beta,
    int batch_size,
    int height,
    int width)
{
    if (!in_ctx || !grad_output || !input || !grad_input) return -1;
    if (!in_ctx->d_mean || !in_ctx->d_var) {
        LOG_ERROR("Instance norm backward requires forward pass to be called first");
        return -1;
    }

    int N = batch_size;
    int C = in_ctx->num_features;
    int HW = height * width;
    int total_instances = N * C;

    // Zero grad_gamma and grad_beta if provided
    if (in_ctx->affine && grad_gamma) {
        nimcp_gpu_memset(in_ctx->ctx, grad_gamma, 0, C * sizeof(float));
    }
    if (in_ctx->affine && grad_beta) {
        nimcp_gpu_memset(in_ctx->ctx, grad_beta, 0, C * sizeof(float));
    }

    kernel_instance_norm_backward<<<total_instances, 256>>>(
        grad_output, input,
        in_ctx->d_mean, in_ctx->d_var,
        in_ctx->d_gamma,
        grad_input, grad_gamma, grad_beta,
        N, C, HW, in_ctx->epsilon, in_ctx->affine);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Instance norm backward kernel failed: %s", cudaGetErrorString(err));
        return -1;
    }

    return 0;
}

bool nimcp_gpu_instancenorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    if (!ctx || !input || !output) return false;
    if (input->ndim != 4) {
        LOG_ERROR("Instance norm expects 4D input (N, C, H, W)");
        return false;
    }

    int N = input->dims[0];
    int C = input->dims[1];
    int H = input->dims[2];
    int W = input->dims[3];
    int HW = H * W;
    int total_instances = N * C;
    bool affine = (gamma != NULL);

    kernel_instance_norm_forward<<<total_instances, 256>>>(
        (const float*)input->data, (float*)output->data,
        gamma ? (const float*)gamma->data : NULL,
        beta ? (const float*)beta->data : NULL,
        NULL, NULL,  // Don't cache mean/var
        N, C, HW, eps, affine);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/cnn/nimcp_cnn_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "CNN_GPU"

bool nimcp_gpu_conv2d_forward(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight, const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output, const nimcp_conv_params_t* params)
{
    LOG_WARN("CUDA not available - CNN requires GPU");
    return false;
}

bool nimcp_gpu_conv2d_backward(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight, const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input, nimcp_gpu_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_bias, const nimcp_conv_params_t* params)
{
    return false;
}

bool nimcp_gpu_conv1d_forward(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight, const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output, uint32_t ks, uint32_t s, uint32_t p, uint32_t d)
{
    return false;
}

bool nimcp_gpu_depthwise_conv2d(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight, const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output, const nimcp_conv_params_t* params)
{
    return false;
}

bool nimcp_gpu_maxpool2d(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output, nimcp_gpu_tensor_t* indices, const nimcp_pool_params_t* params)
{
    return false;
}

bool nimcp_gpu_avgpool2d(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output, const nimcp_pool_params_t* params)
{
    return false;
}

bool nimcp_gpu_global_avgpool(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    return false;
}

bool nimcp_gpu_adaptive_avgpool2d(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output, uint32_t h, uint32_t w)
{
    return false;
}

bool nimcp_gpu_batchnorm2d_forward(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output, nimcp_gpu_tensor_t* rm, nimcp_gpu_tensor_t* rv,
    float momentum, float eps, bool training)
{
    return false;
}

bool nimcp_gpu_layernorm_forward(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output, float eps)
{
    return false;
}

bool nimcp_gpu_instancenorm_forward(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma, const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output, float eps)
{
    return false;
}

// Conv2D Backward Context API stubs
nimcp_conv2d_backward_ctx_t* nimcp_conv2d_backward_create(
    nimcp_gpu_context_t* ctx,
    int batch_size, int in_channels, int in_height, int in_width,
    int out_channels, int kernel_h, int kernel_w,
    int stride_h, int stride_w, int pad_h, int pad_w)
{
    LOG_WARN("CUDA not available - conv2d backward requires GPU");
    return NULL;
}

void nimcp_conv2d_backward_destroy(nimcp_conv2d_backward_ctx_t* bwd_ctx)
{
    // No-op for non-CUDA build
}

int nimcp_conv2d_backward(
    nimcp_conv2d_backward_ctx_t* bwd_ctx,
    const float* output_grad,
    const float* weights,
    const float* input)
{
    return -1;
}

// im2col/col2im stubs
bool nimcp_gpu_im2col(
    nimcp_gpu_context_t* ctx,
    const float* input, float* col,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    return false;
}

bool nimcp_gpu_col2im(
    nimcp_gpu_context_t* ctx,
    const float* col, float* input_grad,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    return false;
}

// Layer Normalization API stubs
nimcp_layer_norm_ctx_t* nimcp_layer_norm_create(
    nimcp_gpu_context_t* ctx,
    int normalized_shape,
    float eps)
{
    LOG_WARN("CUDA not available - layer norm requires GPU");
    return NULL;
}

void nimcp_layer_norm_destroy(nimcp_layer_norm_ctx_t* ln_ctx)
{
    // No-op for non-CUDA build
}

int nimcp_layer_norm_forward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* input,
    float* output,
    int batch_size)
{
    return -1;
}

int nimcp_layer_norm_backward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* grad_output,
    const float* input,
    float* grad_input,
    float* grad_gamma,
    float* grad_beta,
    int batch_size)
{
    return -1;
}

// Instance Normalization API stubs
nimcp_instance_norm_ctx_t* nimcp_instance_norm_create(
    nimcp_gpu_context_t* ctx,
    int num_features,
    float eps,
    bool affine)
{
    LOG_WARN("CUDA not available - instance norm requires GPU");
    return NULL;
}

void nimcp_instance_norm_destroy(nimcp_instance_norm_ctx_t* in_ctx)
{
    // No-op for non-CUDA build
}

int nimcp_instance_norm_forward(
    nimcp_instance_norm_ctx_t* in_ctx,
    const float* input,
    float* output,
    int batch_size,
    int height,
    int width)
{
    return -1;
}

int nimcp_instance_norm_backward(
    nimcp_instance_norm_ctx_t* in_ctx,
    const float* grad_output,
    const float* input,
    float* grad_input,
    float* grad_gamma,
    float* grad_beta,
    int batch_size,
    int height,
    int width)
{
    return -1;
}

#endif // NIMCP_ENABLE_CUDA
