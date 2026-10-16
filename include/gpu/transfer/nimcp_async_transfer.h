/**
 * @file nimcp_async_transfer.h
 * @brief Async Transfer Manager with Double-Buffering and Pipeline Support
 *
 * WHAT: High-performance async memory transfer system for GPU operations
 * WHY:  Overlap compute with data transfer to maximize GPU utilization
 * HOW:  CUDA streams, events, double-buffering, and pipeline patterns
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                    ASYNC TRANSFER MANAGER                        |
 *   |                                                                  |
 *   |  +------------------+    +------------------+    +-------------+ |
 *   |  | Transfer Request |    | Double Buffer    |    | Pipeline    | |
 *   |  | Queue (pending)  |    | (compute/load)   |    | (stages)    | |
 *   |  +------------------+    +------------------+    +-------------+ |
 *   |           |                      |                      |        |
 *   |           v                      v                      v        |
 *   |  +------------------+    +------------------+    +-------------+ |
 *   |  | Stream Pool      |    | Event Sync       |    | Stage Exec  | |
 *   |  | (transfer_streams)|   | (transfer_done,  |    | (callbacks) | |
 *   |  +------------------+    |  compute_done)   |    +-------------+ |
 *   |                          +------------------+                    |
 *   |                                  |                               |
 *   |  +----------------------------------------------------------+   |
 *   |  |                   Pinned Memory Pool                     |   |
 *   |  |  (faster H2D/D2H transfers with page-locked memory)      |   |
 *   |  +----------------------------------------------------------+   |
 *   +------------------------------------------------------------------+
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe
 * - Uses atomic operations for request tracking
 * - Callbacks executed on background threads (async mode)
 *
 * PERFORMANCE:
 * - Pinned memory: 2-3x faster H2D/D2H transfers
 * - Double buffering: Overlaps compute and transfer
 * - Pipeline: Maximizes throughput for multi-stage processing
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_ASYNC_TRANSFER_H
#define NIMCP_ASYNC_TRANSFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum number of transfer streams in pool */
#define NIMCP_TRANSFER_MAX_STREAMS 8

/** @brief Maximum pending transfer requests */
#define NIMCP_TRANSFER_MAX_PENDING 256

/** @brief Maximum pipeline stages */
#define NIMCP_PIPELINE_MAX_STAGES 16

/** @brief Invalid request ID */
#define NIMCP_TRANSFER_INVALID_ID UINT64_MAX

/** @brief Default pinned pool size (64 MB) */
#define NIMCP_TRANSFER_DEFAULT_PINNED_SIZE (64 * 1024 * 1024)

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Transfer direction
 */
typedef enum {
    NIMCP_TRANSFER_H2D = 0,   /**< Host to Device */
    NIMCP_TRANSFER_D2H = 1,   /**< Device to Host */
    NIMCP_TRANSFER_D2D = 2,   /**< Device to Device (same GPU) */
    NIMCP_TRANSFER_P2P = 3    /**< Peer-to-peer (multi-GPU) */
} nimcp_transfer_direction_t;

/**
 * @brief Transfer request status
 */
typedef enum {
    NIMCP_TRANSFER_STATUS_PENDING = 0,    /**< Queued, not started */
    NIMCP_TRANSFER_STATUS_IN_PROGRESS,    /**< Transfer in progress */
    NIMCP_TRANSFER_STATUS_COMPLETED,      /**< Transfer completed successfully */
    NIMCP_TRANSFER_STATUS_FAILED,         /**< Transfer failed */
    NIMCP_TRANSFER_STATUS_CANCELLED       /**< Transfer cancelled */
} nimcp_transfer_status_t;

/**
 * @brief Transfer completion callback
 *
 * @param user_data User-provided context
 */
typedef void (*nimcp_transfer_callback_t)(void* user_data);

/**
 * @brief Pipeline stage execution function
 *
 * @param input Input buffer (device memory)
 * @param output Output buffer (device memory)
 * @param params Stage-specific parameters
 */
typedef void (*nimcp_pipeline_execute_fn)(void* input, void* output, void* params);

//=============================================================================
// Transfer Request Structure
//=============================================================================

/**
 * @brief Transfer request (command pattern)
 *
 * WHAT: Represents a single async memory transfer operation
 * WHY:  Enables queueing, tracking, and callback on completion
 * HOW:  Stores all transfer parameters and completion state
 */
typedef struct nimcp_transfer_request {
    void* host_ptr;                        /**< Host memory pointer */
    void* device_ptr;                      /**< Device memory pointer */
    size_t size;                           /**< Transfer size in bytes */
    nimcp_transfer_direction_t direction;  /**< Transfer direction */
#ifdef NIMCP_ENABLE_CUDA
    cudaStream_t stream;                   /**< CUDA stream for this transfer */
    cudaEvent_t completion_event;          /**< Event signaled on completion */
#else
    void* stream;
    void* completion_event;
#endif
    nimcp_transfer_callback_t callback;    /**< Completion callback */
    void* callback_data;                   /**< Callback user data */
    uint64_t request_id;                   /**< Unique request identifier */
    nimcp_transfer_status_t status;        /**< Current status */
    int error_code;                        /**< Error code if failed */
    uint64_t start_time_ns;                /**< Start timestamp */
    uint64_t end_time_ns;                  /**< End timestamp */
} nimcp_transfer_request_t;

//=============================================================================
// Double Buffer Structure
//=============================================================================

/**
 * @brief Double buffer for overlapping compute and transfer
 *
 * WHAT: Two device buffers that alternate between compute and loading
 * WHY:  Overlap data loading with GPU computation
 * HOW:  While one buffer is being computed, the other is loading next batch
 *
 * PATTERN:
 *   Frame N:   [Buffer A: COMPUTE] [Buffer B: LOAD]
 *   Frame N+1: [Buffer A: LOAD]    [Buffer B: COMPUTE]
 */
typedef struct nimcp_double_buffer {
    void* buffers[2];                      /**< Two device buffers */
    size_t buffer_size;                    /**< Size of each buffer */
    int active_index;                      /**< Buffer being computed (0 or 1) */
    int loading_index;                     /**< Buffer being loaded (0 or 1) */
#ifdef NIMCP_ENABLE_CUDA
    cudaStream_t compute_stream;           /**< Stream for compute operations */
    cudaStream_t transfer_stream;          /**< Stream for data transfers */
    cudaEvent_t transfer_done[2];          /**< Transfer completion events */
    cudaEvent_t compute_done[2];           /**< Compute completion events */
#else
    void* compute_stream;
    void* transfer_stream;
    void* transfer_done[2];
    void* compute_done[2];
#endif
    nimcp_gpu_context_t* ctx;              /**< GPU context */
    bool initialized;                       /**< Initialization flag */
    bool loading_in_progress;               /**< Load operation in progress */
} nimcp_double_buffer_t;

//=============================================================================
// Pipeline Structures
//=============================================================================

/**
 * @brief Pipeline stage
 *
 * WHAT: Single stage in a multi-stage pipeline
 * WHY:  Enable fine-grained parallelism across stages
 * HOW:  Each stage has its own stream, overlapping with other stages
 */
typedef struct nimcp_pipeline_stage {
    const char* name;                      /**< Stage name for debugging */
#ifdef NIMCP_ENABLE_CUDA
    cudaStream_t stream;                   /**< Stream for this stage */
    cudaEvent_t start_event;               /**< Stage start event */
    cudaEvent_t end_event;                 /**< Stage end event */
#else
    void* stream;
    void* start_event;
    void* end_event;
#endif
    nimcp_pipeline_execute_fn execute;     /**< Stage execution function */
    void* params;                          /**< Stage parameters */
    bool active;                           /**< Stage is active */
} nimcp_pipeline_stage_t;

/**
 * @brief Pipeline for multi-stage processing
 *
 * WHAT: Collection of stages that process data in sequence
 * WHY:  Maximize throughput by overlapping stages across batches
 * HOW:  Stage N processes batch M while Stage N+1 processes batch M-1
 */
typedef struct nimcp_pipeline {
    nimcp_pipeline_stage_t stages[NIMCP_PIPELINE_MAX_STAGES];
    int num_stages;                        /**< Number of active stages */
    nimcp_gpu_context_t* ctx;              /**< GPU context */
    bool initialized;                       /**< Initialization flag */
} nimcp_pipeline_t;

//=============================================================================
// Pinned Memory Pool
//=============================================================================

/**
 * @brief Pinned memory allocation entry
 */
typedef struct nimcp_pinned_alloc {
    void* ptr;                             /**< Pinned memory pointer */
    size_t size;                           /**< Allocation size */
    bool in_use;                           /**< Currently allocated */
    struct nimcp_pinned_alloc* next;       /**< Next in free list */
} nimcp_pinned_alloc_t;

//=============================================================================
// Transfer Manager Structure
//=============================================================================

/**
 * @brief Transfer manager (manages all async operations)
 *
 * WHAT: Central manager for async GPU memory transfers
 * WHY:  Unified interface for transfer scheduling and tracking
 * HOW:  Pool of streams, pending request queue, pinned memory pool
 */
typedef struct nimcp_transfer_manager {
    nimcp_gpu_context_t* ctx;              /**< GPU context */

    // Stream pool
#ifdef NIMCP_ENABLE_CUDA
    cudaStream_t transfer_streams[NIMCP_TRANSFER_MAX_STREAMS];
#else
    void* transfer_streams[NIMCP_TRANSFER_MAX_STREAMS];
#endif
    int num_streams;                       /**< Number of streams in pool */
    int next_stream;                       /**< Next stream to use (round-robin) */

    // Pending requests
    nimcp_transfer_request_t pending[NIMCP_TRANSFER_MAX_PENDING];
    size_t pending_count;                  /**< Number of pending requests */
    uint64_t next_request_id;              /**< Next request ID to assign */

    // Statistics
    size_t total_bytes_transferred;        /**< Total bytes transferred */
    size_t total_transfers;                /**< Total transfer count */
    double total_transfer_time_ms;         /**< Total transfer time */
    size_t h2d_bytes;                      /**< H2D bytes transferred */
    size_t d2h_bytes;                      /**< D2H bytes transferred */
    size_t d2d_bytes;                      /**< D2D bytes transferred */

    // Pinned memory pool
    void* pinned_pool;                     /**< Base of pinned memory pool */
    size_t pinned_pool_size;               /**< Total pool size */
    size_t pinned_pool_used;               /**< Bytes currently used */
    nimcp_pinned_alloc_t* pinned_allocs;   /**< Allocation tracking list */
    size_t pinned_alloc_count;             /**< Number of allocations */

    bool initialized;                       /**< Manager initialized */
} nimcp_transfer_manager_t;

//=============================================================================
// Transfer Manager API
//=============================================================================

/**
 * @brief Create transfer manager
 *
 * WHAT: Creates async transfer manager with stream pool and pinned memory
 * WHY:  Centralized management of async GPU transfers
 * HOW:  Allocates streams, creates events, optionally allocates pinned pool
 *
 * @param ctx GPU context (must be valid)
 * @param num_streams Number of transfer streams (1-8, clamped)
 * @param pinned_pool_size Pinned memory pool size (0 to disable)
 * @return Transfer manager or NULL on failure
 *
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 *   nimcp_transfer_manager_t* mgr = nimcp_transfer_manager_create(ctx, 4, 64*1024*1024);
 *   if (mgr) {
 *       // Use manager
 *       nimcp_transfer_manager_destroy(mgr);
 *   }
 */
NIMCP_EXPORT nimcp_transfer_manager_t* nimcp_transfer_manager_create(
    nimcp_gpu_context_t* ctx,
    int num_streams,
    size_t pinned_pool_size);

/**
 * @brief Destroy transfer manager
 *
 * @param mgr Transfer manager to destroy (can be NULL)
 *
 * NOTE: Waits for pending transfers to complete
 */
NIMCP_EXPORT void nimcp_transfer_manager_destroy(nimcp_transfer_manager_t* mgr);

/**
 * @brief Check if transfer manager is valid
 *
 * @param mgr Transfer manager
 * @return true if valid and initialized
 */
NIMCP_EXPORT bool nimcp_transfer_manager_is_valid(const nimcp_transfer_manager_t* mgr);

//=============================================================================
// Async Transfer Operations
//=============================================================================

/**
 * @brief Queue async transfer
 *
 * WHAT: Queues an async memory transfer operation
 * WHY:  Non-blocking transfer with optional callback
 * HOW:  Uses CUDA streams for async execution
 *
 * @param mgr Transfer manager
 * @param dst Destination pointer
 * @param src Source pointer
 * @param size Transfer size in bytes
 * @param direction Transfer direction
 * @param callback Completion callback (can be NULL)
 * @param callback_data Callback user data
 * @return Request ID or NIMCP_TRANSFER_INVALID_ID on failure
 *
 * THREAD SAFETY: Thread-safe
 *
 * EXAMPLE:
 *   uint64_t id = nimcp_transfer_async(mgr, d_ptr, h_ptr, size,
 *                                       NIMCP_TRANSFER_H2D, my_callback, data);
 *   // ... do other work ...
 *   nimcp_transfer_wait(mgr, id);
 */
NIMCP_EXPORT uint64_t nimcp_transfer_async(
    nimcp_transfer_manager_t* mgr,
    void* dst, const void* src, size_t size,
    nimcp_transfer_direction_t direction,
    nimcp_transfer_callback_t callback, void* callback_data);

/**
 * @brief Wait for specific transfer to complete
 *
 * @param mgr Transfer manager
 * @param request_id Request ID from nimcp_transfer_async
 *
 * NOTE: Blocks until transfer completes or times out
 */
NIMCP_EXPORT void nimcp_transfer_wait(nimcp_transfer_manager_t* mgr, uint64_t request_id);

/**
 * @brief Wait for all pending transfers to complete
 *
 * @param mgr Transfer manager
 */
NIMCP_EXPORT void nimcp_transfer_wait_all(nimcp_transfer_manager_t* mgr);

/**
 * @brief Check if transfer is complete
 *
 * @param mgr Transfer manager
 * @param request_id Request ID
 * @return true if completed (or invalid ID)
 */
NIMCP_EXPORT bool nimcp_transfer_is_complete(nimcp_transfer_manager_t* mgr, uint64_t request_id);

/**
 * @brief Get transfer status
 *
 * @param mgr Transfer manager
 * @param request_id Request ID
 * @return Transfer status
 */
NIMCP_EXPORT nimcp_transfer_status_t nimcp_transfer_get_status(
    nimcp_transfer_manager_t* mgr, uint64_t request_id);

/**
 * @brief Cancel pending transfer
 *
 * @param mgr Transfer manager
 * @param request_id Request ID
 * @return true if cancelled, false if already completed or not found
 */
NIMCP_EXPORT bool nimcp_transfer_cancel(nimcp_transfer_manager_t* mgr, uint64_t request_id);

/**
 * @brief Poll and process completed transfers (call callbacks)
 *
 * @param mgr Transfer manager
 * @return Number of transfers processed
 */
NIMCP_EXPORT int nimcp_transfer_poll(nimcp_transfer_manager_t* mgr);

//=============================================================================
// Pinned Memory API
//=============================================================================

/**
 * @brief Allocate pinned memory from pool
 *
 * WHAT: Allocates page-locked (pinned) host memory
 * WHY:  Pinned memory enables faster DMA transfers
 * HOW:  Uses cudaHostAlloc or pool allocation
 *
 * @param mgr Transfer manager
 * @param size Size in bytes
 * @return Pinned memory pointer or NULL on failure
 *
 * NOTE: If pool is exhausted, falls back to direct allocation
 */
NIMCP_EXPORT void* nimcp_transfer_alloc_pinned(nimcp_transfer_manager_t* mgr, size_t size);

/**
 * @brief Free pinned memory
 *
 * @param mgr Transfer manager
 * @param ptr Pointer to free (can be NULL)
 */
NIMCP_EXPORT void nimcp_transfer_free_pinned(nimcp_transfer_manager_t* mgr, void* ptr);

/**
 * @brief Get pinned pool statistics
 *
 * @param mgr Transfer manager
 * @param total_out Output: total pool size
 * @param used_out Output: currently used
 * @param alloc_count_out Output: allocation count
 */
NIMCP_EXPORT void nimcp_transfer_pinned_stats(
    const nimcp_transfer_manager_t* mgr,
    size_t* total_out, size_t* used_out, size_t* alloc_count_out);

//=============================================================================
// Transfer Statistics API
//=============================================================================

/**
 * @brief Get transfer statistics
 *
 * @param mgr Transfer manager
 * @param total_bytes Output: total bytes transferred
 * @param total_count Output: total transfer count
 * @param avg_time_ms Output: average transfer time (ms)
 */
NIMCP_EXPORT void nimcp_transfer_get_stats(
    const nimcp_transfer_manager_t* mgr,
    size_t* total_bytes, size_t* total_count, double* avg_time_ms);

/**
 * @brief Reset transfer statistics
 *
 * @param mgr Transfer manager
 */
NIMCP_EXPORT void nimcp_transfer_reset_stats(nimcp_transfer_manager_t* mgr);

//=============================================================================
// Double Buffer API
//=============================================================================

/**
 * @brief Create double buffer
 *
 * WHAT: Creates double buffer for overlapping compute and transfer
 * WHY:  Hide transfer latency by loading while computing
 * HOW:  Two GPU buffers with separate streams
 *
 * @param ctx GPU context
 * @param buffer_size Size of each buffer in bytes
 * @return Double buffer or NULL on failure
 *
 * EXAMPLE:
 *   nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, batch_size);
 *   for (int batch = 0; batch < num_batches; batch++) {
 *       // Start loading next batch while computing current
 *       if (batch + 1 < num_batches) {
 *           nimcp_double_buffer_start_load(db, next_batch_data, batch_size);
 *       }
 *       // Get buffer for compute
 *       void* compute_buf = nimcp_double_buffer_get_compute_buffer(db);
 *       // ... launch kernels on compute_buf ...
 *       // Swap buffers
 *       nimcp_double_buffer_swap(db);
 *   }
 *   nimcp_double_buffer_destroy(db);
 */
NIMCP_EXPORT nimcp_double_buffer_t* nimcp_double_buffer_create(
    nimcp_gpu_context_t* ctx, size_t buffer_size);

/**
 * @brief Destroy double buffer
 *
 * @param db Double buffer to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_double_buffer_destroy(nimcp_double_buffer_t* db);

/**
 * @brief Start loading data into loading buffer
 *
 * WHAT: Initiates async transfer into the loading buffer
 * WHY:  Load next batch while computing current
 * HOW:  Async cudaMemcpy on transfer stream
 *
 * @param db Double buffer
 * @param host_data Host data to load
 * @param size Size to transfer (must be <= buffer_size)
 * @return 0 on success, -1 on error
 *
 * NOTE: Non-blocking. Call nimcp_double_buffer_swap when compute is done.
 */
NIMCP_EXPORT int nimcp_double_buffer_start_load(
    nimcp_double_buffer_t* db,
    const void* host_data, size_t size);

/**
 * @brief Start loading with callback
 *
 * @param db Double buffer
 * @param host_data Host data to load
 * @param size Size to transfer
 * @param callback Completion callback
 * @param user_data Callback user data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_double_buffer_start_load_callback(
    nimcp_double_buffer_t* db,
    const void* host_data, size_t size,
    nimcp_transfer_callback_t callback, void* user_data);

/**
 * @brief Get buffer ready for compute
 *
 * WHAT: Returns pointer to buffer ready for GPU computation
 * WHY:  Get the buffer that has finished loading
 * HOW:  Returns active buffer (which was previously loaded)
 *
 * @param db Double buffer
 * @return Device pointer to compute buffer
 *
 * NOTE: Waits if previous compute on this buffer not complete
 */
NIMCP_EXPORT void* nimcp_double_buffer_get_compute_buffer(nimcp_double_buffer_t* db);

/**
 * @brief Wait for loading buffer to be ready
 *
 * @param db Double buffer
 * @return Device pointer to loading buffer (after transfer completes)
 */
NIMCP_EXPORT void* nimcp_double_buffer_wait_load(nimcp_double_buffer_t* db);

/**
 * @brief Swap buffers after compute completes
 *
 * WHAT: Swaps active and loading buffers
 * WHY:  Prepare for next iteration
 * HOW:  Signals compute done, swaps indices
 *
 * @param db Double buffer
 *
 * NOTE: Call after kernel launch on compute buffer
 */
NIMCP_EXPORT void nimcp_double_buffer_swap(nimcp_double_buffer_t* db);

/**
 * @brief Synchronize both buffers (wait for all operations)
 *
 * @param db Double buffer
 */
NIMCP_EXPORT void nimcp_double_buffer_sync(nimcp_double_buffer_t* db);

/**
 * @brief Get compute stream for launching kernels
 *
 * @param db Double buffer
 * @return CUDA stream for compute operations
 */
NIMCP_EXPORT nimcp_cuda_stream_t nimcp_double_buffer_get_compute_stream(
    nimcp_double_buffer_t* db);

/**
 * @brief Get transfer stream
 *
 * @param db Double buffer
 * @return CUDA stream for transfer operations
 */
NIMCP_EXPORT nimcp_cuda_stream_t nimcp_double_buffer_get_transfer_stream(
    nimcp_double_buffer_t* db);

/**
 * @brief Check if load is in progress
 *
 * @param db Double buffer
 * @return true if loading
 */
NIMCP_EXPORT bool nimcp_double_buffer_is_loading(const nimcp_double_buffer_t* db);

//=============================================================================
// Pipeline API
//=============================================================================

/**
 * @brief Create pipeline
 *
 * WHAT: Creates a multi-stage processing pipeline
 * WHY:  Maximize throughput via stage overlapping
 * HOW:  Each stage has its own stream for parallel execution
 *
 * @param ctx GPU context
 * @param num_stages Maximum number of stages
 * @return Pipeline or NULL on failure
 */
NIMCP_EXPORT nimcp_pipeline_t* nimcp_pipeline_create(nimcp_gpu_context_t* ctx, int num_stages);

/**
 * @brief Destroy pipeline
 *
 * @param pipeline Pipeline to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_pipeline_destroy(nimcp_pipeline_t* pipeline);

/**
 * @brief Add stage to pipeline
 *
 * WHAT: Adds a processing stage to the pipeline
 * WHY:  Define pipeline structure
 * HOW:  Creates stream and events for stage
 *
 * @param pipeline Pipeline
 * @param name Stage name (for debugging)
 * @param execute Stage execution function
 * @param params Stage parameters (passed to execute)
 * @return Stage index or -1 on failure
 */
NIMCP_EXPORT int nimcp_pipeline_add_stage(
    nimcp_pipeline_t* pipeline,
    const char* name,
    nimcp_pipeline_execute_fn execute,
    void* params);

/**
 * @brief Get number of stages
 *
 * @param pipeline Pipeline
 * @return Number of stages
 */
NIMCP_EXPORT int nimcp_pipeline_get_stage_count(const nimcp_pipeline_t* pipeline);

/**
 * @brief Get stage by index
 *
 * @param pipeline Pipeline
 * @param index Stage index
 * @return Stage pointer or NULL
 */
NIMCP_EXPORT nimcp_pipeline_stage_t* nimcp_pipeline_get_stage(
    nimcp_pipeline_t* pipeline, int index);

/**
 * @brief Execute pipeline on batches
 *
 * WHAT: Executes all pipeline stages on multiple batches
 * WHY:  Process multiple batches with overlapping stages
 * HOW:  Stage N processes batch M while Stage N+1 processes batch M-1
 *
 * @param pipeline Pipeline
 * @param inputs Array of input buffers (one per batch)
 * @param outputs Array of output buffers (one per batch)
 * @param num_batches Number of batches to process
 *
 * COMPLEXITY: O(num_batches * num_stages)
 *
 * EXAMPLE:
 *   void** inputs = allocate_input_buffers(num_batches);
 *   void** outputs = allocate_output_buffers(num_batches);
 *   nimcp_pipeline_execute(pipeline, inputs, outputs, num_batches);
 */
NIMCP_EXPORT void nimcp_pipeline_execute(
    nimcp_pipeline_t* pipeline,
    void** inputs, void** outputs, int num_batches);

/**
 * @brief Execute single batch through pipeline
 *
 * @param pipeline Pipeline
 * @param input Input buffer
 * @param output Output buffer
 */
NIMCP_EXPORT void nimcp_pipeline_execute_single(
    nimcp_pipeline_t* pipeline,
    void* input, void* output);

/**
 * @brief Synchronize pipeline (wait for all stages)
 *
 * @param pipeline Pipeline
 */
NIMCP_EXPORT void nimcp_pipeline_sync(nimcp_pipeline_t* pipeline);

/**
 * @brief Reset pipeline for next run
 *
 * @param pipeline Pipeline
 */
NIMCP_EXPORT void nimcp_pipeline_reset(nimcp_pipeline_t* pipeline);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get transfer direction name
 *
 * @param direction Transfer direction
 * @return Direction name string
 */
NIMCP_EXPORT const char* nimcp_transfer_direction_name(nimcp_transfer_direction_t direction);

/**
 * @brief Get transfer status name
 *
 * @param status Transfer status
 * @return Status name string
 */
NIMCP_EXPORT const char* nimcp_transfer_status_name(nimcp_transfer_status_t status);

/**
 * @brief Print transfer manager info
 *
 * @param mgr Transfer manager
 */
NIMCP_EXPORT void nimcp_transfer_manager_print_info(const nimcp_transfer_manager_t* mgr);

/**
 * @brief Print double buffer info
 *
 * @param db Double buffer
 */
NIMCP_EXPORT void nimcp_double_buffer_print_info(const nimcp_double_buffer_t* db);

/**
 * @brief Print pipeline info
 *
 * @param pipeline Pipeline
 */
NIMCP_EXPORT void nimcp_pipeline_print_info(const nimcp_pipeline_t* pipeline);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASYNC_TRANSFER_H */
