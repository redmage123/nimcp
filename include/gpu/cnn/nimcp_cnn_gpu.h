//=============================================================================
// nimcp_cnn_gpu.h - GPU CNN Kernels (Convolution, Pooling, Normalization)
//=============================================================================
/**
 * @file nimcp_cnn_gpu.h
 * @brief GPU-accelerated CNN operations using CUDA
 *
 * WHAT: CUDA kernels for convolutional neural networks
 * WHY:  Enables GPU acceleration for visual, audio, and speech processing
 * HOW:  Custom kernels for conv, pooling, normalization layers
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_CNN_GPU_H
#define NIMCP_CNN_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_gpu_context_s nimcp_gpu_context_t;

//=============================================================================
// Convolution Types
//=============================================================================

typedef enum {
    NIMCP_PAD_VALID = 0,    /**< No padding */
    NIMCP_PAD_SAME = 1,     /**< Pad to maintain size */
    NIMCP_PAD_CAUSAL = 2    /**< Causal padding (for temporal) */
} nimcp_padding_mode_t;

typedef struct {
    uint32_t kernel_h, kernel_w;  /**< Kernel size */
    uint32_t stride_h, stride_w;  /**< Stride */
    uint32_t pad_h, pad_w;        /**< Padding */
    uint32_t dilation_h, dilation_w;  /**< Dilation */
    uint32_t groups;              /**< Groups for grouped conv */
} nimcp_conv_params_t;

typedef struct {
    uint32_t kernel_h, kernel_w;  /**< Pool size */
    uint32_t stride_h, stride_w;  /**< Stride */
    uint32_t pad_h, pad_w;        /**< Padding */
} nimcp_pool_params_t;

//=============================================================================
// Conv2D Backward Context
//=============================================================================

/**
 * @brief Context for conv2d backward pass with cached data
 *
 * WHAT: Holds gradients and cached forward pass data for backprop
 * WHY:  im2col approach requires cached column buffer for efficiency
 * HOW:  Stores intermediate buffers to avoid recomputation
 */
typedef struct nimcp_conv2d_backward_ctx_s {
    float* d_input_grad;      /**< Gradient w.r.t. input (device memory) */
    float* d_weight_grad;     /**< Gradient w.r.t. weights (device memory) */
    float* d_bias_grad;       /**< Gradient w.r.t. bias (device memory) */

    float* d_input_cache;     /**< Cached forward pass input (device memory) */
    float* d_col_buffer;      /**< im2col buffer (device memory) */

    int batch_size;           /**< Batch size */
    int in_channels;          /**< Number of input channels */
    int in_height;            /**< Input height */
    int in_width;             /**< Input width */
    int out_channels;         /**< Number of output channels */
    int out_height;           /**< Output height */
    int out_width;            /**< Output width */
    int kernel_h;             /**< Kernel height */
    int kernel_w;             /**< Kernel width */
    int stride_h;             /**< Stride height */
    int stride_w;             /**< Stride width */
    int pad_h;                /**< Padding height */
    int pad_w;                /**< Padding width */

    nimcp_gpu_context_t* ctx; /**< GPU context */
} nimcp_conv2d_backward_ctx_t;

//=============================================================================
// Layer Normalization Context
//=============================================================================

/**
 * @brief Context for layer normalization operations
 *
 * WHAT: Normalizes activations across the last dimension
 * WHY:  Used in transformers and RNNs for training stability
 * HOW:  Computes mean/var per sample, normalizes, applies affine transform
 */
typedef struct nimcp_layer_norm_ctx_s {
    float* d_gamma;           /**< Scale parameter (device memory) */
    float* d_beta;            /**< Shift parameter (device memory) */
    float* d_mean;            /**< Cached mean per sample (device memory) */
    float* d_var;             /**< Cached variance per sample (device memory) */
    int normalized_shape;     /**< Size of normalized dimension */
    float epsilon;            /**< Numerical stability term */
    nimcp_gpu_context_t* ctx; /**< GPU context */
} nimcp_layer_norm_ctx_t;

//=============================================================================
// Instance Normalization Context
//=============================================================================

/**
 * @brief Context for instance normalization operations
 *
 * WHAT: Normalizes activations per instance per channel
 * WHY:  Used in style transfer and image generation
 * HOW:  Computes mean/var per (batch, channel), normalizes, applies affine
 */
typedef struct nimcp_instance_norm_ctx_s {
    float* d_gamma;           /**< Scale parameter per channel (device memory) */
    float* d_beta;            /**< Shift parameter per channel (device memory) */
    float* d_mean;            /**< Cached mean (N x C) (device memory) */
    float* d_var;             /**< Cached variance (N x C) (device memory) */
    int num_features;         /**< Number of channels */
    float epsilon;            /**< Numerical stability term */
    bool affine;              /**< Apply learnable affine transform */
    nimcp_gpu_context_t* ctx; /**< GPU context */
} nimcp_instance_norm_ctx_t;

//=============================================================================
// 2D Convolution (Visual Processing)
//=============================================================================

NIMCP_EXPORT bool nimcp_gpu_conv2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,   // (N, C_in, H, W)
    const nimcp_gpu_tensor_t* weight,  // (C_out, C_in/groups, kH, kW)
    const nimcp_gpu_tensor_t* bias,    // (C_out) or NULL
    nimcp_gpu_tensor_t* output,        // (N, C_out, H_out, W_out)
    const nimcp_conv_params_t* params
);

NIMCP_EXPORT bool nimcp_gpu_conv2d_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_bias,
    const nimcp_conv_params_t* params
);

//=============================================================================
// 1D Convolution (Audio/Speech Processing)
//=============================================================================

NIMCP_EXPORT bool nimcp_gpu_conv1d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,   // (N, C_in, L)
    const nimcp_gpu_tensor_t* weight,  // (C_out, C_in, K)
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    uint32_t kernel_size, uint32_t stride, uint32_t padding, uint32_t dilation
);

//=============================================================================
// Depthwise Separable Convolution
//=============================================================================

NIMCP_EXPORT bool nimcp_gpu_depthwise_conv2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weight,  // (C, 1, kH, kW)
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output,
    const nimcp_conv_params_t* params
);

//=============================================================================
// Pooling Operations
//=============================================================================

NIMCP_EXPORT bool nimcp_gpu_maxpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* indices,  // For backward pass
    const nimcp_pool_params_t* params
);

NIMCP_EXPORT bool nimcp_gpu_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_pool_params_t* params
);

NIMCP_EXPORT bool nimcp_gpu_global_avgpool(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

NIMCP_EXPORT bool nimcp_gpu_adaptive_avgpool2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    uint32_t output_h, uint32_t output_w
);

//=============================================================================
// Normalization
//=============================================================================

NIMCP_EXPORT bool nimcp_gpu_batchnorm2d_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    nimcp_gpu_tensor_t* running_mean,
    nimcp_gpu_tensor_t* running_var,
    float momentum, float eps, bool training
);

NIMCP_EXPORT bool nimcp_gpu_layernorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps
);

NIMCP_EXPORT bool nimcp_gpu_instancenorm_forward(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps
);

//=============================================================================
// Conv2D Backward Pass API
//=============================================================================

/**
 * @brief Create conv2d backward context
 *
 * @param ctx GPU context
 * @param batch_size Batch size
 * @param in_channels Number of input channels
 * @param in_height Input height
 * @param in_width Input width
 * @param out_channels Number of output channels
 * @param kernel_h Kernel height
 * @param kernel_w Kernel width
 * @param stride_h Stride height
 * @param stride_w Stride width
 * @param pad_h Padding height
 * @param pad_w Padding width
 * @return Backward context or NULL on failure
 */
NIMCP_EXPORT nimcp_conv2d_backward_ctx_t* nimcp_conv2d_backward_create(
    nimcp_gpu_context_t* ctx,
    int batch_size, int in_channels, int in_height, int in_width,
    int out_channels, int kernel_h, int kernel_w,
    int stride_h, int stride_w, int pad_h, int pad_w
);

/**
 * @brief Destroy conv2d backward context
 *
 * @param bwd_ctx Context to destroy
 */
NIMCP_EXPORT void nimcp_conv2d_backward_destroy(nimcp_conv2d_backward_ctx_t* bwd_ctx);

/**
 * @brief Perform conv2d backward pass
 *
 * @param bwd_ctx Backward context with pre-allocated buffers
 * @param output_grad Gradient from next layer (N, C_out, H_out, W_out)
 * @param weights Filter weights (C_out, C_in, kH, kW)
 * @param input Original input (N, C_in, H_in, W_in)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_conv2d_backward(
    nimcp_conv2d_backward_ctx_t* bwd_ctx,
    const float* output_grad,
    const float* weights,
    const float* input
);

//=============================================================================
// Layer Normalization API
//=============================================================================

/**
 * @brief Create layer normalization context
 *
 * @param ctx GPU context
 * @param normalized_shape Size of normalized dimension
 * @param eps Epsilon for numerical stability
 * @return Layer norm context or NULL on failure
 */
NIMCP_EXPORT nimcp_layer_norm_ctx_t* nimcp_layer_norm_create(
    nimcp_gpu_context_t* ctx,
    int normalized_shape,
    float eps
);

/**
 * @brief Destroy layer normalization context
 *
 * @param ln_ctx Context to destroy
 */
NIMCP_EXPORT void nimcp_layer_norm_destroy(nimcp_layer_norm_ctx_t* ln_ctx);

/**
 * @brief Layer normalization forward pass
 *
 * @param ln_ctx Layer norm context
 * @param input Input tensor data (batch_size, normalized_shape)
 * @param output Output tensor data (same shape as input)
 * @param batch_size Batch size
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_layer_norm_forward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* input,
    float* output,
    int batch_size
);

/**
 * @brief Layer normalization backward pass
 *
 * @param ln_ctx Layer norm context
 * @param grad_output Gradient from next layer
 * @param input Original input
 * @param grad_input Gradient w.r.t. input (output)
 * @param grad_gamma Gradient w.r.t. gamma (output)
 * @param grad_beta Gradient w.r.t. beta (output)
 * @param batch_size Batch size
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_layer_norm_backward(
    nimcp_layer_norm_ctx_t* ln_ctx,
    const float* grad_output,
    const float* input,
    float* grad_input,
    float* grad_gamma,
    float* grad_beta,
    int batch_size
);

//=============================================================================
// Instance Normalization API
//=============================================================================

/**
 * @brief Create instance normalization context
 *
 * @param ctx GPU context
 * @param num_features Number of channels
 * @param eps Epsilon for numerical stability
 * @param affine Whether to apply learnable affine transform
 * @return Instance norm context or NULL on failure
 */
NIMCP_EXPORT nimcp_instance_norm_ctx_t* nimcp_instance_norm_create(
    nimcp_gpu_context_t* ctx,
    int num_features,
    float eps,
    bool affine
);

/**
 * @brief Destroy instance normalization context
 *
 * @param in_ctx Context to destroy
 */
NIMCP_EXPORT void nimcp_instance_norm_destroy(nimcp_instance_norm_ctx_t* in_ctx);

/**
 * @brief Instance normalization forward pass
 *
 * @param in_ctx Instance norm context
 * @param input Input tensor data (N, C, H, W)
 * @param output Output tensor data (same shape)
 * @param batch_size Batch size
 * @param height Spatial height
 * @param width Spatial width
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_instance_norm_forward(
    nimcp_instance_norm_ctx_t* in_ctx,
    const float* input,
    float* output,
    int batch_size,
    int height,
    int width
);

/**
 * @brief Instance normalization backward pass
 *
 * @param in_ctx Instance norm context
 * @param grad_output Gradient from next layer
 * @param input Original input
 * @param grad_input Gradient w.r.t. input (output)
 * @param grad_gamma Gradient w.r.t. gamma (output, can be NULL if !affine)
 * @param grad_beta Gradient w.r.t. beta (output, can be NULL if !affine)
 * @param batch_size Batch size
 * @param height Spatial height
 * @param width Spatial width
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_instance_norm_backward(
    nimcp_instance_norm_ctx_t* in_ctx,
    const float* grad_output,
    const float* input,
    float* grad_input,
    float* grad_gamma,
    float* grad_beta,
    int batch_size,
    int height,
    int width
);

//=============================================================================
// im2col/col2im Utility Functions
//=============================================================================

/**
 * @brief Transform input to column format for efficient convolution
 *
 * @param ctx GPU context
 * @param input Input tensor data (C, H, W)
 * @param col Column buffer output (C*kH*kW, outH*outW)
 * @param C Number of channels
 * @param H Input height
 * @param W Input width
 * @param kH Kernel height
 * @param kW Kernel width
 * @param sH Stride height
 * @param sW Stride width
 * @param pH Padding height
 * @param pW Padding width
 * @param outH Output height
 * @param outW Output width
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_im2col(
    nimcp_gpu_context_t* ctx,
    const float* input, float* col,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW
);

/**
 * @brief Transform column format back to image format
 *
 * @param ctx GPU context
 * @param col Column buffer input
 * @param input_grad Gradient w.r.t. input (output)
 * @param C Number of channels
 * @param H Input height
 * @param W Input width
 * @param kH Kernel height
 * @param kW Kernel width
 * @param sH Stride height
 * @param sW Stride width
 * @param pH Padding height
 * @param pW Padding width
 * @param outH Output height
 * @param outW Output width
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_col2im(
    nimcp_gpu_context_t* ctx,
    const float* col, float* input_grad,
    int C, int H, int W,
    int kH, int kW,
    int sH, int sW,
    int pH, int pW,
    int outH, int outW
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CNN_GPU_H
