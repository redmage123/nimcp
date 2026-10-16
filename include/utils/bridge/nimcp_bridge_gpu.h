/**
 * @file nimcp_bridge_gpu.h
 * @brief GPU-Aware Bridge Base Utilities for NIMCP
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Provides GPU context management utilities for bridge modules
 * WHY:  Enables bridges to leverage GPU acceleration when available
 * HOW:  Wraps GPU context extraction from brain, buffer management, and sync
 *
 * PHASE 2.1: GPU Integration Plan - Bridge GPU Utilities
 *
 * ARCHITECTURE:
 *
 *   +----------------------------------------------------------+
 *   |                 BRIDGE GPU CONTEXT                        |
 *   |                                                          |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |  |  GPU Context |  |  Work Buffer |  |   Stream     |    |
 *   |  | (from brain) |  | (reusable)   |  | (async ops)  |    |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |                           |                              |
 *   |         +--------------------------------+               |
 *   |         | Decision Logic: GPU vs CPU    |               |
 *   |         | (based on data size threshold)|               |
 *   |         +--------------------------------+               |
 *   +----------------------------------------------------------+
 *
 * USAGE:
 * 1. Create GPU context from brain: bridge_gpu_context_create(brain)
 * 2. Check if GPU should be used: bridge_should_use_gpu(ctx, data_size)
 * 3. Ensure work buffer: bridge_gpu_ensure_buffer(ctx, min_size)
 * 4. Sync after async ops: bridge_gpu_sync(ctx)
 * 5. Destroy when done: bridge_gpu_context_destroy(ctx)
 *
 * THREAD SAFETY:
 * - Context creation/destruction NOT thread-safe (call from single thread)
 * - Other operations thread-safe when using separate streams
 *
 * CPU FALLBACK:
 * - All functions gracefully handle NULL GPU context (CPU-only mode)
 * - bridge_should_use_gpu() returns false when GPU unavailable
 */

#ifndef NIMCP_BRIDGE_GPU_H
#define NIMCP_BRIDGE_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Minimum data size (in elements) to justify GPU kernel launch overhead
 *
 * GPU kernels have launch overhead (~5-20 microseconds). For small data sizes,
 * CPU processing may be faster. This threshold balances launch overhead against
 * GPU parallelism benefits.
 *
 * Typical values:
 * - 1024: Conservative, ensures GPU always wins
 * - 512:  Moderate, good for most operations
 * - 256:  Aggressive, for very parallel operations
 */
#define BRIDGE_GPU_MIN_ELEMENTS_THRESHOLD 1024

/**
 * @brief Default initial work buffer size (in bytes)
 */
#define BRIDGE_GPU_DEFAULT_BUFFER_SIZE (1024 * 1024)  // 1 MB

//=============================================================================
// Bridge GPU Context Structure
//=============================================================================

/**
 * @brief GPU context for bridge modules
 *
 * WHAT: Encapsulates GPU resources needed by bridge modules
 * WHY:  Provides consistent GPU access pattern across all bridges
 * HOW:  Wraps GPU context from brain with bridge-specific buffer management
 *
 * USAGE:
 * @code
 * // In bridge create function:
 * my_bridge->gpu_ctx = bridge_gpu_context_create(brain);
 *
 * // In bridge update function:
 * if (bridge_should_use_gpu(my_bridge->gpu_ctx, num_elements)) {
 *     // Use GPU path
 *     bridge_gpu_ensure_buffer(my_bridge->gpu_ctx, num_elements * sizeof(float));
 *     // ... launch kernels using gpu_ctx->stream ...
 *     bridge_gpu_sync(my_bridge->gpu_ctx);
 * } else {
 *     // Use CPU path
 * }
 *
 * // In bridge destroy function:
 * bridge_gpu_context_destroy(my_bridge->gpu_ctx);
 * @endcode
 */
typedef struct bridge_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;      /**< GPU context (NULL = CPU mode) */
    void* work_buffer;                  /**< Reusable GPU work buffer */
    void* stream;                       /**< Async stream (cudaStream_t or equivalent) */
    bool gpu_available;                 /**< Runtime GPU availability status */
    size_t work_buffer_size;           /**< Current work buffer size in bytes */
} bridge_gpu_context_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create GPU context for a bridge, extracting from brain's GPU context
 *
 * WHAT: Creates bridge-specific GPU context from brain's GPU resources
 * WHY:  Bridges need GPU access but shouldn't manage GPU lifecycle
 * HOW:  Extracts GPU context from brain, creates dedicated stream for bridge
 *
 * @param brain Brain handle to extract GPU context from
 * @return GPU context on success, NULL-safe context on failure (CPU mode)
 *
 * THREAD SAFETY: NOT thread-safe (create from single thread)
 *
 * NOTE: Returns a valid struct even if GPU unavailable - gpu_available will
 *       be false, allowing seamless CPU fallback without NULL checks.
 *
 * EXAMPLE:
 * @code
 * bridge_gpu_context_t* gpu_ctx = bridge_gpu_context_create(brain);
 * if (gpu_ctx->gpu_available) {
 *     LOG_INFO("GPU acceleration enabled for bridge");
 * } else {
 *     LOG_INFO("Running bridge in CPU mode");
 * }
 * @endcode
 */
NIMCP_EXPORT bridge_gpu_context_t* bridge_gpu_context_create(brain_t brain);

/**
 * @brief Destroy GPU context and free all resources
 *
 * WHAT: Releases bridge GPU resources
 * WHY:  Proper cleanup prevents memory leaks
 * HOW:  Frees work buffer, does NOT destroy brain's GPU context
 *
 * @param ctx Context to destroy (can be NULL - no-op)
 *
 * NOTE: Does NOT destroy the brain's GPU context - only bridge-specific
 *       resources (work buffer, stream if created separately).
 */
NIMCP_EXPORT void bridge_gpu_context_destroy(bridge_gpu_context_t* ctx);

//=============================================================================
// Decision API
//=============================================================================

/**
 * @brief Check if GPU should be used for given data size
 *
 * WHAT: Determines whether GPU or CPU path is optimal
 * WHY:  GPU has kernel launch overhead; small data is faster on CPU
 * HOW:  Checks GPU availability AND data size vs threshold
 *
 * @param ctx Bridge GPU context
 * @param data_size Number of elements to process (not bytes)
 * @return true if GPU should be used, false for CPU path
 *
 * LOGIC:
 * - Returns false if ctx is NULL
 * - Returns false if gpu_available is false
 * - Returns false if data_size < BRIDGE_GPU_MIN_ELEMENTS_THRESHOLD
 * - Otherwise returns true
 *
 * EXAMPLE:
 * @code
 * if (bridge_should_use_gpu(ctx, num_neurons)) {
 *     gpu_compute_activations(ctx, neurons, num_neurons);
 * } else {
 *     cpu_compute_activations(neurons, num_neurons);
 * }
 * @endcode
 */
NIMCP_EXPORT bool bridge_should_use_gpu(bridge_gpu_context_t* ctx, size_t data_size);

//=============================================================================
// Buffer Management API
//=============================================================================

/**
 * @brief Ensure work buffer is at least the given size
 *
 * WHAT: Ensures GPU work buffer has sufficient capacity
 * WHY:  Reusing buffers avoids allocation overhead in hot paths
 * HOW:  Reallocates if current buffer too small, preserves if adequate
 *
 * @param ctx Bridge GPU context
 * @param min_size Minimum required size in bytes
 * @return true on success (buffer ready), false on failure
 *
 * NOTE: Does NOT preserve buffer contents on reallocation. If you need
 *       the old data, copy it before calling this function.
 *
 * EXAMPLE:
 * @code
 * size_t needed = num_elements * sizeof(float);
 * if (!bridge_gpu_ensure_buffer(ctx, needed)) {
 *     LOG_ERROR("Failed to allocate GPU work buffer");
 *     return -1;
 * }
 * // ctx->work_buffer now has at least 'needed' bytes
 * @endcode
 */
NIMCP_EXPORT bool bridge_gpu_ensure_buffer(bridge_gpu_context_t* ctx, size_t min_size);

//=============================================================================
// Synchronization API
//=============================================================================

/**
 * @brief Sync GPU operations (wait for stream to complete)
 *
 * WHAT: Blocks until all GPU operations on bridge's stream complete
 * WHY:  Ensures GPU results are ready before CPU access
 * HOW:  Calls cudaStreamSynchronize on bridge's stream
 *
 * @param ctx Bridge GPU context
 *
 * THREAD SAFETY: Thread-safe (synchronizes specific stream)
 *
 * NOTE: No-op if ctx is NULL or GPU not available. Safe to call in all cases.
 *
 * EXAMPLE:
 * @code
 * // Launch async GPU operations
 * launch_kernel<<<grid, block, 0, ctx->stream>>>(data, n);
 *
 * // Wait for completion before reading results
 * bridge_gpu_sync(ctx);
 *
 * // Now safe to read from GPU memory
 * @endcode
 */
NIMCP_EXPORT void bridge_gpu_sync(bridge_gpu_context_t* ctx);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Check if GPU context is valid and GPU is available
 *
 * @param ctx Bridge GPU context
 * @return true if ctx is non-NULL and gpu_available is true
 */
static inline bool bridge_gpu_is_available(const bridge_gpu_context_t* ctx) {
    return ctx != NULL && ctx->gpu_available;
}

/**
 * @brief Get the underlying GPU context for direct CUDA API calls
 *
 * @param ctx Bridge GPU context
 * @return nimcp_gpu_context_t pointer, or NULL if unavailable
 */
static inline nimcp_gpu_context_t* bridge_gpu_get_context(bridge_gpu_context_t* ctx) {
    return ctx ? ctx->gpu_ctx : NULL;
}

/**
 * @brief Get the stream for kernel launches
 *
 * @param ctx Bridge GPU context
 * @return Stream handle (cudaStream_t or equivalent), NULL if unavailable
 */
static inline void* bridge_gpu_get_stream(bridge_gpu_context_t* ctx) {
    return ctx ? ctx->stream : NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRIDGE_GPU_H */
