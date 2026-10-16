/**
 * @file nimcp_gpu_context.h
 * @brief GPU Context Manager for CUDA Kernel Execution
 *
 * WHAT: Unified GPU context for managing CUDA resources
 * WHY:  Centralizes GPU device, stream, and handle management
 * HOW:  Wraps CUDA runtime, cuBLAS, cuFFT handles in one context
 *
 * ARCHITECTURE:
 *
 *   +----------------------------------------------------------+
 *   |                  GPU CONTEXT MANAGER                      |
 *   |                                                          |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |  |   Device     |  |   Streams    |  |   Handles    |    |
 *   |  | Management   |  | (Compute/    |  | (cuBLAS,     |    |
 *   |  | (Selection)  |  |  Transfer)   |  |  cuFFT)      |    |
 *   |  +--------------+  +--------------+  +--------------+    |
 *   |                           |                              |
 *   |                  +------------------+                    |
 *   |                  |  Memory Manager  |                    |
 *   |                  | (Alloc/Free/Copy)|                    |
 *   |                  +------------------+                    |
 *   +----------------------------------------------------------+
 *
 * THREAD SAFETY:
 * - Context creation/destruction NOT thread-safe
 * - Memory operations thread-safe within single context
 * - Multiple contexts can be used from different threads
 *
 * CPU FALLBACK:
 * - All functions work without CUDA (return errors or use CPU path)
 * - Context creation returns NULL without CUDA
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_GPU_CONTEXT_H
#define NIMCP_GPU_CONTEXT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "gpu/execution/nimcp_gpu_detect.h"
#include "gpu/nimcp_execution_mode.h"

//=============================================================================
// Forward Declarations (CUDA types - opaque when not compiling with CUDA)
//=============================================================================
// NOTE: CUDA headers MUST be included BEFORE extern "C" block because they
// contain C++ code (operator overloads in cuda_bf16.hpp etc.) that cannot
// have C linkage.

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cufft.h>

typedef cudaStream_t nimcp_cuda_stream_t;
typedef cublasHandle_t nimcp_cublas_handle_t;
typedef cufftHandle nimcp_cufft_handle_t;
#else
typedef void* nimcp_cuda_stream_t;
typedef void* nimcp_cublas_handle_t;
typedef int nimcp_cufft_handle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// GPU Memory Transfer Direction
//=============================================================================

/**
 * @brief Memory transfer direction
 */
typedef enum {
    GPU_MEMCPY_HOST_TO_DEVICE = 0,   /**< Host to device transfer */
    GPU_MEMCPY_DEVICE_TO_HOST = 1,   /**< Device to host transfer */
    GPU_MEMCPY_DEVICE_TO_DEVICE = 2, /**< Device to device transfer */
    GPU_MEMCPY_HOST_TO_HOST = 3,     /**< Host to host transfer */
} nimcp_gpu_memcpy_kind_t;

//=============================================================================
// GPU Context Structure
//=============================================================================

/**
 * @brief GPU context for managing CUDA resources
 *
 * WHAT: Encapsulates all GPU resources needed for kernel execution
 * WHY:  Simplifies resource management and enables multi-GPU support
 * HOW:  Manages device, streams, library handles, and memory tracking
 */
typedef struct nimcp_gpu_context_s {
    // Device management
    int device_id;                      /**< CUDA device ID (0-based) */
    gpu_device_info_t device_info;      /**< Device capabilities and info */
    bool initialized;                   /**< Context initialized flag */

    // CUDA streams
    nimcp_cuda_stream_t compute_stream;  /**< Stream for compute operations */
    nimcp_cuda_stream_t transfer_stream; /**< Stream for memory transfers */
    nimcp_cuda_stream_t aux_stream;      /**< Auxiliary stream for overlapping */

    // Stream pool for concurrent kernel execution
    #define NIMCP_GPU_STREAM_POOL_SIZE 8
    nimcp_cuda_stream_t stream_pool[8];  /**< Pool of streams for parallel kernels */
    uint32_t stream_pool_count;          /**< Number of streams created in pool */
    volatile uint32_t stream_pool_next;  /**< Round-robin index for stream selection (use __atomic builtins) */

    // Library handles
    nimcp_cublas_handle_t cublas_handle; /**< cuBLAS handle for BLAS ops */
    nimcp_cufft_handle_t cufft_plan_1d;  /**< cuFFT plan for 1D transforms */
    nimcp_cufft_handle_t cufft_plan_2d;  /**< cuFFT plan for 2D transforms */
    bool cublas_initialized;             /**< cuBLAS handle valid */
    bool cufft_initialized;              /**< cuFFT plans valid */
    volatile int cufft_creating;         /**< Per-context spinlock for cuFFT plan creation */

    // Memory tracking
    size_t total_memory;                /**< Total GPU memory (bytes) */
    size_t allocated_memory;            /**< Currently allocated (bytes) */
    size_t peak_memory;                 /**< Peak memory usage (bytes) */
    uint64_t allocation_count;          /**< Number of allocations */
    uint64_t deallocation_count;        /**< Number of deallocations */

    // Kernel execution config
    uint32_t default_block_size;        /**< Default threads per block */
    uint32_t max_blocks_per_grid;       /**< Max blocks per grid dimension */
    uint32_t warp_size;                 /**< Warp size (32 for NVIDIA) */

    // Error tracking
    int last_error;                     /**< Last CUDA error code */
    char last_error_msg[256];           /**< Last error message */

    // Execution mode
    execution_mode_t execution_mode;    /**< Current execution mode */
} nimcp_gpu_context_t;

//=============================================================================
// Context Lifecycle API
//=============================================================================

/**
 * @brief Create GPU context for specified device
 *
 * WHAT: Creates and initializes GPU context with all handles
 * WHY:  Provides unified GPU resource management
 * HOW:  Initializes CUDA device, creates streams and library handles
 *
 * @param device_id CUDA device ID (0 for default, -1 for auto-select best)
 * @return GPU context on success, NULL on failure or no GPU
 *
 * THREAD SAFETY: NOT thread-safe (create from single thread)
 *
 * EXAMPLE:
 *   nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 *   if (ctx) {
 *       // Use GPU context
 *       nimcp_gpu_context_destroy(ctx);
 *   }
 */
NIMCP_EXPORT nimcp_gpu_context_t* nimcp_gpu_context_create(int device_id);

/**
 * @brief Create GPU context with auto-detected best device
 *
 * @return GPU context on success, NULL on failure or no GPU
 */
NIMCP_EXPORT nimcp_gpu_context_t* nimcp_gpu_context_create_auto(void);

/**
 * @brief Destroy GPU context and free all resources
 *
 * @param ctx Context to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_gpu_context_destroy(nimcp_gpu_context_t* ctx);

/**
 * @brief Check if GPU context is valid and initialized
 *
 * @param ctx Context to check
 * @return true if context is valid and initialized
 */
NIMCP_EXPORT bool nimcp_gpu_context_is_valid(const nimcp_gpu_context_t* ctx);

/**
 * @brief Get a stream from the pool for concurrent kernel execution
 *
 * WHAT: Returns next available stream from the round-robin pool
 * WHY:  Enables independent kernels to run concurrently on different streams
 * HOW:  Atomic increment of pool index, modulo pool size
 *
 * @param ctx GPU context
 * @return CUDA stream from pool, or compute_stream if pool not initialized
 */
NIMCP_EXPORT nimcp_cuda_stream_t nimcp_gpu_get_pool_stream(nimcp_gpu_context_t* ctx);

/**
 * @brief Synchronize all streams in the pool
 *
 * WHAT: Waits for all pool streams to finish
 * WHY:  Needed at synchronization points (end of forward pass, before backward)
 *
 * @param ctx GPU context
 */
NIMCP_EXPORT void nimcp_gpu_sync_pool(nimcp_gpu_context_t* ctx);

//=============================================================================
// Device Management API
//=============================================================================

/**
 * @brief Set current CUDA device to context's device
 *
 * @param ctx GPU context
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_gpu_context_set_device(nimcp_gpu_context_t* ctx);

/**
 * @brief Synchronize context (wait for all GPU operations to complete)
 *
 * @param ctx GPU context
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_gpu_context_synchronize(nimcp_gpu_context_t* ctx);

/**
 * @brief Get last CUDA error message
 *
 * @param ctx GPU context
 * @return Error message string (never NULL)
 */
NIMCP_EXPORT const char* nimcp_gpu_context_get_error(const nimcp_gpu_context_t* ctx);

//=============================================================================
// Memory Management API
//=============================================================================

/**
 * @brief Allocate GPU memory
 *
 * @param ctx GPU context
 * @param size_bytes Size in bytes
 * @return Device pointer on success, NULL on failure
 */
NIMCP_EXPORT void* nimcp_gpu_malloc(nimcp_gpu_context_t* ctx, size_t size_bytes);

/**
 * @brief Free GPU memory
 *
 * @param ctx GPU context
 * @param dev_ptr Device pointer to free
 */
NIMCP_EXPORT void nimcp_gpu_free(nimcp_gpu_context_t* ctx, void* dev_ptr);

/**
 * @brief Copy memory between host and device
 *
 * @param ctx GPU context
 * @param dst Destination pointer
 * @param src Source pointer
 * @param size_bytes Size in bytes
 * @param kind Transfer direction
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_gpu_memcpy(nimcp_gpu_context_t* ctx,
                                   void* dst, const void* src,
                                   size_t size_bytes,
                                   nimcp_gpu_memcpy_kind_t kind);

/**
 * @brief Async copy memory between host and device
 *
 * @param ctx GPU context
 * @param dst Destination pointer
 * @param src Source pointer
 * @param size_bytes Size in bytes
 * @param kind Transfer direction
 * @param use_transfer_stream Use transfer stream (true) or compute stream (false)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_gpu_memcpy_async(nimcp_gpu_context_t* ctx,
                                         void* dst, const void* src,
                                         size_t size_bytes,
                                         nimcp_gpu_memcpy_kind_t kind,
                                         bool use_transfer_stream);

/**
 * @brief Set GPU memory to value
 *
 * @param ctx GPU context
 * @param dev_ptr Device pointer
 * @param value Value to set (byte value)
 * @param size_bytes Size in bytes
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_gpu_memset(nimcp_gpu_context_t* ctx,
                                   void* dev_ptr, int value,
                                   size_t size_bytes);

/**
 * @brief Get memory usage statistics
 *
 * @param ctx GPU context
 * @param allocated_out Output: currently allocated bytes (can be NULL)
 * @param peak_out Output: peak allocation bytes (can be NULL)
 * @param free_out Output: free GPU memory bytes (can be NULL)
 */
NIMCP_EXPORT void nimcp_gpu_memory_stats(const nimcp_gpu_context_t* ctx,
                                          size_t* allocated_out,
                                          size_t* peak_out,
                                          size_t* free_out);

//=============================================================================
// Stream Management API
//=============================================================================

/**
 * @brief Get compute stream for kernel launches
 *
 * @param ctx GPU context
 * @return CUDA stream handle
 */
NIMCP_EXPORT nimcp_cuda_stream_t nimcp_gpu_get_compute_stream(nimcp_gpu_context_t* ctx);

/**
 * @brief Get transfer stream for async copies
 *
 * @param ctx GPU context
 * @return CUDA stream handle
 */
NIMCP_EXPORT nimcp_cuda_stream_t nimcp_gpu_get_transfer_stream(nimcp_gpu_context_t* ctx);

/**
 * @brief Synchronize compute stream
 *
 * @param ctx GPU context
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT int nimcp_gpu_stream_synchronize(nimcp_gpu_context_t* ctx);

//=============================================================================
// Library Handles API
//=============================================================================

/**
 * @brief Get cuBLAS handle for BLAS operations
 *
 * @param ctx GPU context
 * @return cuBLAS handle (NULL if not available)
 */
NIMCP_EXPORT nimcp_cublas_handle_t nimcp_gpu_get_cublas(nimcp_gpu_context_t* ctx);

/**
 * @brief Get cuFFT plan for 1D FFT operations
 *
 * @param ctx GPU context
 * @param n FFT size
 * @return cuFFT handle (0 if not available)
 */
NIMCP_EXPORT nimcp_cufft_handle_t nimcp_gpu_get_cufft_1d(nimcp_gpu_context_t* ctx, int n);

//=============================================================================
// Kernel Configuration API
//=============================================================================

/**
 * @brief Calculate optimal thread block size for kernel
 *
 * @param ctx GPU context
 * @param num_elements Number of elements to process
 * @param block_size_out Output: threads per block
 * @param grid_size_out Output: number of blocks
 */
NIMCP_EXPORT void nimcp_gpu_calc_launch_config(const nimcp_gpu_context_t* ctx,
                                                uint32_t num_elements,
                                                uint32_t* block_size_out,
                                                uint32_t* grid_size_out);

/**
 * @brief Get optimal block size for given kernel
 *
 * @param ctx GPU context
 * @param shared_mem_per_block Shared memory bytes per block
 * @return Optimal block size
 */
NIMCP_EXPORT uint32_t nimcp_gpu_get_optimal_block_size(const nimcp_gpu_context_t* ctx,
                                                        size_t shared_mem_per_block);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Print GPU context info to stdout
 *
 * @param ctx GPU context
 */
NIMCP_EXPORT void nimcp_gpu_context_print_info(const nimcp_gpu_context_t* ctx);

/**
 * @brief Get GPU context info as string
 *
 * @param ctx GPU context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT int nimcp_gpu_context_get_info_string(const nimcp_gpu_context_t* ctx,
                                                    char* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_CONTEXT_H */
