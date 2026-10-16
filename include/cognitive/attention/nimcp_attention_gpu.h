/**
 * @file nimcp_attention_gpu.h
 * @brief GPU-Accelerated Attention Precision Weighting API
 *
 * WHAT: C API for GPU-accelerated attention precision weighting kernels
 * WHY:  GPU acceleration for FEP-attention integration hot paths
 * HOW:  Wraps CUDA kernels for precision-weighted gain modulation
 *
 * BIOLOGICAL BASIS:
 * =================
 * Precision-weighted attention implements Feldman & Friston (2010) theory:
 * - Attention = precision optimization in predictive coding
 * - High precision -> Increased gain on prediction errors -> Stronger attention
 * - Low precision -> Reduced gain -> Attentional suppression
 *
 * INTEGRATION:
 * ============
 * This module accelerates the precision weighting operations in
 * nimcp_attention_fep_bridge.h, specifically:
 * - attention_fep_apply_precision_gain_modulation()
 * - Precision-weighted prediction error processing
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * nimcp_gpu_tensor_t* input = nimcp_gpu_tensor_from_host(ctx, ...);
 * nimcp_gpu_tensor_t* precision = nimcp_gpu_tensor_from_host(ctx, ...);
 * nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32);
 *
 * bool success = nimcp_gpu_attention_precision_weight(ctx, output, input, precision, base_gain);
 *
 * nimcp_gpu_tensor_destroy(output);
 * nimcp_gpu_tensor_destroy(precision);
 * nimcp_gpu_tensor_destroy(input);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_ATTENTION_GPU_H
#define NIMCP_ATTENTION_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
// which contain C++ code that cannot have C linkage
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Precision-Weighted Attention Gain Modulation
//=============================================================================

/**
 * @brief Apply precision-weighted attention gain modulation (GPU-accelerated)
 *
 * WHAT: Compute output[i] = input[i] * base_gain * precision[i]
 * WHY:  Core FEP-attention integration: precision modulates attention gain
 * HOW:  GPU kernel processes all elements in parallel
 *
 * This implements the core precision weighting from Feldman & Friston (2010):
 * - High precision values boost attention gain
 * - Low precision values suppress attention gain
 *
 * @param ctx GPU context (required)
 * @param output Output GPU tensor for precision-weighted signal (same shape as input)
 * @param input Input GPU tensor with signal to weight
 * @param precision GPU tensor with per-element precision values
 * @param base_gain Baseline attention gain scalar (typically 1.0-2.0)
 * @return true on success, false on failure (check logs for details)
 *
 * THREAD SAFETY: Thread-safe for independent GPU contexts
 *
 * EXAMPLE:
 * @code
 * // Apply precision weighting with 1.5x base gain
 * bool ok = nimcp_gpu_attention_precision_weight(ctx, output, input, precision, 1.5f);
 * @endcode
 */
NIMCP_EXPORT bool nimcp_gpu_attention_precision_weight(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain
);

/**
 * @brief Apply precision-weighted attention gain with output clamping
 *
 * WHAT: Compute output[i] = clamp(input[i] * base_gain * precision[i], min_val, max_val)
 * WHY:  Prevent numerical instability from extreme precision values
 * HOW:  GPU kernel with fused multiply-clamp operation
 *
 * @param ctx GPU context (required)
 * @param output Output GPU tensor for precision-weighted signal
 * @param input Input GPU tensor with signal to weight
 * @param precision GPU tensor with per-element precision values
 * @param base_gain Baseline attention gain scalar
 * @param min_val Minimum output value (prevents underflow)
 * @param max_val Maximum output value (prevents overflow)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_gpu_attention_precision_weight_clamped(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain,
    float min_val,
    float max_val
);

/**
 * @brief Apply adaptive precision-weighted attention gain
 *
 * WHAT: Compute output[i] = input[i] * (1 + (base_gain - 1) * sensitivity) * precision[i]
 * WHY:  Supports learnable precision sensitivity from Attention-FEP bridge config
 * HOW:  GPU kernel with adaptive gain blending
 *
 * This matches the attention_fep_config_t precision_sensitivity parameter:
 * - sensitivity=1.0: Full precision effect (base_gain applied directly)
 * - sensitivity=0.0: No precision effect (gain fixed at 1.0)
 * - sensitivity=0.5: Half precision effect (blended gain)
 *
 * @param ctx GPU context (required)
 * @param output Output GPU tensor for precision-weighted signal
 * @param input Input GPU tensor with signal to weight
 * @param precision GPU tensor with per-element precision values
 * @param base_gain Baseline attention gain (e.g., 1.5 for high, 0.5 for low)
 * @param precision_sensitivity Precision effect scaling factor [0.0-1.0]
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_gpu_attention_precision_weight_adaptive(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain,
    float precision_sensitivity
);

/**
 * @brief Apply batched precision-weighted attention gain for multi-head attention
 *
 * WHAT: Apply precision weighting to multiple attention heads in parallel
 * WHY:  Multi-head attention requires processing multiple heads efficiently
 * HOW:  Single kernel launch processes all heads
 *
 * @param ctx GPU context (required)
 * @param output Output GPU tensor [batch_size x head_dim]
 * @param input Input GPU tensor [batch_size x head_dim]
 * @param precision GPU tensor for precision values
 *                  - If shared_precision=true: [head_dim] (shared across all heads)
 *                  - If shared_precision=false: [batch_size x head_dim] (per-head)
 * @param base_gains GPU tensor with per-head base gain values [batch_size]
 * @param batch_size Number of attention heads
 * @param head_dim Dimension of each head
 * @param shared_precision If true, precision is shared across all heads
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * @code
 * // Process 8 attention heads of dimension 64 with shared precision
 * bool ok = nimcp_gpu_attention_precision_weight_batched(
 *     ctx, output, input, precision, base_gains,
 *     8, 64, true
 * );
 * @endcode
 */
NIMCP_EXPORT bool nimcp_gpu_attention_precision_weight_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    const nimcp_gpu_tensor_t* base_gains,
    uint32_t batch_size,
    uint32_t head_dim,
    bool shared_precision
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_GPU_H */
