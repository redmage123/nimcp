/**
 * @file nimcp_working_memory_gpu.h
 * @brief GPU-accelerated working memory operations
 *
 * WHAT: CUDA kernel interfaces for working memory acceleration
 * WHY:  GPU acceleration for API consistency and batching potential
 * HOW:  C-callable wrappers around CUDA kernels
 *
 * HOT PATHS ACCELERATED:
 * - Exponential decay on salience array
 * - Threshold counting for eviction decisions
 *
 * NOTE ON DATA SIZE:
 * Working memory typically has 7-32 items (Miller's 7+/-2). While small data
 * size means limited single-operation GPU benefit, acceleration is valuable:
 * 1. API consistency with other GPU-accelerated cognitive modules
 * 2. Enables batching with attention, FEP, and other cognitive operations
 * 3. Zero-copy integration when data already on GPU
 * 4. Prepares for future extensions (larger associative buffers)
 *
 * REFACTORED (2026-01-01):
 * ========================
 * - Added tensor-based API variants for zero-copy GPU integration
 * - Original host-pointer API retained for backward compatibility
 *
 * @version 1.1
 * @author NIMCP Development Team - Phase 2.3 GPU Integration
 * @date 2025-2026
 */

#ifndef NIMCP_WORKING_MEMORY_GPU_H
#define NIMCP_WORKING_MEMORY_GPU_H

// Include GPU headers before extern "C" (they may contain C++ code)
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Core GPU Decay Functions (Legacy Host Pointer API)
//=============================================================================

/**
 * @brief Apply exponential decay to working memory salience on GPU
 *
 * WHAT: GPU-accelerated time-based decay of salience values
 * WHY:  Parallel computation of decay across all items
 * HOW:  CUDA kernel: salience[i] *= exp(-elapsed / tau)
 *
 * FORMULA:
 *   salience_new = salience_old * exp(-elapsed_ms / decay_tau_ms)
 *
 * NUMERICAL STABILITY:
 * - Exponent clamped to -80 to prevent underflow to zero
 * - Items decay to threshold, not zero (biological plausibility)
 *
 * PERFORMANCE NOTES:
 * - For small arrays (<4 items), GPU overhead may exceed benefit
 * - Beneficial for batching with other GPU cognitive operations
 * - Zero-copy when integrated with GPU attention pipeline
 *
 * @param salience       Salience array (modified in-place)
 * @param timestamps     Last access time for each item (milliseconds)
 * @param current_time   Current time in milliseconds
 * @param decay_tau_ms   Decay time constant (default: 1000ms)
 * @param min_salience   Minimum salience threshold (for reference, not enforced)
 * @param num_items      Number of items in working memory
 * @return 0 on success, -1 on error
 *
 * @note Caller must ensure arrays are valid and aligned
 * @note Falls back to CPU implementation if CUDA unavailable
 */
int nimcp_gpu_working_memory_decay(
    float* salience,
    const uint64_t* timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    uint32_t num_items
);

/**
 * @brief Apply decay with attention refresh handling
 *
 * WHAT: GPU decay that skips attention-refreshed items
 * WHY:  Rehearsal prevents decay (biological attention mechanism)
 * HOW:  Check refresh flag, skip if set, then clear flag
 *
 * ALGORITHM:
 *   for each item:
 *     if attention_refreshed[i]:
 *       attention_refreshed[i] = false  // Clear flag
 *       continue                         // Skip decay
 *     else:
 *       salience[i] *= exp(-elapsed / tau)
 *
 * @param salience            Salience array (modified)
 * @param timestamps          Last access timestamps
 * @param attention_refreshed Refresh flags (modified, cleared after use)
 * @param current_time        Current time in milliseconds
 * @param decay_tau_ms        Decay time constant
 * @param min_salience        Minimum salience threshold (reference)
 * @param num_items           Number of items
 * @return 0 on success, -1 on error
 *
 * @note attention_refreshed flags are cleared after kernel execution
 */
int nimcp_gpu_working_memory_decay_with_attention(
    float* salience,
    const uint64_t* timestamps,
    bool* attention_refreshed,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    uint32_t num_items
);

//=============================================================================
// Threshold Analysis
//=============================================================================

/**
 * @brief Count items below threshold on GPU
 *
 * WHAT: Count how many items have salience below minimum threshold
 * WHY:  Determine eviction count before array modifications
 * HOW:  Parallel reduction with atomic counting
 *
 * @param salience     Salience array (read-only)
 * @param min_salience Minimum salience threshold
 * @param num_items    Number of items
 * @param count        Output: count of items below threshold
 * @return 0 on success, -1 on error
 */
int nimcp_gpu_working_memory_count_below_threshold(
    const float* salience,
    float min_salience,
    uint32_t num_items,
    uint32_t* count
);

//=============================================================================
// Tensor-Based API (New in v1.1)
//=============================================================================

/**
 * @brief Apply working memory decay using GPU tensors
 *
 * WHAT: GPU decay using nimcp_gpu_tensor_t for salience
 * WHY:  Zero-copy integration when data already on GPU
 * HOW:  Direct kernel launch on tensor data pointers
 *
 * This variant is useful when working memory salience is already on GPU
 * from prior cognitive operations (e.g., after attention processing).
 *
 * TENSOR REQUIREMENTS:
 * - salience: FP32 tensor
 * - timestamps_low: UINT32 tensor (lower 32 bits of timestamps)
 * - timestamps_high: UINT32 tensor (upper 32 bits of timestamps)
 *
 * @param ctx GPU context
 * @param salience GPU tensor for salience values (FP32, modified in-place)
 * @param timestamps_low GPU tensor for lower 32 bits of timestamps (UINT32)
 * @param timestamps_high GPU tensor for upper 32 bits of timestamps (UINT32)
 * @param current_time Current time in milliseconds
 * @param decay_tau_ms Decay time constant
 * @param min_salience Minimum salience threshold
 * @return true on success, false on failure
 */
bool nimcp_gpu_working_memory_decay_tensor(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* salience,
    const nimcp_gpu_tensor_t* timestamps_low,
    const nimcp_gpu_tensor_t* timestamps_high,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience
);

/**
 * @brief Apply working memory decay with raw 64-bit timestamp pointer
 *
 * WHAT: GPU decay with salience tensor and raw timestamp device pointer
 * WHY:  Most efficient when timestamps are already in 64-bit format on device
 * HOW:  Direct kernel launch with no timestamp conversion
 *
 * @param ctx GPU context
 * @param salience GPU tensor for salience values (FP32, modified in-place)
 * @param d_timestamps Device pointer to 64-bit timestamps
 * @param current_time Current time in milliseconds
 * @param decay_tau_ms Decay time constant
 * @param min_salience Minimum salience threshold
 * @return true on success, false on failure
 */
bool nimcp_gpu_working_memory_decay_tensor_raw_timestamps(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* salience,
    const uint64_t* d_timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience
);

/**
 * @brief Count items below threshold using GPU tensor
 *
 * @param ctx GPU context
 * @param salience GPU tensor for salience values (FP32, read-only)
 * @param min_salience Minimum salience threshold
 * @param count Output: count of items below threshold
 * @return true on success, false on failure
 */
bool nimcp_gpu_working_memory_count_below_threshold_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* salience,
    float min_salience,
    uint32_t* count
);

//=============================================================================
// Availability Check
//=============================================================================

/**
 * @brief Check if GPU working memory acceleration is available
 *
 * WHAT: Query CUDA availability for working memory operations
 * WHY:  Allow runtime fallback to CPU implementation
 * HOW:  Check for CUDA devices
 *
 * @return true if GPU acceleration available, false otherwise
 */
bool nimcp_gpu_working_memory_available(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WORKING_MEMORY_GPU_H
