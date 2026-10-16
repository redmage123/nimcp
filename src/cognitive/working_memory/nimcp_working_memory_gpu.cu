/**
 * @file nimcp_working_memory_gpu.cu
 * @brief GPU-accelerated working memory decay kernel
 *
 * WHAT: CUDA kernel for exponential decay of salience in working memory
 * WHY:  GPU acceleration for API consistency with other accelerated modules
 * HOW:  Parallel exponential decay computation across working memory items
 *
 * HOT PATH ACCELERATED:
 * - Exponential decay on salience array: salience[i] *= exp(-elapsed / tau)
 *
 * NOTE ON DATA SIZE:
 * Working memory typically has 7-32 items (Miller's 7+/-2). While small data
 * size means limited single-operation benefit, GPU acceleration is valuable:
 * 1. API consistency with other GPU-accelerated modules
 * 2. Enables batching with other cognitive operations
 * 3. Prepares for future extensions (larger associative buffers)
 * 4. Zero-copy integration when data already on GPU (e.g., after attention)
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex (PFC) maintains ~7 items in active state
 * - Exponential decay without rehearsal (tau approx 1-2 seconds)
 * - Items untouched decay to threshold, not zero (MIN_DECAY_EXPONENT clamp)
 *
 * REFACTORED (2026-01-01):
 * ========================
 * - Added tensor-based API variants for consistency with other GPU modules
 * - Original host-pointer API retained for backward compatibility
 * - Uses nimcp_gpu_tensor_t for on-device operations
 *
 * @version 1.1
 * @author NIMCP Development Team - Phase 2.3 GPU Integration
 * @date 2025-2026
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Our headers (after CUDA headers to avoid extern "C" issues)
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "WM_GPU"

//=============================================================================
// CUDA Error Handling
//=============================================================================

#define CUDA_CHECK(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err)); \
        return -1; \
    } \
} while(0)

#define CUDA_CHECK_BOOL(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err)); \
        return false; \
    } \
} while(0)

// Block size - optimized for small workloads (32-256 items)
#define WM_BLOCK_SIZE 32
#define WM_GRID_SIZE(n) (((n) + WM_BLOCK_SIZE - 1) / WM_BLOCK_SIZE)

//=============================================================================
// Working Memory Decay Kernels
//=============================================================================

/**
 * @brief GPU kernel for exponential decay of working memory salience
 *
 * WHAT: Apply time-based exponential decay to salience array
 * WHY:  Model biological forgetting curve (without attention refresh)
 * HOW:  Each thread handles one item: salience[i] *= exp(-elapsed / tau)
 *
 * FORMULA: salience_new = salience_old * exp(-elapsed_ms / decay_tau_ms)
 *
 * NUMERICAL STABILITY:
 * - Exponent clamped to MIN_DECAY_EXPONENT (-80) to prevent underflow
 * - Items decay to threshold, not zero (biological plausibility)
 *
 * @param salience      [in/out] Salience values for each item (device memory)
 * @param timestamps    [in]     Last access time for each item (device memory, uint64)
 * @param current_time  [in]     Current time in milliseconds
 * @param decay_tau_ms  [in]     Decay time constant (default: 1000ms)
 * @param min_salience  [in]     Minimum salience threshold for eviction check
 * @param num_items     [in]     Number of items in working memory
 */
__global__ void kernel_working_memory_decay(
    float* __restrict__ salience,
    const uint64_t* __restrict__ timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    size_t num_items
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_items) return;

    // Calculate elapsed time since last access
    float elapsed = (float)(current_time - timestamps[idx]);

    // Compute decay exponent with clamp to prevent underflow
    // MIN_DECAY_EXPONENT = -80 ensures meaningful decay factor (~10^-35)
    float exponent = -elapsed / decay_tau_ms;
    float clamped_exponent = fmaxf(exponent, -80.0f);

    // Apply exponential decay
    float decay = expf(clamped_exponent);
    salience[idx] *= decay;
}

/**
 * @brief Extended decay kernel with attention refresh handling
 *
 * WHAT: Apply decay only to non-refreshed items, then clear refresh flags
 * WHY:  Attention refresh (rehearsal) prevents decay (biological)
 * HOW:  Skip refreshed items, apply decay to others, clear flags
 *
 * @param salience           [in/out] Salience values (device memory)
 * @param timestamps         [in]     Last access timestamps (device memory)
 * @param attention_refreshed [in/out] Refresh flags (device memory, cleared after use)
 * @param current_time       [in]     Current time in milliseconds
 * @param decay_tau_ms       [in]     Decay time constant
 * @param min_salience       [in]     Minimum salience threshold
 * @param num_items          [in]     Number of items
 */
__global__ void kernel_working_memory_decay_with_attention(
    float* __restrict__ salience,
    const uint64_t* __restrict__ timestamps,
    bool* __restrict__ attention_refreshed,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    size_t num_items
) {
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_items) return;

    // Check if item was attention-refreshed (skip decay)
    if (attention_refreshed[idx]) {
        attention_refreshed[idx] = false;  // Clear refresh flag
        return;  // No decay for refreshed items
    }

    // Calculate elapsed time and decay
    float elapsed = (float)(current_time - timestamps[idx]);
    float exponent = fmaxf(-elapsed / decay_tau_ms, -80.0f);
    float decay = expf(exponent);
    salience[idx] *= decay;
}

/**
 * @brief Kernel to count items below threshold (for eviction counting)
 *
 * WHAT: Count how many items have salience below threshold
 * WHY:  Determine how many items will be evicted due to decay
 * HOW:  Parallel reduction with atomic add
 *
 * @param salience      [in]  Salience values (device memory)
 * @param min_salience  [in]  Minimum threshold
 * @param num_items     [in]  Number of items
 * @param below_count   [out] Count of items below threshold (device memory)
 */
__global__ void kernel_count_below_threshold(
    const float* __restrict__ salience,
    float min_salience,
    size_t num_items,
    uint32_t* __restrict__ below_count
) {
    __shared__ uint32_t shared_count;
    if (threadIdx.x == 0) shared_count = 0;
    __syncthreads();

    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_items && salience[idx] < min_salience) {
        atomicAdd(&shared_count, 1);
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        atomicAdd(below_count, shared_count);
    }
}

//=============================================================================
// Helper: Validate tensor for working memory operations
//=============================================================================

static bool validate_wm_tensor(
    const nimcp_gpu_tensor_t* tensor,
    const char* name,
    nimcp_gpu_precision_t expected_precision
) {
    if (!tensor) {
        LOG_ERROR("Working memory: NULL %s tensor", name);
        return false;
    }

    if (!tensor->data) {
        LOG_ERROR("Working memory: %s tensor has NULL data", name);
        return false;
    }

    if (tensor->precision != expected_precision) {
        LOG_ERROR("Working memory: %s tensor has wrong precision (expected %d, got %d)",
                  name, expected_precision, tensor->precision);
        return false;
    }

    return true;
}

//=============================================================================
// C-Callable Launch Wrappers - Legacy Host Pointer API
//=============================================================================

extern "C" {

/**
 * @brief Launch working memory decay kernel on GPU (legacy host pointer API)
 *
 * WHAT: C-callable wrapper for GPU decay kernel with host pointers
 * WHY:  Backward compatibility with existing code
 * HOW:  Upload data, launch kernel, download results
 *
 * @param salience       Host salience array (modified in-place)
 * @param timestamps     Host timestamp array
 * @param current_time   Current time in milliseconds
 * @param decay_tau_ms   Decay time constant
 * @param min_salience   Minimum salience threshold
 * @param num_items      Number of items in working memory
 * @return 0 on success, -1 on error
 */
int nimcp_gpu_working_memory_decay(
    float* salience,
    const uint64_t* timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    uint32_t num_items
) {
    if (!salience || !timestamps || num_items == 0) {
        LOG_ERROR("Invalid parameters for GPU working memory decay");
        return -1;
    }

    // For very small arrays, GPU overhead may exceed benefit
    // But we proceed for API consistency and batching potential
    if (num_items < 4) {
        LOG_DEBUG("WM decay: small array (%u items), GPU overhead may exceed benefit", num_items);
    }

    // Allocate device memory
    float* d_salience = NULL;
    uint64_t* d_timestamps = NULL;
    size_t salience_size = num_items * sizeof(float);
    size_t timestamps_size = num_items * sizeof(uint64_t);

    CUDA_CHECK(cudaMalloc(&d_salience, salience_size));
    CUDA_CHECK(cudaMalloc(&d_timestamps, timestamps_size));

    // Copy data to device
    CUDA_CHECK(cudaMemcpy(d_salience, salience, salience_size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_timestamps, timestamps, timestamps_size, cudaMemcpyHostToDevice));

    // Launch kernel
    kernel_working_memory_decay<<<WM_GRID_SIZE(num_items), WM_BLOCK_SIZE>>>(
        d_salience, d_timestamps, current_time, decay_tau_ms, min_salience, num_items
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Kernel launch failed: %s", cudaGetErrorString(err));
        cudaFree(d_salience);
        cudaFree(d_timestamps);
        return -1;
    }

    // Copy result back
    CUDA_CHECK(cudaMemcpy(salience, d_salience, salience_size, cudaMemcpyDeviceToHost));

    // Cleanup
    cudaFree(d_salience);
    cudaFree(d_timestamps);

    LOG_DEBUG("GPU working memory decay completed: %u items", num_items);
    return 0;
}

/**
 * @brief Launch decay kernel with attention refresh handling (legacy host pointer API)
 *
 * @param salience            Host salience array (modified)
 * @param timestamps          Host timestamp array
 * @param attention_refreshed Host attention refresh flags (modified)
 * @param current_time        Current time in milliseconds
 * @param decay_tau_ms        Decay time constant
 * @param min_salience        Minimum salience threshold
 * @param num_items           Number of items
 * @return 0 on success, -1 on error
 */
int nimcp_gpu_working_memory_decay_with_attention(
    float* salience,
    const uint64_t* timestamps,
    bool* attention_refreshed,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    uint32_t num_items
) {
    if (!salience || !timestamps || !attention_refreshed || num_items == 0) {
        LOG_ERROR("Invalid parameters for GPU working memory decay with attention");
        return -1;
    }

    // Allocate device memory
    float* d_salience = NULL;
    uint64_t* d_timestamps = NULL;
    bool* d_attention = NULL;
    size_t salience_size = num_items * sizeof(float);
    size_t timestamps_size = num_items * sizeof(uint64_t);
    size_t attention_size = num_items * sizeof(bool);

    CUDA_CHECK(cudaMalloc(&d_salience, salience_size));
    CUDA_CHECK(cudaMalloc(&d_timestamps, timestamps_size));
    CUDA_CHECK(cudaMalloc(&d_attention, attention_size));

    // Copy data to device
    CUDA_CHECK(cudaMemcpy(d_salience, salience, salience_size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_timestamps, timestamps, timestamps_size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_attention, attention_refreshed, attention_size, cudaMemcpyHostToDevice));

    // Launch kernel
    kernel_working_memory_decay_with_attention<<<WM_GRID_SIZE(num_items), WM_BLOCK_SIZE>>>(
        d_salience, d_timestamps, d_attention, current_time, decay_tau_ms, min_salience, num_items
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        LOG_ERROR("Kernel launch failed: %s", cudaGetErrorString(err));
        cudaFree(d_salience);
        cudaFree(d_timestamps);
        cudaFree(d_attention);
        return -1;
    }

    // Copy results back
    CUDA_CHECK(cudaMemcpy(salience, d_salience, salience_size, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(attention_refreshed, d_attention, attention_size, cudaMemcpyDeviceToHost));

    // Cleanup
    cudaFree(d_salience);
    cudaFree(d_timestamps);
    cudaFree(d_attention);

    return 0;
}

/**
 * @brief Count items below threshold on GPU (legacy host pointer API)
 *
 * @param salience     Host salience array
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
) {
    if (!salience || !count || num_items == 0) {
        return -1;
    }

    float* d_salience = NULL;
    uint32_t* d_count = NULL;

    CUDA_CHECK(cudaMalloc(&d_salience, num_items * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_count, sizeof(uint32_t)));

    CUDA_CHECK(cudaMemcpy(d_salience, salience, num_items * sizeof(float), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemset(d_count, 0, sizeof(uint32_t)));

    kernel_count_below_threshold<<<WM_GRID_SIZE(num_items), WM_BLOCK_SIZE>>>(
        d_salience, min_salience, num_items, d_count
    );

    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaMemcpy(count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost));

    cudaFree(d_salience);
    cudaFree(d_count);

    return 0;
}

//=============================================================================
// New Tensor-Based API
//=============================================================================

/**
 * @brief Apply working memory decay using GPU tensors (zero-copy when data on GPU)
 *
 * WHAT: GPU decay using nimcp_gpu_tensor_t for salience and timestamps
 * WHY:  Zero-copy integration when data already on GPU from other cognitive ops
 * HOW:  Direct kernel launch on tensor data pointers
 *
 * TENSOR REQUIREMENTS:
 * - salience: FP32 tensor with num_items elements
 * - timestamps: UINT32 tensor treated as uint64_t pairs (2 x num_items uint32s)
 *               OR use nimcp_gpu_tensor_t with NIMCP_GPU_PRECISION_UINT32
 *
 * @param ctx GPU context
 * @param salience GPU tensor for salience values (modified in-place)
 * @param timestamps_low GPU tensor for lower 32 bits of timestamps
 * @param timestamps_high GPU tensor for upper 32 bits of timestamps
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
) {
    if (!ctx) {
        LOG_ERROR("Null GPU context");
        return false;
    }

    if (!validate_wm_tensor(salience, "salience", NIMCP_GPU_PRECISION_FP32)) {
        return false;
    }

    if (!validate_wm_tensor(timestamps_low, "timestamps_low", NIMCP_GPU_PRECISION_UINT32)) {
        return false;
    }

    if (!validate_wm_tensor(timestamps_high, "timestamps_high", NIMCP_GPU_PRECISION_UINT32)) {
        return false;
    }

    size_t num_items = salience->numel;
    if (timestamps_low->numel != num_items || timestamps_high->numel != num_items) {
        LOG_ERROR("Timestamp tensor size mismatch");
        return false;
    }

    // For this kernel, we need to combine timestamp halves
    // Allocate temporary buffer for full 64-bit timestamps
    uint64_t* d_timestamps = NULL;
    CUDA_CHECK_BOOL(cudaMalloc(&d_timestamps, num_items * sizeof(uint64_t)));

    // Launch a simple kernel to combine timestamp halves (TODO: could be optimized)
    // For now, use simple memory operations
    uint32_t* h_low = (uint32_t*)malloc(num_items * sizeof(uint32_t));
    uint32_t* h_high = (uint32_t*)malloc(num_items * sizeof(uint32_t));
    uint64_t* h_combined = (uint64_t*)malloc(num_items * sizeof(uint64_t));

    if (!h_low || !h_high || !h_combined) {
        LOG_ERROR("Failed to allocate host memory for timestamp conversion");
        free(h_low);
        free(h_high);
        free(h_combined);
        cudaFree(d_timestamps);
        return false;
    }

    CUDA_CHECK_BOOL(cudaMemcpy(h_low, timestamps_low->data, num_items * sizeof(uint32_t), cudaMemcpyDeviceToHost));
    CUDA_CHECK_BOOL(cudaMemcpy(h_high, timestamps_high->data, num_items * sizeof(uint32_t), cudaMemcpyDeviceToHost));

    for (size_t i = 0; i < num_items; i++) {
        h_combined[i] = ((uint64_t)h_high[i] << 32) | h_low[i];
    }

    CUDA_CHECK_BOOL(cudaMemcpy(d_timestamps, h_combined, num_items * sizeof(uint64_t), cudaMemcpyHostToDevice));

    free(h_low);
    free(h_high);
    free(h_combined);

    // Launch decay kernel
    kernel_working_memory_decay<<<WM_GRID_SIZE(num_items), WM_BLOCK_SIZE>>>(
        static_cast<float*>(salience->data),
        d_timestamps,
        current_time,
        decay_tau_ms,
        min_salience,
        num_items
    );

    cudaError_t err = cudaGetLastError();
    cudaFree(d_timestamps);

    if (err != cudaSuccess) {
        LOG_ERROR("Kernel launch failed: %s", cudaGetErrorString(err));
        return false;
    }

    return true;
}

/**
 * @brief Apply working memory decay with single 64-bit timestamp tensor
 *
 * WHAT: GPU decay using a contiguous 64-bit timestamp buffer
 * WHY:  More efficient when timestamps are already in 64-bit format on device
 * HOW:  Direct kernel launch with no timestamp conversion
 *
 * @param ctx GPU context
 * @param salience GPU tensor for salience values (FP32, modified in-place)
 * @param timestamps Device pointer to 64-bit timestamps (raw pointer for efficiency)
 * @param current_time Current time in milliseconds
 * @param decay_tau_ms Decay time constant
 * @param min_salience Minimum salience threshold
 * @param num_items Number of items
 * @return true on success, false on failure
 */
bool nimcp_gpu_working_memory_decay_tensor_raw_timestamps(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* salience,
    const uint64_t* d_timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience
) {
    if (!ctx) {
        LOG_ERROR("Null GPU context");
        return false;
    }

    if (!validate_wm_tensor(salience, "salience", NIMCP_GPU_PRECISION_FP32)) {
        return false;
    }

    if (!d_timestamps) {
        LOG_ERROR("Null timestamps pointer");
        return false;
    }

    size_t num_items = salience->numel;

    kernel_working_memory_decay<<<WM_GRID_SIZE(num_items), WM_BLOCK_SIZE>>>(
        static_cast<float*>(salience->data),
        d_timestamps,
        current_time,
        decay_tau_ms,
        min_salience,
        num_items
    );

    CUDA_CHECK_BOOL(cudaGetLastError());
    return true;
}

/**
 * @brief Count items below threshold using GPU tensor
 *
 * @param ctx GPU context
 * @param salience GPU tensor for salience values (FP32)
 * @param min_salience Minimum salience threshold
 * @param count Output: count of items below threshold
 * @return true on success, false on failure
 */
bool nimcp_gpu_working_memory_count_below_threshold_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* salience,
    float min_salience,
    uint32_t* count
) {
    if (!ctx || !count) {
        LOG_ERROR("Null parameter");
        return false;
    }

    if (!validate_wm_tensor(salience, "salience", NIMCP_GPU_PRECISION_FP32)) {
        return false;
    }

    size_t num_items = salience->numel;
    uint32_t* d_count = NULL;

    CUDA_CHECK_BOOL(cudaMalloc(&d_count, sizeof(uint32_t)));
    CUDA_CHECK_BOOL(cudaMemset(d_count, 0, sizeof(uint32_t)));

    kernel_count_below_threshold<<<WM_GRID_SIZE(num_items), WM_BLOCK_SIZE>>>(
        static_cast<const float*>(salience->data),
        min_salience,
        num_items,
        d_count
    );

    CUDA_CHECK_BOOL(cudaGetLastError());
    CUDA_CHECK_BOOL(cudaMemcpy(count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost));

    cudaFree(d_count);
    return true;
}

/**
 * @brief Check if GPU working memory acceleration is available
 *
 * @return true if CUDA is available, false otherwise
 */
bool nimcp_gpu_working_memory_available(void) {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs (when CUDA not available)
//=============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "WM_GPU"

extern "C" {

int nimcp_gpu_working_memory_decay(
    float* salience,
    const uint64_t* timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    uint32_t num_items
) {
    // CPU fallback implementation
    if (!salience || !timestamps || num_items == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < num_items; i++) {
        float elapsed = (float)(current_time - timestamps[i]);
        float exponent = -elapsed / decay_tau_ms;
        if (exponent < -80.0f) exponent = -80.0f;
        float decay = expf(exponent);
        salience[i] *= decay;
    }

    LOG_DEBUG("CPU fallback: working memory decay for %u items", num_items);
    return 0;
}

int nimcp_gpu_working_memory_decay_with_attention(
    float* salience,
    const uint64_t* timestamps,
    bool* attention_refreshed,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience,
    uint32_t num_items
) {
    if (!salience || !timestamps || !attention_refreshed || num_items == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < num_items; i++) {
        if (attention_refreshed[i]) {
            attention_refreshed[i] = false;
            continue;
        }
        float elapsed = (float)(current_time - timestamps[i]);
        float exponent = -elapsed / decay_tau_ms;
        if (exponent < -80.0f) exponent = -80.0f;
        float decay = expf(exponent);
        salience[i] *= decay;
    }

    return 0;
}

int nimcp_gpu_working_memory_count_below_threshold(
    const float* salience,
    float min_salience,
    uint32_t num_items,
    uint32_t* count
) {
    if (!salience || !count || num_items == 0) {
        return -1;
    }

    *count = 0;
    for (uint32_t i = 0; i < num_items; i++) {
        if (salience[i] < min_salience) {
            (*count)++;
        }
    }
    return 0;
}

bool nimcp_gpu_working_memory_decay_tensor(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* salience,
    const nimcp_gpu_tensor_t* timestamps_low,
    const nimcp_gpu_tensor_t* timestamps_high,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience
) {
    LOG_WARN("CUDA not available - tensor-based working memory decay requires GPU");
    return false;
}

bool nimcp_gpu_working_memory_decay_tensor_raw_timestamps(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* salience,
    const uint64_t* d_timestamps,
    uint64_t current_time,
    float decay_tau_ms,
    float min_salience
) {
    LOG_WARN("CUDA not available - tensor-based working memory decay requires GPU");
    return false;
}

bool nimcp_gpu_working_memory_count_below_threshold_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* salience,
    float min_salience,
    uint32_t* count
) {
    LOG_WARN("CUDA not available - tensor-based threshold counting requires GPU");
    return false;
}

bool nimcp_gpu_working_memory_available(void) {
    return false;  // CUDA not available
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
