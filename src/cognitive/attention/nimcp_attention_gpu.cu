/**
 * @file nimcp_attention_gpu.cu
 * @brief GPU CUDA Kernels for Attention Precision Weighting
 *
 * WHAT: CUDA kernels for GPU-accelerated attention precision weighting
 * WHY:  GPU acceleration for precision-weighted gain modulation in attention system
 * HOW:  Custom kernels for precision-weighted input scaling (FEP-attention integration)
 *
 * BIOLOGICAL BASIS:
 * =================
 * Precision-weighted attention implements Feldman & Friston (2010) theory:
 * - Attention = precision optimization in predictive coding
 * - High precision -> Increased gain on prediction errors -> Stronger attention
 * - Low precision -> Reduced gain -> Attentional suppression
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Precision weighting is applied element-wise to large attention vectors/matrices.
 * GPU parallelization provides significant speedup for:
 * - High-dimensional sensory inputs
 * - Multi-head attention with many channels
 * - Real-time precision modulation updates
 *
 * REFACTORED (2026-01-01):
 * ========================
 * - Updated to use nimcp_gpu_tensor_t throughout
 * - Leverages GPU tensor API for element-wise operations where appropriate
 * - Maintains custom kernels for fused precision-weighting operations
 *
 * @version 1.1
 * @author NIMCP Development Team
 * @date 2025-2026
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "ATTENTION_GPU"

#define CUDA_CHECK(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err)); \
        return false; \
    } \
} while(0)

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Precision-Weighted Gain Modulation Kernels
//=============================================================================

/**
 * @brief GPU kernel for precision-weighted attention gain modulation
 *
 * WHAT: Apply precision-weighted gain to input signals
 * WHY:  Core FEP-attention integration: precision modulates attention gain
 * HOW:  output[i] = input[i] * base_gain * precision[i]
 *
 * BIOLOGICAL MAPPING:
 * - input: Sensory or prediction error signals
 * - precision: FEP precision values (inverse variance of prediction errors)
 * - base_gain: Baseline attention gain (from attention system state)
 * - output: Precision-weighted attended signal
 *
 * NOTE: Kernel uses raw float* for device memory access. Tensor metadata
 *       (dims, numel) is validated in the wrapper function.
 *
 * @param output Output buffer for precision-weighted signal
 * @param input Input signal buffer
 * @param precision Per-element precision values from FEP system
 * @param base_gain Baseline attention gain scalar
 * @param numel Number of elements to process
 */
__global__ void kernel_attention_precision_weight(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ precision,
    float base_gain,
    size_t numel
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel) return;

    output[idx] = input[idx] * base_gain * precision[idx];
}

/**
 * @brief GPU kernel for precision-weighted gain with clamping
 *
 * WHAT: Apply precision-weighted gain with output clamping
 * WHY:  Prevent numerical instability from extreme precision values
 * HOW:  output[i] = clamp(input[i] * base_gain * precision[i], min_val, max_val)
 *
 * @param output Output buffer for precision-weighted signal
 * @param input Input signal buffer
 * @param precision Per-element precision values
 * @param base_gain Baseline attention gain scalar
 * @param min_val Minimum output value (prevents underflow)
 * @param max_val Maximum output value (prevents overflow)
 * @param numel Number of elements to process
 */
__global__ void kernel_attention_precision_weight_clamped(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ precision,
    float base_gain,
    float min_val,
    float max_val,
    size_t numel
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel) return;

    float result = input[idx] * base_gain * precision[idx];
    output[idx] = fminf(fmaxf(result, min_val), max_val);
}

/**
 * @brief GPU kernel for fused precision weighting with gain modulation
 *
 * WHAT: Apply precision-weighted gain with learned modulation factor
 * WHY:  Supports adaptive precision sensitivity from Attention-FEP bridge
 * HOW:  output[i] = input[i] * (1 + (base_gain - 1) * precision_sensitivity) * precision[i]
 *
 * This implements the attention_fep_apply_precision_gain_modulation logic:
 * - gain_modifier = 1.0 + (gain_modifier - 1.0) * precision_sensitivity
 *
 * @param output Output buffer for precision-weighted signal
 * @param input Input signal buffer
 * @param precision Per-element precision values
 * @param base_gain Baseline attention gain (1.5 for high precision, 0.5 for low)
 * @param precision_sensitivity Scaling factor for precision effect [0-1]
 * @param numel Number of elements to process
 */
__global__ void kernel_attention_precision_weight_adaptive(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ precision,
    float base_gain,
    float precision_sensitivity,
    size_t numel
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel) return;

    // Compute adaptive gain modifier: blend base_gain with precision_sensitivity
    float adaptive_gain = 1.0f + (base_gain - 1.0f) * precision_sensitivity;
    output[idx] = input[idx] * adaptive_gain * precision[idx];
}

/**
 * @brief GPU kernel for batched precision weighting
 *
 * WHAT: Apply precision weighting to multiple attention heads in parallel
 * WHY:  Multi-head attention requires processing multiple heads efficiently
 * HOW:  Each head has its own precision vector, process all in one kernel
 *
 * @param output Output buffer [batch_size x head_dim]
 * @param input Input signal buffer [batch_size x head_dim]
 * @param precision Precision values [batch_size x head_dim] or [head_dim] if shared
 * @param base_gains Per-head base gain values [batch_size]
 * @param batch_size Number of attention heads
 * @param head_dim Dimension of each head
 * @param shared_precision If true, precision is shared across all heads
 */
__global__ void kernel_attention_precision_weight_batched(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ precision,
    const float* __restrict__ base_gains,
    uint32_t batch_size,
    uint32_t head_dim,
    bool shared_precision
) {
    size_t total_elements = (size_t)batch_size * head_dim;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_elements) return;

    uint32_t batch_idx = idx / head_dim;
    uint32_t elem_idx = idx % head_dim;

    float gain = base_gains[batch_idx];
    float prec = shared_precision ? precision[elem_idx] : precision[idx];

    output[idx] = input[idx] * gain * prec;
}

//=============================================================================
// Helper: Validate tensor compatibility
//=============================================================================

static bool validate_tensor_compatibility(
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    const char* op_name
) {
    if (!a || !b) {
        LOG_ERROR("%s: NULL tensor parameter", op_name);
        return false;
    }

    if (a->numel != b->numel) {
        LOG_ERROR("%s: tensor element count mismatch (a=%zu, b=%zu)",
                  op_name, a->numel, b->numel);
        return false;
    }

    if (a->precision != NIMCP_GPU_PRECISION_FP32 ||
        b->precision != NIMCP_GPU_PRECISION_FP32) {
        LOG_ERROR("%s: only FP32 precision currently supported", op_name);
        return false;
    }

    return true;
}

//=============================================================================
// C-Callable Wrapper Functions
//=============================================================================

extern "C" {

/**
 * @brief Launch precision-weighted attention gain kernel
 *
 * WHAT: C wrapper for GPU precision weighting kernel
 * WHY:  Enable C code to invoke GPU-accelerated precision weighting
 * HOW:  Validate tensors, configure grid/block, launch kernel
 *
 * TENSOR REQUIREMENTS:
 * - All tensors must have same numel (total element count)
 * - All tensors must be FP32 precision
 * - Tensors must be on GPU (have valid device data pointers)
 *
 * @param ctx GPU context
 * @param output GPU tensor for output
 * @param input GPU tensor for input
 * @param precision GPU tensor for precision values
 * @param base_gain Baseline attention gain
 * @return true on success, false on failure
 */
bool nimcp_gpu_attention_precision_weight(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain
) {
    if (!ctx) {
        LOG_ERROR("Null GPU context in attention precision weight");
        return false;
    }

    if (!output || !output->data) {
        LOG_ERROR("Invalid output tensor in attention precision weight");
        return false;
    }

    if (!validate_tensor_compatibility(input, precision, "attention_precision_weight")) {
        return false;
    }

    if (input->numel != output->numel) {
        LOG_ERROR("Output tensor size mismatch: expected %zu, got %zu",
                  input->numel, output->numel);
        return false;
    }

    size_t numel = input->numel;

    // Launch kernel with tensor data pointers
    kernel_attention_precision_weight<<<GRID_SIZE(numel), BLOCK_SIZE>>>(
        static_cast<float*>(output->data),
        static_cast<const float*>(input->data),
        static_cast<const float*>(precision->data),
        base_gain,
        numel
    );

    CUDA_CHECK(cudaGetLastError());
    return true;
}

/**
 * @brief Launch precision-weighted attention gain kernel with clamping
 *
 * @param ctx GPU context
 * @param output GPU tensor for output
 * @param input GPU tensor for input
 * @param precision GPU tensor for precision values
 * @param base_gain Baseline attention gain
 * @param min_val Minimum output value
 * @param max_val Maximum output value
 * @return true on success, false on failure
 */
bool nimcp_gpu_attention_precision_weight_clamped(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain,
    float min_val,
    float max_val
) {
    if (!ctx) {
        LOG_ERROR("Null GPU context in attention precision weight clamped");
        return false;
    }

    if (!output || !output->data) {
        LOG_ERROR("Invalid output tensor");
        return false;
    }

    if (!validate_tensor_compatibility(input, precision, "attention_precision_weight_clamped")) {
        return false;
    }

    if (input->numel != output->numel) {
        LOG_ERROR("Output tensor size mismatch");
        return false;
    }

    size_t numel = input->numel;

    kernel_attention_precision_weight_clamped<<<GRID_SIZE(numel), BLOCK_SIZE>>>(
        static_cast<float*>(output->data),
        static_cast<const float*>(input->data),
        static_cast<const float*>(precision->data),
        base_gain,
        min_val,
        max_val,
        numel
    );

    CUDA_CHECK(cudaGetLastError());
    return true;
}

/**
 * @brief Launch adaptive precision-weighted attention gain kernel
 *
 * @param ctx GPU context
 * @param output GPU tensor for output
 * @param input GPU tensor for input
 * @param precision GPU tensor for precision values
 * @param base_gain Baseline attention gain
 * @param precision_sensitivity Precision effect scaling factor
 * @return true on success, false on failure
 */
bool nimcp_gpu_attention_precision_weight_adaptive(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain,
    float precision_sensitivity
) {
    if (!ctx) {
        LOG_ERROR("Null GPU context in attention precision weight adaptive");
        return false;
    }

    if (!output || !output->data) {
        LOG_ERROR("Invalid output tensor");
        return false;
    }

    if (!validate_tensor_compatibility(input, precision, "attention_precision_weight_adaptive")) {
        return false;
    }

    if (input->numel != output->numel) {
        LOG_ERROR("Output tensor size mismatch");
        return false;
    }

    size_t numel = input->numel;

    kernel_attention_precision_weight_adaptive<<<GRID_SIZE(numel), BLOCK_SIZE>>>(
        static_cast<float*>(output->data),
        static_cast<const float*>(input->data),
        static_cast<const float*>(precision->data),
        base_gain,
        precision_sensitivity,
        numel
    );

    CUDA_CHECK(cudaGetLastError());
    return true;
}

/**
 * @brief Launch batched precision-weighted attention gain kernel
 *
 * @param ctx GPU context
 * @param output GPU tensor for output [batch_size x head_dim]
 * @param input GPU tensor for input [batch_size x head_dim]
 * @param precision GPU tensor for precision values
 * @param base_gains GPU tensor for per-head base gains [batch_size]
 * @param batch_size Number of attention heads
 * @param head_dim Dimension of each head
 * @param shared_precision If true, precision is shared across all heads
 * @return true on success, false on failure
 */
bool nimcp_gpu_attention_precision_weight_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    const nimcp_gpu_tensor_t* base_gains,
    uint32_t batch_size,
    uint32_t head_dim,
    bool shared_precision
) {
    if (!ctx) {
        LOG_ERROR("Null GPU context in batched attention precision weight");
        return false;
    }

    if (!output || !input || !precision || !base_gains) {
        LOG_ERROR("Null tensor parameter in attention precision weight batched");
        return false;
    }

    if (!output->data || !input->data || !precision->data || !base_gains->data) {
        LOG_ERROR("Invalid tensor data pointer");
        return false;
    }

    size_t total_elements = (size_t)batch_size * head_dim;
    if (input->numel != total_elements || output->numel != total_elements) {
        LOG_ERROR("Tensor dimension mismatch for batched operation: expected %zu elements",
                  total_elements);
        return false;
    }

    if (base_gains->numel < batch_size) {
        LOG_ERROR("Base gains tensor too small: need %u, have %zu",
                  batch_size, base_gains->numel);
        return false;
    }

    // Validate precision tensor size
    size_t expected_prec_size = shared_precision ? head_dim : total_elements;
    if (precision->numel < expected_prec_size) {
        LOG_ERROR("Precision tensor too small: need %zu, have %zu",
                  expected_prec_size, precision->numel);
        return false;
    }

    kernel_attention_precision_weight_batched<<<GRID_SIZE(total_elements), BLOCK_SIZE>>>(
        static_cast<float*>(output->data),
        static_cast<const float*>(input->data),
        static_cast<const float*>(precision->data),
        static_cast<const float*>(base_gains->data),
        batch_size,
        head_dim,
        shared_precision
    );

    CUDA_CHECK(cudaGetLastError());
    return true;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

// CPU fallback stubs when CUDA is not available

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <stdbool.h>

#define LOG_MODULE "ATTENTION_GPU"

bool nimcp_gpu_attention_precision_weight(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain
) {
    LOG_WARN("CUDA not available - attention precision weighting requires GPU");
    return false;
}

bool nimcp_gpu_attention_precision_weight_clamped(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain,
    float min_val,
    float max_val
) {
    LOG_WARN("CUDA not available - attention precision weighting requires GPU");
    return false;
}

bool nimcp_gpu_attention_precision_weight_adaptive(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    float base_gain,
    float precision_sensitivity
) {
    LOG_WARN("CUDA not available - attention precision weighting requires GPU");
    return false;
}

bool nimcp_gpu_attention_precision_weight_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* precision,
    const nimcp_gpu_tensor_t* base_gains,
    uint32_t batch_size,
    uint32_t head_dim,
    bool shared_precision
) {
    LOG_WARN("CUDA not available - batched attention precision weighting requires GPU");
    return false;
}

#endif // NIMCP_ENABLE_CUDA
