//=============================================================================
// nimcp_inference_gpu.h - GPU Inference Kernels (Optimized Forward Pass)
//=============================================================================
/**
 * @file nimcp_inference_gpu.h
 * @brief GPU-accelerated inference operations using CUDA
 *
 * WHAT: CUDA kernels for optimized neural network inference
 * WHY:  Enables high-throughput, low-latency inference on GPU
 * HOW:  Fused operations, quantized compute, batched processing
 *
 * ARCHITECTURE:
 * - Fused operations: Linear+ReLU, Conv+BN+ReLU for reduced memory bandwidth
 * - Quantization: INT8 and FP16 for faster compute and lower memory
 * - Batched inference: Process multiple inputs simultaneously
 * - Memory optimization: Persistent kernels, graph capture
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_INFERENCE_GPU_H
#define NIMCP_INFERENCE_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Precision Types
//=============================================================================

typedef enum {
    NIMCP_INFER_FP32 = 0,     /**< Full precision (default) */
    NIMCP_INFER_FP16 = 1,     /**< Half precision (Tensor Cores) */
    NIMCP_INFER_BF16 = 2,     /**< Brain float 16 */
    NIMCP_INFER_INT8 = 3,     /**< Quantized INT8 */
    NIMCP_INFER_INT4 = 4,     /**< Quantized INT4 (weight only) */
    NIMCP_INFER_TF32 = 5      /**< Tensor Float 32 (Ampere+) */
} nimcp_infer_precision_t;

//=============================================================================
// Quantization Configuration
//=============================================================================

/**
 * @brief Per-tensor quantization parameters
 */
typedef struct {
    float scale;              /**< Quantization scale: real = scale * (quant - zero_point) */
    int32_t zero_point;       /**< Zero point offset */
    float min_val;            /**< Minimum representable value */
    float max_val;            /**< Maximum representable value */
} nimcp_quant_params_t;

/**
 * @brief Calibration statistics for quantization
 */
typedef struct {
    float* min_vals;          /**< Per-channel minimums [n_channels] */
    float* max_vals;          /**< Per-channel maximums [n_channels] */
    float* histogram;         /**< Value histogram for optimal scale */
    uint32_t n_channels;      /**< Number of channels */
    uint32_t n_bins;          /**< Histogram bins */
    bool per_channel;         /**< Use per-channel quantization */
} nimcp_calibration_t;

//=============================================================================
// Fused Layer Operations
//=============================================================================

/**
 * @brief Fused Linear + ReLU: y = ReLU(x @ W^T + b)
 *
 * WHAT: Combined matrix multiply, bias add, and ReLU activation
 * WHY:  Reduces memory bandwidth by avoiding intermediate storage
 * HOW:  Single kernel with fused epilogue
 *
 * @param ctx GPU context
 * @param input Input tensor [batch, in_features]
 * @param weights Weight tensor [out_features, in_features]
 * @param bias Bias tensor [out_features] (can be NULL)
 * @param output Output tensor [batch, out_features]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_linear_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Fused Linear + GELU: y = GELU(x @ W^T + b)
 */
NIMCP_EXPORT bool nimcp_gpu_infer_linear_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Fused Linear + SiLU/Swish: y = SiLU(x @ W^T + b)
 */
NIMCP_EXPORT bool nimcp_gpu_infer_linear_silu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Fused Conv2D + BatchNorm + ReLU
 *
 * WHAT: Combined convolution, batch normalization, and ReLU
 * WHY:  BatchNorm can be folded into convolution weights
 * HOW:  Precompute fused weights/bias, single kernel execution
 *
 * @param ctx GPU context
 * @param input Input tensor [N, C, H, W]
 * @param weights Convolution weights [out_ch, in_ch, kH, kW]
 * @param bn_gamma BatchNorm gamma [out_ch]
 * @param bn_beta BatchNorm beta [out_ch]
 * @param bn_mean BatchNorm running mean [out_ch]
 * @param bn_var BatchNorm running var [out_ch]
 * @param output Output tensor [N, out_ch, oH, oW]
 * @param stride Convolution stride
 * @param padding Convolution padding
 * @param eps BatchNorm epsilon
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_conv_bn_relu(
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
    float eps
);

/**
 * @brief Fused Attention (Q, K, V projection + attention + output projection)
 *
 * WHAT: Complete self-attention in one operation
 * WHY:  Flash Attention-style memory optimization
 * HOW:  Tiled computation to fit in shared memory
 *
 * @param ctx GPU context
 * @param input Input tensor [batch, seq_len, embed_dim]
 * @param W_qkv Packed QKV projection weights [3, n_heads, embed_dim, head_dim]
 * @param W_o Output projection weights [embed_dim, embed_dim]
 * @param output Output tensor [batch, seq_len, embed_dim]
 * @param mask Attention mask (optional, NULL for no mask)
 * @param n_heads Number of attention heads
 * @param scale Attention scale (usually 1/sqrt(head_dim))
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_attention_fused(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* W_qkv,
    const nimcp_gpu_tensor_t* W_o,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* mask,
    uint32_t n_heads,
    float scale
);

//=============================================================================
// Quantization Operations
//=============================================================================

/**
 * @brief Quantize FP32 tensor to INT8
 *
 * @param ctx GPU context
 * @param input FP32 input tensor
 * @param output INT8 output tensor
 * @param params Quantization parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_quantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_quant_params_t* params
);

/**
 * @brief Dequantize INT8 tensor to FP32
 *
 * @param ctx GPU context
 * @param input INT8 input tensor
 * @param output FP32 output tensor
 * @param params Quantization parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_dequantize_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    const nimcp_quant_params_t* params
);

/**
 * @brief INT8 quantized matrix multiplication
 *
 * Uses tensor core INT8 GEMM for maximum throughput
 * Result is dequantized to FP32
 *
 * @param ctx GPU context
 * @param a INT8 input A [M, K]
 * @param b INT8 input B [K, N]
 * @param c FP32 output C [M, N]
 * @param params_a Quantization params for A
 * @param params_b Quantization params for B
 * @param params_c Quantization params for C (for rescaling)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_gemm_int8(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* c,
    const nimcp_quant_params_t* params_a,
    const nimcp_quant_params_t* params_b,
    const nimcp_quant_params_t* params_c
);

/**
 * @brief FP16 half-precision matrix multiplication
 *
 * Uses tensor cores for FP16 computation
 *
 * @param ctx GPU context
 * @param a FP16 input A
 * @param b FP16 input B
 * @param c FP16/FP32 output C
 * @param accumulate_fp32 Accumulate in FP32 for precision
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_gemm_fp16(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* c,
    bool accumulate_fp32
);

/**
 * @brief Calculate quantization parameters from tensor statistics
 *
 * @param ctx GPU context
 * @param tensor Input FP32 tensor to analyze
 * @param params Output: calculated quantization parameters
 * @param symmetric Use symmetric quantization (zero_point = 0)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_calibrate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* tensor,
    nimcp_quant_params_t* params,
    bool symmetric
);

//=============================================================================
// Batched Inference
//=============================================================================

/**
 * @brief Batched linear layer inference
 *
 * Process multiple samples in parallel for throughput
 *
 * @param ctx GPU context
 * @param inputs Array of input tensors
 * @param outputs Array of output tensors
 * @param weights Shared weight tensor
 * @param bias Shared bias tensor (can be NULL)
 * @param n_batches Number of batches to process
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_batch_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t** inputs,
    nimcp_gpu_tensor_t** outputs,
    const nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* bias,
    size_t n_batches
);

/**
 * @brief Dynamic batching with varying sequence lengths
 *
 * @param ctx GPU context
 * @param inputs Packed input tensor
 * @param lengths Sequence length for each sample [n_samples]
 * @param outputs Packed output tensor
 * @param weights Layer weights
 * @param n_samples Number of samples
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_dynamic_batch(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const uint32_t* lengths,
    nimcp_gpu_tensor_t* outputs,
    const nimcp_gpu_tensor_t* weights,
    size_t n_samples
);

//=============================================================================
// Memory-Optimized Operations
//=============================================================================

/**
 * @brief In-place activation (no additional memory allocation)
 *
 * @param ctx GPU context
 * @param tensor Tensor to apply activation in-place
 * @param activation Activation type: 0=ReLU, 1=Sigmoid, 2=Tanh, 3=GELU, 4=SiLU
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_activation_inplace(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    int activation
);

/**
 * @brief Residual add with optional scaling: y = alpha * x + beta * residual
 *
 * @param ctx GPU context
 * @param x Input tensor
 * @param residual Residual tensor
 * @param y Output tensor (can be same as x for in-place)
 * @param alpha Scale for x
 * @param beta Scale for residual
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_residual_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* residual,
    nimcp_gpu_tensor_t* y,
    float alpha,
    float beta
);

/**
 * @brief Layer normalization optimized for inference
 *
 * @param ctx GPU context
 * @param input Input tensor
 * @param gamma Scale parameter
 * @param beta Bias parameter
 * @param output Output tensor
 * @param eps Epsilon for numerical stability
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* output,
    float eps
);

/**
 * @brief RMS normalization (used in LLaMA-style models)
 *
 * @param ctx GPU context
 * @param input Input tensor
 * @param gamma Scale parameter
 * @param output Output tensor
 * @param eps Epsilon
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_rmsnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* gamma,
    nimcp_gpu_tensor_t* output,
    float eps
);

//=============================================================================
// Inference Session Management
//=============================================================================

/**
 * @brief Inference session for cached state
 */
typedef struct {
    nimcp_gpu_context_t* ctx;           /**< GPU context */
    nimcp_infer_precision_t precision;  /**< Active precision */
    void* cuda_graph;                   /**< Captured CUDA graph (optional) */
    void* workspace;                    /**< Preallocated workspace */
    size_t workspace_size;              /**< Workspace size in bytes */
    bool graph_captured;                /**< Whether graph is captured */
} nimcp_infer_session_t;

/**
 * @brief Create inference session
 *
 * @param ctx GPU context
 * @param precision Desired precision
 * @param workspace_size Workspace size (0 for auto)
 * @return Session or NULL on failure
 */
NIMCP_EXPORT nimcp_infer_session_t* nimcp_infer_session_create(
    nimcp_gpu_context_t* ctx,
    nimcp_infer_precision_t precision,
    size_t workspace_size
);

/**
 * @brief Destroy inference session
 */
NIMCP_EXPORT void nimcp_infer_session_destroy(nimcp_infer_session_t* session);

/**
 * @brief Begin CUDA graph capture for the session
 *
 * After capture, replaying the graph avoids kernel launch overhead
 */
NIMCP_EXPORT bool nimcp_infer_session_begin_capture(nimcp_infer_session_t* session);

/**
 * @brief End CUDA graph capture and compile
 */
NIMCP_EXPORT bool nimcp_infer_session_end_capture(nimcp_infer_session_t* session);

/**
 * @brief Execute captured CUDA graph
 */
NIMCP_EXPORT bool nimcp_infer_session_replay(nimcp_infer_session_t* session);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert tensor to different precision
 *
 * @param ctx GPU context
 * @param input Source tensor
 * @param output Destination tensor
 * @param target_precision Target precision
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_convert_precision(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output,
    nimcp_infer_precision_t target_precision
);

/**
 * @brief Get recommended precision for current GPU
 *
 * @param ctx GPU context
 * @return Recommended precision based on GPU capabilities
 */
NIMCP_EXPORT nimcp_infer_precision_t nimcp_gpu_infer_recommended_precision(
    nimcp_gpu_context_t* ctx
);

/**
 * @brief Warmup inference kernels
 *
 * Run dummy inference to JIT-compile and cache kernels
 *
 * @param ctx GPU context
 * @param precision Target precision
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_infer_warmup(
    nimcp_gpu_context_t* ctx,
    nimcp_infer_precision_t precision
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_INFERENCE_GPU_H
