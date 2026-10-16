/**
 * @file nimcp_gpu_stubs_cnn.c
 * @brief CPU fallback implementations for GPU CNN operations
 *
 * WHAT: CPU implementations for convolution, pooling, and normalization layers
 * WHY:  Enables CNN operations on CPU-only systems without CUDA
 * HOW:  im2col-based convolution, sliding window pooling, per-channel normalization
 *
 * All 24 functions from gpu/cnn/nimcp_cnn_gpu.h are implemented here.
 * Tensors use NCHW format (batch, channels, height, width).
 *
 * @author NIMCP Development Team
 * @date 2026
 */

#include "gpu/cnn/nimcp_cnn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>

/*=============================================================================
 * Internal Helpers
 *=============================================================================*/

/** Helper: access float data from gpu_tensor (CPU fallback - data is host memory) */
static inline float* tensor_data_f(const nimcp_gpu_tensor_t* t) {
    return (float*)t->data;
}

/** Helper: access uint32 data from gpu_tensor for indices */
static inline uint32_t* tensor_data_u32(const nimcp_gpu_tensor_t* t) {
    return (uint32_t*)t->data;
}

/** Helper: compute output size for convolution/pooling */
static inline int conv_output_size(int input, int kernel, int stride, int pad, int dilation) {
    return (input + 2 * pad - dilation * (kernel - 1) - 1) / stride + 1;
}

/*=============================================================================
 * im2col / col2im Utilities (Functions 23, 24)
 *=============================================================================*/

bool nimcp_gpu_im2col(
    nimcp_gpu_context_t* ctx,
    const float* input, float* col,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW)
{
    (void)ctx;

    if (!input || !col) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_im2col: required parameter is NULL");
        return false;
    }

    /* col layout: (C*kH*kW) rows x (outH*outW) cols */
    int col_rows = C * kH * kW;
    int col_cols = outH * outW;

    for (int c = 0; c < C; c++) {
        for (int kh = 0; kh < kH; kh++) {
            for (int kw = 0; kw < kW; kw++) {
                int col_row = (c * kH + kh) * kW + kw;
                for (int oh = 0; oh < outH; oh++) {
                    for (int ow = 0; ow < outW; ow++) {
                        int ih = oh * sH - pH + kh;
                        int iw = ow * sW - pW + kw;
                        int col_idx = col_row * col_cols + oh * outW + ow;
                        if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                            col[col_idx] = input[(c * H + ih) * W + iw];
                        } else {
                            col[col_idx] = 0.0f;  /* zero-padding */
                        }
                    }
                }
            }
        }
    }

    (void)col_rows;  /* used conceptually for layout */
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
    (void)ctx;

    if (!col || !input_grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_col2im: required parameter is NULL");
        return false;
    }

    /* Zero the output first - col2im accumulates (scatter-add) */
    memset(input_grad, 0, (size_t)C * H * W * sizeof(float));

    int col_cols = outH * outW;

    for (int c = 0; c < C; c++) {
        for (int kh = 0; kh < kH; kh++) {
            for (int kw = 0; kw < kW; kw++) {
                int col_row = (c * kH + kh) * kW + kw;
                for (int oh = 0; oh < outH; oh++) {
                    for (int ow = 0; ow < outW; ow++) {
                        int ih = oh * sH - pH + kh;
                        int iw = ow * sW - pW + kw;
                        if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                            int col_idx = col_row * col_cols + oh * outW + ow;
                            input_grad[(c * H + ih) * W + iw] += col[col_idx];
                        }
                    }
                }
            }
        }
    }

    return true;
}

/*=============================================================================
 * Conv2D Forward (Function 1) - im2col + matmul approach
 *=============================================================================*/

bool nimcp_gpu_conv2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    const nimcp_conv_params_t* params)
{
    (void)ctx;

    if (!input || !weight || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_conv2d_forward: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* w_data = tensor_data_f(weight);
    const float* b_data = bias ? tensor_data_f(bias) : NULL;
    float* out_data = tensor_data_f(output);

    /* Extract dimensions from tensors: input (N, C_in, H, W) */
    int N = (int)input->dims[0];
    int C_in = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];

    /* weight (C_out, C_in/groups, kH, kW) */
    int C_out = (int)weight->dims[0];
    int kH = (int)params->kernel_h;
    int kW = (int)params->kernel_w;
    int sH = (int)params->stride_h;
    int sW = (int)params->stride_w;
    int pH = (int)params->pad_h;
    int pW = (int)params->pad_w;
    int dH = (int)params->dilation_h;
    int dW = (int)params->dilation_w;
    int groups = (int)params->groups;

    if (groups < 1) groups = 1;

    /* Effective kernel size with dilation */
    int eff_kH = dH * (kH - 1) + 1;
    int eff_kW = dW * (kW - 1) + 1;

    int outH = conv_output_size(H, kH, sH, pH, dH);
    int outW = conv_output_size(W, kW, sW, pW, dW);

    int C_in_per_group = C_in / groups;
    int C_out_per_group = C_out / groups;

    /* Allocate im2col buffer: (C_in_per_group * kH * kW) x (outH * outW) */
    size_t col_size = (size_t)C_in_per_group * kH * kW * outH * outW;
    float* col_buffer = (float*)nimcp_calloc(col_size, sizeof(float));
    if (!col_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_gpu_conv2d_forward: col_buffer allocation failed");
        return false;
    }

    int col_rows = C_in_per_group * kH * kW;
    int col_cols = outH * outW;

    for (int n = 0; n < N; n++) {
        for (int g = 0; g < groups; g++) {
            /* im2col for this batch sample and group */
            /* Handle dilation: manually unroll with dilation offsets */
            const float* in_ptr = in_data + n * C_in * H * W + g * C_in_per_group * H * W;

            for (int c = 0; c < C_in_per_group; c++) {
                for (int kh = 0; kh < kH; kh++) {
                    for (int kw = 0; kw < kW; kw++) {
                        int col_row = (c * kH + kh) * kW + kw;
                        for (int oh = 0; oh < outH; oh++) {
                            for (int ow = 0; ow < outW; ow++) {
                                int ih = oh * sH - pH + kh * dH;
                                int iw = ow * sW - pW + kw * dW;
                                int col_idx = col_row * col_cols + oh * outW + ow;
                                if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                    col_buffer[col_idx] = in_ptr[(c * H + ih) * W + iw];
                                } else {
                                    col_buffer[col_idx] = 0.0f;
                                }
                            }
                        }
                    }
                }
            }

            /* Matrix multiply: output = weight_group @ col_buffer */
            /* weight_group shape: (C_out_per_group, C_in_per_group * kH * kW) */
            /* col_buffer shape: (C_in_per_group * kH * kW, outH * outW) */
            /* result shape: (C_out_per_group, outH * outW) */
            const float* w_group = w_data + g * C_out_per_group * C_in_per_group * kH * kW;
            float* out_ptr = out_data + n * C_out * outH * outW + g * C_out_per_group * outH * outW;

            for (int oc = 0; oc < C_out_per_group; oc++) {
                /* Initialize with bias if present */
                float bias_val = b_data ? b_data[g * C_out_per_group + oc] : 0.0f;
                for (int j = 0; j < col_cols; j++) {
                    out_ptr[oc * col_cols + j] = bias_val;
                }

                /* Accumulate weight @ col */
                const float* w_row = w_group + oc * col_rows;
                for (int k = 0; k < col_rows; k++) {
                    float wk = w_row[k];
                    const float* col_row_ptr = col_buffer + k * col_cols;
                    float* out_row = out_ptr + oc * col_cols;
                    for (int j = 0; j < col_cols; j++) {
                        out_row[j] += wk * col_row_ptr[j];
                    }
                }
            }
        }
    }

    nimcp_free(col_buffer);
    (void)eff_kH;
    (void)eff_kW;
    return true;
}

/*=============================================================================
 * Conv2D Backward (Function 2) - tensor-based interface
 *=============================================================================*/

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
    (void)ctx;

    if (!input || !weight || !grad_output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_conv2d_backward: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* w_data = tensor_data_f(weight);
    const float* go_data = tensor_data_f(grad_output);

    int N = (int)input->dims[0];
    int C_in = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int C_out = (int)weight->dims[0];
    int kH = (int)params->kernel_h;
    int kW = (int)params->kernel_w;
    int sH = (int)params->stride_h;
    int sW = (int)params->stride_w;
    int pH = (int)params->pad_h;
    int pW = (int)params->pad_w;
    int dH = (int)params->dilation_h;
    int dW = (int)params->dilation_w;
    int groups = (int)params->groups;

    if (groups < 1) groups = 1;

    int outH = conv_output_size(H, kH, sH, pH, dH);
    int outW = conv_output_size(W, kW, sW, pW, dW);

    int C_in_per_group = C_in / groups;
    int C_out_per_group = C_out / groups;
    int col_rows = C_in_per_group * kH * kW;
    int col_cols = outH * outW;

    /* Allocate im2col buffer */
    size_t col_size = (size_t)col_rows * col_cols;
    float* col_buffer = (float*)nimcp_calloc(col_size, sizeof(float));
    if (!col_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_gpu_conv2d_backward: col_buffer allocation failed");
        return false;
    }

    /* Zero gradients */
    if (grad_input) {
        memset(tensor_data_f(grad_input), 0, (size_t)N * C_in * H * W * sizeof(float));
    }
    if (grad_weight) {
        memset(tensor_data_f(grad_weight), 0, (size_t)C_out * C_in_per_group * kH * kW * sizeof(float));
    }
    if (grad_bias) {
        memset(tensor_data_f(grad_bias), 0, (size_t)C_out * sizeof(float));
    }

    for (int n = 0; n < N; n++) {
        for (int g = 0; g < groups; g++) {
            /* im2col of input for this batch/group */
            const float* in_ptr = in_data + n * C_in * H * W + g * C_in_per_group * H * W;
            for (int c = 0; c < C_in_per_group; c++) {
                for (int kh = 0; kh < kH; kh++) {
                    for (int kw = 0; kw < kW; kw++) {
                        int cr = (c * kH + kh) * kW + kw;
                        for (int oh = 0; oh < outH; oh++) {
                            for (int ow = 0; ow < outW; ow++) {
                                int ih = oh * sH - pH + kh * dH;
                                int iw = ow * sW - pW + kw * dW;
                                int ci = cr * col_cols + oh * outW + ow;
                                if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                    col_buffer[ci] = in_ptr[(c * H + ih) * W + iw];
                                } else {
                                    col_buffer[ci] = 0.0f;
                                }
                            }
                        }
                    }
                }
            }

            const float* go_ptr = go_data + n * C_out * outH * outW + g * C_out_per_group * outH * outW;

            /* grad_weight += grad_output_reshaped @ col^T */
            if (grad_weight) {
                float* gw_group = tensor_data_f(grad_weight) + g * C_out_per_group * C_in_per_group * kH * kW;
                for (int oc = 0; oc < C_out_per_group; oc++) {
                    const float* go_row = go_ptr + oc * col_cols;
                    float* gw_row = gw_group + oc * col_rows;
                    for (int k = 0; k < col_rows; k++) {
                        float sum = 0.0f;
                        const float* col_row_ptr = col_buffer + k * col_cols;
                        for (int j = 0; j < col_cols; j++) {
                            sum += go_row[j] * col_row_ptr[j];
                        }
                        gw_row[k] += sum;
                    }
                }
            }

            /* grad_bias += sum over spatial dims of grad_output */
            if (grad_bias) {
                float* gb_data = tensor_data_f(grad_bias) + g * C_out_per_group;
                for (int oc = 0; oc < C_out_per_group; oc++) {
                    const float* go_row = go_ptr + oc * col_cols;
                    for (int j = 0; j < col_cols; j++) {
                        gb_data[oc] += go_row[j];
                    }
                }
            }

            /* grad_input via col2im of (weight^T @ grad_output) */
            if (grad_input) {
                const float* w_group = w_data + g * C_out_per_group * C_in_per_group * kH * kW;

                /* Compute col_grad = weight^T @ grad_output_reshaped */
                /* weight^T shape: (col_rows, C_out_per_group) */
                /* go_reshaped shape: (C_out_per_group, col_cols) */
                /* col_grad shape: (col_rows, col_cols) */
                memset(col_buffer, 0, col_size * sizeof(float));

                for (int k = 0; k < col_rows; k++) {
                    float* col_row_ptr = col_buffer + k * col_cols;
                    for (int oc = 0; oc < C_out_per_group; oc++) {
                        float w_val = w_group[oc * col_rows + k];
                        const float* go_row = go_ptr + oc * col_cols;
                        for (int j = 0; j < col_cols; j++) {
                            col_row_ptr[j] += w_val * go_row[j];
                        }
                    }
                }

                /* col2im: scatter-add col_buffer back to grad_input */
                float* gi_ptr = tensor_data_f(grad_input) + n * C_in * H * W + g * C_in_per_group * H * W;
                for (int c = 0; c < C_in_per_group; c++) {
                    for (int kh = 0; kh < kH; kh++) {
                        for (int kw = 0; kw < kW; kw++) {
                            int cr = (c * kH + kh) * kW + kw;
                            for (int oh = 0; oh < outH; oh++) {
                                for (int ow = 0; ow < outW; ow++) {
                                    int ih = oh * sH - pH + kh * dH;
                                    int iw = ow * sW - pW + kw * dW;
                                    if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                        int ci = cr * col_cols + oh * outW + ow;
                                        gi_ptr[(c * H + ih) * W + iw] += col_buffer[ci];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    nimcp_free(col_buffer);
    return true;
}

/*=============================================================================
 * Conv1D Forward (Function 3) - treat as conv2d with height=1
 *=============================================================================*/

bool nimcp_gpu_conv1d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    uint32_t kernel_size, uint32_t stride, uint32_t padding, uint32_t dilation)
{
    (void)ctx;

    if (!input || !weight || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_conv1d_forward: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* w_data = tensor_data_f(weight);
    const float* b_data = bias ? tensor_data_f(bias) : NULL;
    float* out_data = tensor_data_f(output);

    /* input (N, C_in, L), weight (C_out, C_in, K) */
    int N = (int)input->dims[0];
    int C_in = (int)input->dims[1];
    int L = (int)input->dims[2];
    int C_out = (int)weight->dims[0];
    int K = (int)kernel_size;
    int S = (int)stride;
    int P = (int)padding;
    int D = (int)dilation;

    int eff_K = D * (K - 1) + 1;
    int outL = (L + 2 * P - eff_K) / S + 1;

    for (int n = 0; n < N; n++) {
        for (int oc = 0; oc < C_out; oc++) {
            float bias_val = b_data ? b_data[oc] : 0.0f;
            for (int ol = 0; ol < outL; ol++) {
                float sum = bias_val;
                for (int ic = 0; ic < C_in; ic++) {
                    for (int k = 0; k < K; k++) {
                        int il = ol * S - P + k * D;
                        if (il >= 0 && il < L) {
                            sum += w_data[(oc * C_in + ic) * K + k]
                                 * in_data[(n * C_in + ic) * L + il];
                        }
                    }
                }
                out_data[(n * C_out + oc) * outL + ol] = sum;
            }
        }
    }

    (void)eff_K;
    return true;
}

/*=============================================================================
 * Depthwise Conv2D (Function 4)
 *=============================================================================*/

bool nimcp_gpu_depthwise_conv2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    const nimcp_conv_params_t* params)
{
    (void)ctx;

    if (!input || !weight || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_depthwise_conv2d: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* w_data = tensor_data_f(weight);
    const float* b_data = bias ? tensor_data_f(bias) : NULL;
    float* out_data = tensor_data_f(output);

    /* input (N, C, H, W), weight (C, 1, kH, kW) */
    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int kH = (int)params->kernel_h;
    int kW = (int)params->kernel_w;
    int sH = (int)params->stride_h;
    int sW = (int)params->stride_w;
    int pH = (int)params->pad_h;
    int pW = (int)params->pad_w;
    int dH = (int)params->dilation_h;
    int dW = (int)params->dilation_w;

    int outH = conv_output_size(H, kH, sH, pH, dH);
    int outW = conv_output_size(W, kW, sW, pW, dW);

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            float bias_val = b_data ? b_data[c] : 0.0f;
            const float* in_ch = in_data + (n * C + c) * H * W;
            const float* w_ch = w_data + c * kH * kW;  /* (C, 1, kH, kW) */
            float* out_ch = out_data + (n * C + c) * outH * outW;

            for (int oh = 0; oh < outH; oh++) {
                for (int ow = 0; ow < outW; ow++) {
                    float sum = bias_val;
                    for (int kh = 0; kh < kH; kh++) {
                        for (int kw = 0; kw < kW; kw++) {
                            int ih = oh * sH - pH + kh * dH;
                            int iw = ow * sW - pW + kw * dW;
                            if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                sum += in_ch[ih * W + iw] * w_ch[kh * kW + kw];
                            }
                        }
                    }
                    out_ch[oh * outW + ow] = sum;
                }
            }
        }
    }

    return true;
}

/*=============================================================================
 * MaxPool2D (Function 5) - sliding window max with optional indices
 *=============================================================================*/

bool nimcp_gpu_maxpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* indices,
    const nimcp_pool_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_maxpool2d: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    float* out_data = tensor_data_f(output);
    uint32_t* idx_data = indices ? tensor_data_u32(indices) : NULL;

    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int kH = (int)params->kernel_h;
    int kW = (int)params->kernel_w;
    int sH = (int)params->stride_h;
    int sW = (int)params->stride_w;
    int pH = (int)params->pad_h;
    int pW = (int)params->pad_w;

    int outH = (H + 2 * pH - kH) / sH + 1;
    int outW = (W + 2 * pW - kW) / sW + 1;

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            const float* in_ch = in_data + (n * C + c) * H * W;
            float* out_ch = out_data + (n * C + c) * outH * outW;
            uint32_t* idx_ch = idx_data ? idx_data + (n * C + c) * outH * outW : NULL;

            for (int oh = 0; oh < outH; oh++) {
                for (int ow = 0; ow < outW; ow++) {
                    float max_val = -FLT_MAX;
                    uint32_t max_idx = 0;

                    for (int kh = 0; kh < kH; kh++) {
                        for (int kw = 0; kw < kW; kw++) {
                            int ih = oh * sH - pH + kh;
                            int iw = ow * sW - pW + kw;
                            if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                float val = in_ch[ih * W + iw];
                                if (val > max_val) {
                                    max_val = val;
                                    max_idx = (uint32_t)(ih * W + iw);
                                }
                            }
                        }
                    }

                    out_ch[oh * outW + ow] = max_val;
                    if (idx_ch) {
                        idx_ch[oh * outW + ow] = max_idx;
                    }
                }
            }
        }
    }

    return true;
}

/*=============================================================================
 * AvgPool2D (Function 6) - sliding window average
 *=============================================================================*/

bool nimcp_gpu_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_pool_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_avgpool2d: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    float* out_data = tensor_data_f(output);

    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int kH = (int)params->kernel_h;
    int kW = (int)params->kernel_w;
    int sH = (int)params->stride_h;
    int sW = (int)params->stride_w;
    int pH = (int)params->pad_h;
    int pW = (int)params->pad_w;

    int outH = (H + 2 * pH - kH) / sH + 1;
    int outW = (W + 2 * pW - kW) / sW + 1;

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            const float* in_ch = in_data + (n * C + c) * H * W;
            float* out_ch = out_data + (n * C + c) * outH * outW;

            for (int oh = 0; oh < outH; oh++) {
                for (int ow = 0; ow < outW; ow++) {
                    float sum = 0.0f;
                    int count = 0;

                    for (int kh = 0; kh < kH; kh++) {
                        for (int kw = 0; kw < kW; kw++) {
                            int ih = oh * sH - pH + kh;
                            int iw = ow * sW - pW + kw;
                            if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                sum += in_ch[ih * W + iw];
                                count++;
                            }
                        }
                    }

                    out_ch[oh * outW + ow] = count > 0 ? sum / (float)count : 0.0f;
                }
            }
        }
    }

    return true;
}

/*=============================================================================
 * Global AvgPool (Function 7) - average over all spatial dims (H,W) per channel
 *=============================================================================*/

bool nimcp_gpu_global_avgpool(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;

    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_global_avgpool: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    float* out_data = tensor_data_f(output);

    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int spatial = H * W;

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            const float* in_ch = in_data + (n * C + c) * spatial;
            float sum = 0.0f;
            for (int i = 0; i < spatial; i++) {
                sum += in_ch[i];
            }
            /* output shape: (N, C, 1, 1) or (N, C) */
            out_data[n * C + c] = sum / (float)spatial;
        }
    }

    return true;
}

/*=============================================================================
 * Adaptive AvgPool2D (Function 8) - map input_h/w -> output_h/w
 *=============================================================================*/

bool nimcp_gpu_adaptive_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    uint32_t output_h, uint32_t output_w)
{
    (void)ctx;

    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_adaptive_avgpool2d: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    float* out_data = tensor_data_f(output);

    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int oH = (int)output_h;
    int oW = (int)output_w;

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            const float* in_ch = in_data + (n * C + c) * H * W;
            float* out_ch = out_data + (n * C + c) * oH * oW;

            for (int oh = 0; oh < oH; oh++) {
                /* Compute adaptive window: start and end indices in input */
                int h_start = (oh * H) / oH;
                int h_end = ((oh + 1) * H) / oH;
                if (h_end > H) h_end = H;

                for (int ow = 0; ow < oW; ow++) {
                    int w_start = (ow * W) / oW;
                    int w_end = ((ow + 1) * W) / oW;
                    if (w_end > W) w_end = W;

                    float sum = 0.0f;
                    int count = 0;
                    for (int ih = h_start; ih < h_end; ih++) {
                        for (int iw = w_start; iw < w_end; iw++) {
                            sum += in_ch[ih * W + iw];
                            count++;
                        }
                    }
                    out_ch[oh * oW + ow] = count > 0 ? sum / (float)count : 0.0f;
                }
            }
        }
    }

    return true;
}

/*=============================================================================
 * BatchNorm2D Forward (Function 9)
 * y = gamma * (x - mean) / sqrt(var + eps) + beta
 * In training mode: compute batch stats and update running stats with momentum
 *=============================================================================*/

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
    (void)ctx;

    if (!input || !gamma || !beta || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_batchnorm2d_forward: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* g_data = tensor_data_f(gamma);
    const float* b_data = tensor_data_f(beta);
    float* out_data = tensor_data_f(output);

    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int spatial = H * W;
    int batch_spatial = N * spatial;

    for (int c = 0; c < C; c++) {
        float mean, var;

        if (training) {
            /* Compute batch mean and variance for this channel */
            mean = 0.0f;
            for (int n = 0; n < N; n++) {
                const float* in_ch = in_data + (n * C + c) * spatial;
                for (int i = 0; i < spatial; i++) {
                    mean += in_ch[i];
                }
            }
            mean /= (float)batch_spatial;

            var = 0.0f;
            for (int n = 0; n < N; n++) {
                const float* in_ch = in_data + (n * C + c) * spatial;
                for (int i = 0; i < spatial; i++) {
                    float diff = in_ch[i] - mean;
                    var += diff * diff;
                }
            }
            var /= (float)batch_spatial;

            /* Update running stats: running = (1 - momentum) * running + momentum * batch */
            if (running_mean && running_var) {
                float* rm = tensor_data_f(running_mean);
                float* rv = tensor_data_f(running_var);
                rm[c] = (1.0f - momentum) * rm[c] + momentum * mean;
                rv[c] = (1.0f - momentum) * rv[c] + momentum * var;
            }
        } else {
            /* Use running stats for inference */
            if (!running_mean || !running_var) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                    "nimcp_gpu_batchnorm2d_forward: running_mean/running_var NULL in eval mode");
                return false;
            }
            mean = tensor_data_f(running_mean)[c];
            var = tensor_data_f(running_var)[c];
        }

        /* Normalize: y = gamma * (x - mean) / sqrt(var + eps) + beta */
        float inv_std = 1.0f / sqrtf(var + eps);
        float g = g_data[c];
        float b = b_data[c];

        for (int n = 0; n < N; n++) {
            const float* in_ch = in_data + (n * C + c) * spatial;
            float* out_ch = out_data + (n * C + c) * spatial;
            for (int i = 0; i < spatial; i++) {
                out_ch[i] = g * (in_ch[i] - mean) * inv_std + b;
            }
        }
    }

    return true;
}

/*=============================================================================
 * LayerNorm Forward (Function 10) - tensor interface
 * Normalize over last dimension(s)
 *=============================================================================*/

bool nimcp_gpu_layernorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    (void)ctx;

    if (!input || !gamma || !beta || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_layernorm_forward: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* g_data = tensor_data_f(gamma);
    const float* b_data = tensor_data_f(beta);
    float* out_data = tensor_data_f(output);

    /* For layer norm, normalize over the last dimension.
     * Total elements / last dim size = number of normalization groups */
    size_t total = input->numel;
    int last_dim = (int)input->dims[input->ndim - 1];
    int num_groups = (int)(total / (size_t)last_dim);

    for (int g = 0; g < num_groups; g++) {
        const float* x = in_data + g * last_dim;
        float* y = out_data + g * last_dim;

        /* Compute mean */
        float mean = 0.0f;
        for (int i = 0; i < last_dim; i++) {
            mean += x[i];
        }
        mean /= (float)last_dim;

        /* Compute variance */
        float var = 0.0f;
        for (int i = 0; i < last_dim; i++) {
            float diff = x[i] - mean;
            var += diff * diff;
        }
        var /= (float)last_dim;

        /* Normalize: y = gamma * (x - mean) / sqrt(var + eps) + beta */
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int i = 0; i < last_dim; i++) {
            y[i] = g_data[i] * (x[i] - mean) * inv_std + b_data[i];
        }
    }

    return true;
}

/*=============================================================================
 * InstanceNorm Forward (Function 11) - tensor interface
 * Per-instance per-channel normalization over spatial dims
 *=============================================================================*/

bool nimcp_gpu_instancenorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps)
{
    (void)ctx;

    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_instancenorm_forward: required parameter is NULL");
        return false;
    }

    const float* in_data = tensor_data_f(input);
    const float* g_data = gamma ? tensor_data_f(gamma) : NULL;
    const float* b_data = beta ? tensor_data_f(beta) : NULL;
    float* out_data = tensor_data_f(output);

    /* input (N, C, H, W) */
    int N = (int)input->dims[0];
    int C = (int)input->dims[1];
    int H = (int)input->dims[2];
    int W = (int)input->dims[3];
    int spatial = H * W;

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            const float* in_ch = in_data + (n * C + c) * spatial;
            float* out_ch = out_data + (n * C + c) * spatial;

            /* Mean over spatial dims */
            float mean = 0.0f;
            for (int i = 0; i < spatial; i++) {
                mean += in_ch[i];
            }
            mean /= (float)spatial;

            /* Variance over spatial dims */
            float var = 0.0f;
            for (int i = 0; i < spatial; i++) {
                float diff = in_ch[i] - mean;
                var += diff * diff;
            }
            var /= (float)spatial;

            /* Normalize */
            float inv_std = 1.0f / sqrtf(var + eps);
            float g = g_data ? g_data[c] : 1.0f;
            float b = b_data ? b_data[c] : 0.0f;

            for (int i = 0; i < spatial; i++) {
                out_ch[i] = g * (in_ch[i] - mean) * inv_std + b;
            }
        }
    }

    return true;
}

/*=============================================================================
 * Conv2D Backward Context Create/Destroy (Functions 12, 13)
 *=============================================================================*/

nimcp_conv2d_backward_ctx_t* nimcp_conv2d_backward_create(
    nimcp_gpu_context_t* ctx,
    int batch_size, int in_channels, int in_height, int in_width,
    int out_channels, int kernel_h, int kernel_w,
    int stride_h, int stride_w, int pad_h, int pad_w)
{
    (void)ctx;

    nimcp_conv2d_backward_ctx_t* bwd = (nimcp_conv2d_backward_ctx_t*)nimcp_calloc(
        1, sizeof(nimcp_conv2d_backward_ctx_t));
    if (!bwd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_conv2d_backward_create: context allocation failed");
        return NULL;
    }

    bwd->batch_size = batch_size;
    bwd->in_channels = in_channels;
    bwd->in_height = in_height;
    bwd->in_width = in_width;
    bwd->out_channels = out_channels;
    bwd->kernel_h = kernel_h;
    bwd->kernel_w = kernel_w;
    bwd->stride_h = stride_h;
    bwd->stride_w = stride_w;
    bwd->pad_h = pad_h;
    bwd->pad_w = pad_w;
    bwd->ctx = ctx;

    /* Compute output dims */
    int out_h = (in_height + 2 * pad_h - kernel_h) / stride_h + 1;
    int out_w = (in_width + 2 * pad_w - kernel_w) / stride_w + 1;
    bwd->out_height = out_h;
    bwd->out_width = out_w;

    /* Allocate gradient buffers */
    size_t input_size = (size_t)batch_size * in_channels * in_height * in_width;
    size_t weight_size = (size_t)out_channels * in_channels * kernel_h * kernel_w;
    size_t bias_size = (size_t)out_channels;
    size_t col_size = (size_t)in_channels * kernel_h * kernel_w * out_h * out_w;

    bwd->d_input_grad = (float*)nimcp_calloc(input_size, sizeof(float));
    bwd->d_weight_grad = (float*)nimcp_calloc(weight_size, sizeof(float));
    bwd->d_bias_grad = (float*)nimcp_calloc(bias_size, sizeof(float));
    bwd->d_input_cache = (float*)nimcp_calloc(input_size, sizeof(float));
    bwd->d_col_buffer = (float*)nimcp_calloc(col_size, sizeof(float));

    if (!bwd->d_input_grad || !bwd->d_weight_grad || !bwd->d_bias_grad ||
        !bwd->d_input_cache || !bwd->d_col_buffer) {
        nimcp_conv2d_backward_destroy(bwd);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_conv2d_backward_create: buffer allocation failed");
        return NULL;
    }

    return bwd;
}

void nimcp_conv2d_backward_destroy(nimcp_conv2d_backward_ctx_t* bwd_ctx) {
    if (!bwd_ctx) return;

    nimcp_free(bwd_ctx->d_input_grad);
    nimcp_free(bwd_ctx->d_weight_grad);
    nimcp_free(bwd_ctx->d_bias_grad);
    nimcp_free(bwd_ctx->d_input_cache);
    nimcp_free(bwd_ctx->d_col_buffer);
    nimcp_free(bwd_ctx);
}

/*=============================================================================
 * Conv2D Backward - raw float* interface (Function 14)
 *=============================================================================*/

int nimcp_conv2d_backward(
    nimcp_conv2d_backward_ctx_t* bwd_ctx,
    const float* output_grad,
    const float* weights,
    const float* input)
{
    if (!bwd_ctx || !output_grad || !weights || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_conv2d_backward: required parameter is NULL");
        return -1;
    }

    int N = bwd_ctx->batch_size;
    int C_in = bwd_ctx->in_channels;
    int H = bwd_ctx->in_height;
    int W = bwd_ctx->in_width;
    int C_out = bwd_ctx->out_channels;
    int kH = bwd_ctx->kernel_h;
    int kW = bwd_ctx->kernel_w;
    int sH = bwd_ctx->stride_h;
    int sW = bwd_ctx->stride_w;
    int pH = bwd_ctx->pad_h;
    int pW = bwd_ctx->pad_w;
    int outH = bwd_ctx->out_height;
    int outW = bwd_ctx->out_width;

    int col_rows = C_in * kH * kW;
    int col_cols = outH * outW;

    /* Cache the input */
    memcpy(bwd_ctx->d_input_cache, input,
           (size_t)N * C_in * H * W * sizeof(float));

    /* Zero gradient buffers */
    memset(bwd_ctx->d_input_grad, 0,
           (size_t)N * C_in * H * W * sizeof(float));
    memset(bwd_ctx->d_weight_grad, 0,
           (size_t)C_out * C_in * kH * kW * sizeof(float));
    memset(bwd_ctx->d_bias_grad, 0,
           (size_t)C_out * sizeof(float));

    float* col = bwd_ctx->d_col_buffer;

    for (int n = 0; n < N; n++) {
        const float* in_n = input + n * C_in * H * W;
        const float* go_n = output_grad + n * C_out * outH * outW;

        /* im2col of input */
        for (int c = 0; c < C_in; c++) {
            for (int kh = 0; kh < kH; kh++) {
                for (int kw = 0; kw < kW; kw++) {
                    int cr = (c * kH + kh) * kW + kw;
                    for (int oh = 0; oh < outH; oh++) {
                        for (int ow = 0; ow < outW; ow++) {
                            int ih = oh * sH - pH + kh;
                            int iw = ow * sW - pW + kw;
                            int ci = cr * col_cols + oh * outW + ow;
                            if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                col[ci] = in_n[(c * H + ih) * W + iw];
                            } else {
                                col[ci] = 0.0f;
                            }
                        }
                    }
                }
            }
        }

        /* grad_weight += grad_output_reshaped @ col^T */
        for (int oc = 0; oc < C_out; oc++) {
            const float* go_row = go_n + oc * col_cols;
            float* gw_row = bwd_ctx->d_weight_grad + oc * col_rows;
            for (int k = 0; k < col_rows; k++) {
                float sum = 0.0f;
                const float* col_row_ptr = col + k * col_cols;
                for (int j = 0; j < col_cols; j++) {
                    sum += go_row[j] * col_row_ptr[j];
                }
                gw_row[k] += sum;
            }
        }

        /* grad_bias += sum of grad_output over spatial dims */
        for (int oc = 0; oc < C_out; oc++) {
            const float* go_row = go_n + oc * col_cols;
            for (int j = 0; j < col_cols; j++) {
                bwd_ctx->d_bias_grad[oc] += go_row[j];
            }
        }

        /* grad_input via col2im of (weight^T @ grad_output) */
        memset(col, 0, (size_t)col_rows * col_cols * sizeof(float));
        for (int k = 0; k < col_rows; k++) {
            float* col_row_ptr = col + k * col_cols;
            for (int oc = 0; oc < C_out; oc++) {
                float w_val = weights[oc * col_rows + k];
                const float* go_row = go_n + oc * col_cols;
                for (int j = 0; j < col_cols; j++) {
                    col_row_ptr[j] += w_val * go_row[j];
                }
            }
        }

        /* col2im: scatter-add to grad_input */
        float* gi_n = bwd_ctx->d_input_grad + n * C_in * H * W;
        for (int c = 0; c < C_in; c++) {
            for (int kh = 0; kh < kH; kh++) {
                for (int kw = 0; kw < kW; kw++) {
                    int cr = (c * kH + kh) * kW + kw;
                    for (int oh = 0; oh < outH; oh++) {
                        for (int ow = 0; ow < outW; ow++) {
                            int ih = oh * sH - pH + kh;
                            int iw = ow * sW - pW + kw;
                            if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                int ci = cr * col_cols + oh * outW + ow;
                                gi_n[(c * H + ih) * W + iw] += col[ci];
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

/*=============================================================================
 * Layer Normalization Context API (Functions 15, 16, 17, 18)
 *=============================================================================*/

nimcp_layer_norm_ctx_t* nimcp_layer_norm_create(
    nimcp_gpu_context_t* ctx,
    int normalized_shape,
    float eps)
{
    (void)ctx;

    if (normalized_shape <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_layer_norm_create: normalized_shape must be > 0");
        return NULL;
    }

    nimcp_layer_norm_ctx_t* ln = (nimcp_layer_norm_ctx_t*)nimcp_calloc(
        1, sizeof(nimcp_layer_norm_ctx_t));
    if (!ln) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_layer_norm_create: context allocation failed");
        return NULL;
    }

    ln->normalized_shape = normalized_shape;
    ln->epsilon = eps;
    ln->ctx = ctx;

    /* Allocate gamma (scale) and beta (shift), initialized to 1 and 0 */
    ln->d_gamma = (float*)nimcp_malloc((size_t)normalized_shape * sizeof(float));
    ln->d_beta = (float*)nimcp_calloc((size_t)normalized_shape, sizeof(float));

    if (!ln->d_gamma || !ln->d_beta) {
        nimcp_layer_norm_destroy(ln);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_layer_norm_create: gamma/beta allocation failed");
        return NULL;
    }

    /* Initialize gamma to 1.0 */
    for (int i = 0; i < normalized_shape; i++) {
        ln->d_gamma[i] = 1.0f;
    }

    /* d_mean and d_var are allocated during forward (depend on batch_size) */
    ln->d_mean = NULL;
    ln->d_var = NULL;

    return ln;
}

void nimcp_layer_norm_destroy(nimcp_layer_norm_ctx_t* ln_ctx) {
    if (!ln_ctx) return;

    nimcp_free(ln_ctx->d_gamma);
    nimcp_free(ln_ctx->d_beta);
    nimcp_free(ln_ctx->d_mean);
    nimcp_free(ln_ctx->d_var);
    nimcp_free(ln_ctx);
}

int nimcp_layer_norm_forward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* input,
    float* output,
    int batch_size)
{
    if (!ln_ctx || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_layer_norm_forward: required parameter is NULL");
        return -1;
    }

    int D = ln_ctx->normalized_shape;
    float eps = ln_ctx->epsilon;

    /* (Re)allocate mean/var buffers for this batch */
    nimcp_free(ln_ctx->d_mean);
    nimcp_free(ln_ctx->d_var);
    ln_ctx->d_mean = (float*)nimcp_calloc((size_t)batch_size, sizeof(float));
    ln_ctx->d_var = (float*)nimcp_calloc((size_t)batch_size, sizeof(float));

    if (!ln_ctx->d_mean || !ln_ctx->d_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_layer_norm_forward: mean/var allocation failed");
        return -1;
    }

    for (int b = 0; b < batch_size; b++) {
        const float* x = input + b * D;
        float* y = output + b * D;

        /* Compute mean */
        float mean = 0.0f;
        for (int i = 0; i < D; i++) {
            mean += x[i];
        }
        mean /= (float)D;
        ln_ctx->d_mean[b] = mean;

        /* Compute variance */
        float var = 0.0f;
        for (int i = 0; i < D; i++) {
            float diff = x[i] - mean;
            var += diff * diff;
        }
        var /= (float)D;
        ln_ctx->d_var[b] = var;

        /* Normalize: y = gamma * (x - mean) / sqrt(var + eps) + beta */
        float inv_std = 1.0f / sqrtf(var + eps);
        for (int i = 0; i < D; i++) {
            y[i] = ln_ctx->d_gamma[i] * (x[i] - mean) * inv_std + ln_ctx->d_beta[i];
        }
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
    if (!ln_ctx || !grad_output || !input || !grad_input || !grad_gamma || !grad_beta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_layer_norm_backward: required parameter is NULL");
        return -1;
    }

    if (!ln_ctx->d_mean || !ln_ctx->d_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_layer_norm_backward: must call forward first to populate mean/var");
        return -1;
    }

    int D = ln_ctx->normalized_shape;
    float eps = ln_ctx->epsilon;

    /* Zero grad_gamma and grad_beta accumulators */
    memset(grad_gamma, 0, (size_t)D * sizeof(float));
    memset(grad_beta, 0, (size_t)D * sizeof(float));

    for (int b = 0; b < batch_size; b++) {
        const float* x = input + b * D;
        const float* go = grad_output + b * D;
        float* gi = grad_input + b * D;

        float mean = ln_ctx->d_mean[b];
        float var = ln_ctx->d_var[b];
        float inv_std = 1.0f / sqrtf(var + eps);

        /* Accumulate grad_gamma and grad_beta */
        for (int i = 0; i < D; i++) {
            float x_hat = (x[i] - mean) * inv_std;
            grad_gamma[i] += go[i] * x_hat;
            grad_beta[i] += go[i];
        }

        /* Compute grad_input using the layer norm backward formula:
         * dx_hat = go * gamma
         * dvar = sum(dx_hat * (x - mean)) * (-0.5) * (var + eps)^(-1.5)
         * dmean = sum(dx_hat) * (-inv_std) + dvar * (-2/D) * sum(x - mean)
         * dx = dx_hat * inv_std + dvar * 2*(x - mean)/D + dmean/D
         */
        float sum_dx_hat = 0.0f;
        float sum_dx_hat_xhat = 0.0f;
        for (int i = 0; i < D; i++) {
            float dx_hat = go[i] * ln_ctx->d_gamma[i];
            float x_hat = (x[i] - mean) * inv_std;
            sum_dx_hat += dx_hat;
            sum_dx_hat_xhat += dx_hat * x_hat;
        }

        /* Simplified layer norm backward:
         * gi[i] = inv_std/D * (D * dx_hat[i] - sum_dx_hat - x_hat[i] * sum_dx_hat_xhat)
         */
        float inv_D = 1.0f / (float)D;
        for (int i = 0; i < D; i++) {
            float dx_hat = go[i] * ln_ctx->d_gamma[i];
            float x_hat = (x[i] - mean) * inv_std;
            gi[i] = inv_std * inv_D * ((float)D * dx_hat - sum_dx_hat - x_hat * sum_dx_hat_xhat);
        }
    }

    return 0;
}

/*=============================================================================
 * Instance Normalization Context API (Functions 19, 20, 21, 22)
 *=============================================================================*/

nimcp_instance_norm_ctx_t* nimcp_instance_norm_create(
    nimcp_gpu_context_t* ctx,
    int num_features,
    float eps,
    bool affine)
{
    (void)ctx;

    if (num_features <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_instance_norm_create: num_features must be > 0");
        return NULL;
    }

    nimcp_instance_norm_ctx_t* in_ctx = (nimcp_instance_norm_ctx_t*)nimcp_calloc(
        1, sizeof(nimcp_instance_norm_ctx_t));
    if (!in_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_instance_norm_create: context allocation failed");
        return NULL;
    }

    in_ctx->num_features = num_features;
    in_ctx->epsilon = eps;
    in_ctx->affine = affine;
    in_ctx->ctx = ctx;

    if (affine) {
        in_ctx->d_gamma = (float*)nimcp_malloc((size_t)num_features * sizeof(float));
        in_ctx->d_beta = (float*)nimcp_calloc((size_t)num_features, sizeof(float));

        if (!in_ctx->d_gamma || !in_ctx->d_beta) {
            nimcp_instance_norm_destroy(in_ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_instance_norm_create: gamma/beta allocation failed");
            return NULL;
        }

        /* Initialize gamma to 1.0 */
        for (int i = 0; i < num_features; i++) {
            in_ctx->d_gamma[i] = 1.0f;
        }
    } else {
        in_ctx->d_gamma = NULL;
        in_ctx->d_beta = NULL;
    }

    in_ctx->d_mean = NULL;
    in_ctx->d_var = NULL;

    return in_ctx;
}

void nimcp_instance_norm_destroy(nimcp_instance_norm_ctx_t* in_ctx) {
    if (!in_ctx) return;

    nimcp_free(in_ctx->d_gamma);
    nimcp_free(in_ctx->d_beta);
    nimcp_free(in_ctx->d_mean);
    nimcp_free(in_ctx->d_var);
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
    if (!in_ctx || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_instance_norm_forward: required parameter is NULL");
        return -1;
    }

    int C = in_ctx->num_features;
    float eps = in_ctx->epsilon;
    int spatial = height * width;

    /* (Re)allocate mean/var: shape (N, C) */
    int nc = batch_size * C;
    nimcp_free(in_ctx->d_mean);
    nimcp_free(in_ctx->d_var);
    in_ctx->d_mean = (float*)nimcp_calloc((size_t)nc, sizeof(float));
    in_ctx->d_var = (float*)nimcp_calloc((size_t)nc, sizeof(float));

    if (!in_ctx->d_mean || !in_ctx->d_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_instance_norm_forward: mean/var allocation failed");
        return -1;
    }

    for (int n = 0; n < batch_size; n++) {
        for (int c = 0; c < C; c++) {
            int idx = n * C + c;
            const float* x = input + (size_t)idx * spatial;
            float* y = output + (size_t)idx * spatial;

            /* Compute mean over spatial dims */
            float mean = 0.0f;
            for (int i = 0; i < spatial; i++) {
                mean += x[i];
            }
            mean /= (float)spatial;
            in_ctx->d_mean[idx] = mean;

            /* Compute variance over spatial dims */
            float var = 0.0f;
            for (int i = 0; i < spatial; i++) {
                float diff = x[i] - mean;
                var += diff * diff;
            }
            var /= (float)spatial;
            in_ctx->d_var[idx] = var;

            /* Normalize */
            float inv_std = 1.0f / sqrtf(var + eps);
            float g = (in_ctx->affine && in_ctx->d_gamma) ? in_ctx->d_gamma[c] : 1.0f;
            float b = (in_ctx->affine && in_ctx->d_beta) ? in_ctx->d_beta[c] : 0.0f;

            for (int i = 0; i < spatial; i++) {
                y[i] = g * (x[i] - mean) * inv_std + b;
            }
        }
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
    if (!in_ctx || !grad_output || !input || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_instance_norm_backward: required parameter is NULL");
        return -1;
    }

    if (!in_ctx->d_mean || !in_ctx->d_var) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_instance_norm_backward: must call forward first to populate mean/var");
        return -1;
    }

    int C = in_ctx->num_features;
    float eps = in_ctx->epsilon;
    int spatial = height * width;

    /* Zero affine gradients if requested */
    if (in_ctx->affine && grad_gamma && grad_beta) {
        memset(grad_gamma, 0, (size_t)C * sizeof(float));
        memset(grad_beta, 0, (size_t)C * sizeof(float));
    }

    for (int n = 0; n < batch_size; n++) {
        for (int c = 0; c < C; c++) {
            int idx = n * C + c;
            const float* x = input + (size_t)idx * spatial;
            const float* go = grad_output + (size_t)idx * spatial;
            float* gi = grad_input + (size_t)idx * spatial;

            float mean = in_ctx->d_mean[idx];
            float var = in_ctx->d_var[idx];
            float inv_std = 1.0f / sqrtf(var + eps);
            float g = (in_ctx->affine && in_ctx->d_gamma) ? in_ctx->d_gamma[c] : 1.0f;

            /* Accumulate affine gradients */
            if (in_ctx->affine && grad_gamma && grad_beta) {
                for (int i = 0; i < spatial; i++) {
                    float x_hat = (x[i] - mean) * inv_std;
                    grad_gamma[c] += go[i] * x_hat;
                    grad_beta[c] += go[i];
                }
            }

            /* Compute grad_input using instance norm backward formula
             * Same structure as layer norm but over spatial dims
             * gi[i] = g * inv_std / S * (S * go[i] - sum_go - x_hat[i] * sum_go_xhat)
             */
            float sum_go = 0.0f;
            float sum_go_xhat = 0.0f;
            for (int i = 0; i < spatial; i++) {
                float x_hat = (x[i] - mean) * inv_std;
                sum_go += go[i] * g;
                sum_go_xhat += go[i] * g * x_hat;
            }

            float inv_S = 1.0f / (float)spatial;
            for (int i = 0; i < spatial; i++) {
                float x_hat = (x[i] - mean) * inv_std;
                float dxhat = go[i] * g;
                gi[i] = inv_std * inv_S * ((float)spatial * dxhat - sum_go - x_hat * sum_go_xhat);
            }
        }
    }

    return 0;
}
