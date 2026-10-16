//=============================================================================
// nimcp_ternary_tensor.h - Ternary-Tensor Integration
//=============================================================================
/**
 * @file nimcp_ternary_tensor.h
 * @brief Bridge between ternary logic and NIMCP tensor library
 *
 * WHAT: Bidirectional conversion between ternary types and nimcp_tensor_t
 * WHY:  Enable ternary representations to leverage tensor calculus
 * HOW:  Type-safe conversions with threshold quantization
 *
 * INTEGRATION POINTS:
 * - Convert tensors to ternary vectors/matrices (quantization)
 * - Convert ternary types to tensors (expansion)
 * - Ternary operations on tensor data in-place
 * - Gradient-aware ternary quantization for training
 *
 * USE CASES:
 * - Quantize SNN weight tensors to ternary (20x compression)
 * - Apply ternary logic gates to tensor masks
 * - Ternary attention gates on feature tensors
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_TENSOR_H
#define NIMCP_TERNARY_TENSOR_H

#include "nimcp_ternary_types.h"
#include "nimcp_ternary_vector.h"
#include "nimcp_ternary_matrix.h"
#include "nimcp_ternary_convert.h"
#include "../tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Tensor to Ternary Conversion
//=============================================================================

/**
 * @brief Quantize 1D tensor to ternary vector
 *
 * WHAT: Convert float/double tensor to ternary vector
 * WHY:  Compress continuous weights to ternary
 * HOW:  Apply threshold quantization element-wise
 *
 * @param tensor Input tensor (must be 1D or will flatten)
 * @param threshold Quantization threshold (values in [-t,t] become 0)
 * @param pack_mode Output packing mode
 * @return Ternary vector, or NULL on failure
 */
trit_vector_t* trit_vector_from_tensor(
    const nimcp_tensor_t* tensor,
    float threshold,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Quantize 2D tensor to ternary matrix
 *
 * WHAT: Convert float/double tensor to ternary matrix
 * WHY:  Compress weight matrices to ternary
 * HOW:  Apply threshold quantization maintaining shape
 *
 * @param tensor Input tensor (must be 2D)
 * @param threshold Quantization threshold
 * @param pack_mode Output packing mode
 * @return Ternary matrix, or NULL on failure
 */
trit_matrix_t* trit_matrix_from_tensor(
    const nimcp_tensor_t* tensor,
    float threshold,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Quantize N-D tensor to flat ternary vector
 *
 * WHAT: Convert any tensor to flat ternary representation
 * WHY:  Unified ternary storage for arbitrary tensors
 * HOW:  Flatten then quantize
 *
 * @param tensor Input tensor (any rank)
 * @param threshold Quantization threshold
 * @param pack_mode Output packing mode
 * @return Flattened ternary vector, or NULL on failure
 */
trit_vector_t* trit_vector_from_tensor_flat(
    const nimcp_tensor_t* tensor,
    float threshold,
    ternary_pack_mode_t pack_mode
);

//=============================================================================
// Ternary to Tensor Conversion
//=============================================================================

/**
 * @brief Expand ternary vector to 1D tensor
 *
 * WHAT: Convert ternary vector to float tensor
 * WHY:  Interface with tensor operations
 * HOW:  Map -1→-scale, 0→0, +1→+scale
 *
 * @param vec Input ternary vector
 * @param scale Scale factor for non-zero values
 * @param dtype Output data type (F32 or F64)
 * @return Tensor, or NULL on failure
 */
nimcp_tensor_t* trit_vector_to_tensor(
    const trit_vector_t* vec,
    float scale,
    nimcp_dtype_t dtype
);

/**
 * @brief Expand ternary matrix to 2D tensor
 *
 * WHAT: Convert ternary matrix to float tensor
 * WHY:  Interface with tensor operations
 * HOW:  Map ternary to scaled float values
 *
 * @param mat Input ternary matrix
 * @param scale Scale factor for non-zero values
 * @param dtype Output data type (F32 or F64)
 * @return Tensor, or NULL on failure
 */
nimcp_tensor_t* trit_matrix_to_tensor(
    const trit_matrix_t* mat,
    float scale,
    nimcp_dtype_t dtype
);

/**
 * @brief Expand ternary vector to N-D tensor
 *
 * WHAT: Convert flat ternary to shaped tensor
 * WHY:  Reconstruct tensor from quantized storage
 * HOW:  Expand then reshape
 *
 * @param vec Input ternary vector
 * @param dims Target dimensions
 * @param rank Number of dimensions
 * @param scale Scale factor
 * @param dtype Output data type
 * @return Reshaped tensor, or NULL on failure
 */
nimcp_tensor_t* trit_vector_to_tensor_shaped(
    const trit_vector_t* vec,
    const uint32_t* dims,
    uint32_t rank,
    float scale,
    nimcp_dtype_t dtype
);

//=============================================================================
// Adaptive Quantization
//=============================================================================

/**
 * @brief Quantization statistics for adaptive thresholding
 */
typedef struct {
    float mean;                     /**< Mean of input values */
    float std;                      /**< Standard deviation */
    float suggested_threshold;      /**< Suggested threshold (e.g., 0.5*std) */
    size_t n_positive;              /**< Count of values that would be +1 */
    size_t n_unknown;               /**< Count of values that would be 0 */
    size_t n_negative;              /**< Count of values that would be -1 */
    float sparsity;                 /**< Fraction of zeros */
    float compression_ratio;        /**< Memory compression ratio */
} trit_quantization_stats_t;

/**
 * @brief Analyze tensor for optimal ternary quantization
 *
 * WHAT: Compute statistics for choosing quantization threshold
 * WHY:  Data-dependent threshold gives better quantization
 * HOW:  Compute mean, std, distribution of quantized values
 *
 * @param tensor Input tensor
 * @param trial_threshold Threshold to analyze (or 0 for auto)
 * @param stats Output statistics
 * @return TERNARY_OK on success
 */
ternary_error_t trit_analyze_tensor(
    const nimcp_tensor_t* tensor,
    float trial_threshold,
    trit_quantization_stats_t* stats
);

/**
 * @brief Quantize tensor with adaptive threshold
 *
 * WHAT: Automatically choose threshold from data
 * WHY:  Optimal quantization without manual tuning
 * HOW:  Use std-based threshold (e.g., 0.5 * std)
 *
 * @param tensor Input tensor
 * @param target_sparsity Target fraction of zeros (0.0-1.0)
 * @param pack_mode Output packing mode
 * @param actual_stats Output actual quantization statistics (optional)
 * @return Quantized ternary vector, or NULL on failure
 */
trit_vector_t* trit_quantize_adaptive(
    const nimcp_tensor_t* tensor,
    float target_sparsity,
    ternary_pack_mode_t pack_mode,
    trit_quantization_stats_t* actual_stats
);

//=============================================================================
// Ternary Tensor Operations
//=============================================================================

/**
 * @brief Apply ternary mask to tensor (element-wise)
 *
 * WHAT: Modulate tensor by ternary mask
 * WHY:  Ternary gating for attention, routing
 * HOW:  tensor[i] *= trit[i] (with trit as -1, 0, +1)
 *
 * @param tensor Input/output tensor (modified in place)
 * @param mask Ternary mask (same numel as tensor)
 * @return TERNARY_OK on success
 */
ternary_error_t trit_mask_tensor(
    nimcp_tensor_t* tensor,
    const trit_vector_t* mask
);

/**
 * @brief Apply ternary gate to tensor (selective pass/block/invert)
 *
 * WHAT: Three-way routing gate
 * WHY:  Attention-like gating with explicit block state
 * HOW:  +1: pass through, 0: zero out, -1: negate
 *
 * @param tensor Input tensor
 * @param gate Ternary gate values
 * @return Gated tensor (new allocation), or NULL on failure
 */
nimcp_tensor_t* trit_gate_tensor(
    const nimcp_tensor_t* tensor,
    const trit_vector_t* gate
);

/**
 * @brief Ternary matrix-tensor multiply
 *
 * WHAT: Multiply ternary weight matrix with tensor
 * WHY:  Efficient inference with quantized weights
 * HOW:  Use ternary values as {-1, 0, +1} weights
 *
 * @param weights Ternary weight matrix (m x n)
 * @param input Input tensor (n elements)
 * @param weight_scale Scale factor for ternary weights
 * @return Output tensor (m elements), or NULL on failure
 */
nimcp_tensor_t* trit_matmul_tensor(
    const trit_matrix_t* weights,
    const nimcp_tensor_t* input,
    float weight_scale
);

//=============================================================================
// Gradient-Aware Quantization (for Training)
//=============================================================================

/**
 * @brief Straight-through estimator quantization
 *
 * WHAT: Quantize tensor preserving gradient path
 * WHY:  Enable backpropagation through ternary quantization
 * HOW:  Forward: quantize; Backward: identity gradient
 *
 * @param tensor Input tensor
 * @param threshold Quantization threshold
 * @return Quantized tensor (same shape, F32), or NULL on failure
 *
 * Note: Returned tensor has values exactly -1, 0, +1 but as floats
 *       for gradient computation compatibility
 */
nimcp_tensor_t* trit_quantize_ste(
    const nimcp_tensor_t* tensor,
    float threshold
);

/**
 * @brief Soft ternary quantization (differentiable)
 *
 * WHAT: Approximate ternary with smooth function
 * WHY:  Differentiable approximation for training
 * HOW:  Smooth step function with temperature
 *
 * @param tensor Input tensor
 * @param temperature Softness parameter (lower = sharper)
 * @return Soft-quantized tensor, or NULL on failure
 */
nimcp_tensor_t* trit_quantize_soft(
    const nimcp_tensor_t* tensor,
    float temperature
);

//=============================================================================
// Tensor Statistics Helpers
//=============================================================================

/**
 * @brief Compute sparsity of ternary-quantized tensor
 *
 * @param tensor Input tensor
 * @param threshold Quantization threshold
 * @return Sparsity (fraction of zeros after quantization)
 */
float trit_tensor_sparsity(
    const nimcp_tensor_t* tensor,
    float threshold
);

/**
 * @brief Compute quantization error (L2)
 *
 * WHAT: Measure information loss from quantization
 * WHY:  Evaluate quantization quality
 * HOW:  ||tensor - dequant(quant(tensor))||_2
 *
 * @param tensor Input tensor
 * @param threshold Quantization threshold
 * @param scale Dequantization scale
 * @return L2 error
 */
float trit_quantization_error(
    const nimcp_tensor_t* tensor,
    float threshold,
    float scale
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_TENSOR_H */
