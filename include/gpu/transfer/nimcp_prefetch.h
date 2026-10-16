/**
 * @file nimcp_prefetch.h
 * @brief Data Prefetching Manager for Training Data Loading
 *
 * WHAT: Prefetching system for overlapping data loading with GPU training
 * WHY:  Hide I/O latency by loading next batch while GPU processes current
 * HOW:  Background thread, double-buffered tensors, async transfers
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                    PREFETCH MANAGER                              |
 *   |                                                                  |
 *   |  +------------------+    +----------------------+                |
 *   |  | Data Loader      |    | Double Buffers       |                |
 *   |  | (background      |    | (per-tensor)         |                |
 *   |  |  thread)         |    +----------------------+                |
 *   |  +------------------+             |                              |
 *   |           |                       v                              |
 *   |           v              +------------------+                    |
 *   |  +------------------+    | Transfer Manager |                    |
 *   |  | Pinned Staging   |    | (async H2D)      |                    |
 *   |  | Buffers          |    +------------------+                    |
 *   |  +------------------+             |                              |
 *   |                                   v                              |
 *   |                          +------------------+                    |
 *   |                          | GPU Buffers      |                    |
 *   |                          | (ready for use)  |                    |
 *   |                          +------------------+                    |
 *   +------------------------------------------------------------------+
 *
 * USAGE PATTERN:
 *
 *   // 1. Create prefetch manager
 *   size_t sizes[] = {input_size, target_size};
 *   nimcp_prefetch_manager_t* mgr = nimcp_prefetch_create(ctx, sizes, 2);
 *
 *   // 2. Start prefetching with data loader callback
 *   nimcp_prefetch_start(mgr, my_data_loader, user_data);
 *
 *   // 3. Training loop
 *   for (int batch = 0; batch < num_batches; batch++) {
 *       // Get current batch (blocks if not ready)
 *       void** batch_ptrs = nimcp_prefetch_get_batch(mgr);
 *       void* input = batch_ptrs[0];
 *       void* target = batch_ptrs[1];
 *
 *       // ... train on batch ...
 *
 *       // Release batch for next prefetch
 *       nimcp_prefetch_release_batch(mgr);
 *   }
 *
 *   // 4. Cleanup
 *   nimcp_prefetch_stop(mgr);
 *   nimcp_prefetch_destroy(mgr);
 *
 * THREAD SAFETY:
 * - get_batch/release_batch are thread-safe
 * - Single consumer pattern (one training thread)
 * - Data loader callback must be thread-safe
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_PREFETCH_H
#define NIMCP_PREFETCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>
#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/transfer/nimcp_async_transfer.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum number of tensors per batch */
#define NIMCP_PREFETCH_MAX_TENSORS 16

/** @brief Default queue depth (batches to prefetch ahead) */
#define NIMCP_PREFETCH_DEFAULT_QUEUE_DEPTH 2

/** @brief Maximum queue depth */
#define NIMCP_PREFETCH_MAX_QUEUE_DEPTH 8

//=============================================================================
// Types and Enumerations
//=============================================================================

/**
 * @brief Prefetch state
 */
typedef enum {
    NIMCP_PREFETCH_STATE_IDLE = 0,     /**< Not started */
    NIMCP_PREFETCH_STATE_RUNNING,      /**< Actively prefetching */
    NIMCP_PREFETCH_STATE_PAUSED,       /**< Temporarily paused */
    NIMCP_PREFETCH_STATE_STOPPING,     /**< Shutting down */
    NIMCP_PREFETCH_STATE_STOPPED,      /**< Stopped */
    NIMCP_PREFETCH_STATE_ERROR         /**< Error occurred */
} nimcp_prefetch_state_t;

/**
 * @brief Data loader callback function
 *
 * WHAT: User-provided function to load batch data
 * WHY:  Decouple data loading from prefetch management
 * HOW:  Called by prefetch thread to get next batch
 *
 * @param batch_idx Current batch index (0-based)
 * @param tensor_idx Tensor index within batch (0-based)
 * @param size Expected size in bytes
 * @param user_data User context passed to nimcp_prefetch_start
 * @return Pointer to host data (must remain valid until next call for same tensor)
 *         NULL to indicate end of data or error
 *
 * NOTE: The returned pointer should point to pinned memory for best performance.
 *       If not pinned, the prefetch manager will copy through a staging buffer.
 */
typedef void* (*nimcp_data_loader_fn)(
    int batch_idx,
    int tensor_idx,
    size_t size,
    void* user_data);

/**
 * @brief Batch completion callback (optional)
 *
 * @param batch_idx Batch that completed prefetching
 * @param user_data User context
 */
typedef void (*nimcp_prefetch_callback_fn)(int batch_idx, void* user_data);

//=============================================================================
// Prefetch Buffer Structure
//=============================================================================

/**
 * @brief Per-tensor prefetch buffer
 */
typedef struct nimcp_prefetch_buffer {
    nimcp_double_buffer_t* double_buffer;  /**< Double buffer for this tensor */
    size_t tensor_size;                    /**< Size of this tensor in bytes */
    void* staging_buffer;                  /**< Pinned staging buffer (if needed) */
    bool use_staging;                      /**< Using staging buffer */
} nimcp_prefetch_buffer_t;

//=============================================================================
// Batch Slot Structure
//=============================================================================

/**
 * @brief Batch slot in the prefetch queue
 */
typedef struct nimcp_prefetch_slot {
    void* tensor_ptrs[NIMCP_PREFETCH_MAX_TENSORS];  /**< GPU pointers for this batch */
    int batch_idx;                          /**< Batch index */
    bool ready;                             /**< Data is ready for consumption */
    bool in_use;                            /**< Batch is being used */
    bool loading;                           /**< Currently loading */
} nimcp_prefetch_slot_t;

//=============================================================================
// Prefetch Manager Structure
//=============================================================================

/**
 * @brief Prefetch manager for training data
 *
 * WHAT: Manages background prefetching of training batches
 * WHY:  Overlap data loading with GPU computation
 * HOW:  Background thread, double buffers, circular queue
 */
typedef struct nimcp_prefetch_manager {
    // GPU context
    nimcp_gpu_context_t* ctx;              /**< GPU context */
    nimcp_transfer_manager_t* transfer_mgr; /**< Transfer manager */
    bool owns_transfer_mgr;                /**< Did we create transfer_mgr? */

    // Tensor configuration
    nimcp_prefetch_buffer_t buffers[NIMCP_PREFETCH_MAX_TENSORS];
    int num_tensors;                       /**< Number of tensors per batch */
    size_t* tensor_sizes;                  /**< Copy of tensor sizes */

    // Batch queue
    nimcp_prefetch_slot_t queue[NIMCP_PREFETCH_MAX_QUEUE_DEPTH];
    int queue_depth;                       /**< Queue capacity */
    int queue_head;                        /**< Next slot to produce */
    int queue_tail;                        /**< Next slot to consume */
    int queue_count;                       /**< Current items in queue */

    // Data loader
    nimcp_data_loader_fn data_loader;      /**< User data loader */
    void* loader_user_data;                /**< User data for loader */

    // Callbacks
    nimcp_prefetch_callback_fn on_batch_ready;  /**< Called when batch ready */
    void* callback_user_data;              /**< User data for callback */

    // Background thread
    pthread_t loader_thread;               /**< Background loader thread */
    nimcp_mutex_t mutex;                 /**< Synchronization mutex */
    pthread_cond_t cond_producer;          /**< Producer condition (slot available) */
    pthread_cond_t cond_consumer;          /**< Consumer condition (batch ready) */
    bool thread_created;                   /**< Thread was created */

    // State
    nimcp_prefetch_state_t state;          /**< Current state */
    int current_batch_idx;                 /**< Current batch being loaded */
    int total_batches;                     /**< Total batches (-1 for infinite) */
    bool running;                          /**< Prefetch loop running */

    // Statistics
    uint64_t batches_loaded;               /**< Total batches loaded */
    uint64_t batches_consumed;             /**< Total batches consumed */
    uint64_t stalls;                       /**< Consumer stalls (had to wait) */
    uint64_t total_load_time_ns;           /**< Total load time */
    uint64_t total_wait_time_ns;           /**< Total consumer wait time */

    bool initialized;                       /**< Manager initialized */
} nimcp_prefetch_manager_t;

//=============================================================================
// Prefetch Manager API
//=============================================================================

/**
 * @brief Create prefetch manager
 *
 * WHAT: Creates prefetch manager for multi-tensor batches
 * WHY:  Unified prefetching for training data
 * HOW:  Allocates double buffers for each tensor
 *
 * @param ctx GPU context
 * @param tensor_sizes Array of tensor sizes (bytes per tensor)
 * @param num_tensors Number of tensors per batch
 * @return Prefetch manager or NULL on failure
 *
 * THREAD SAFETY: Not thread-safe (call from single thread)
 *
 * EXAMPLE:
 *   // For input and target tensors
 *   size_t sizes[2] = {input_size, target_size};
 *   nimcp_prefetch_manager_t* mgr = nimcp_prefetch_create(ctx, sizes, 2);
 */
NIMCP_EXPORT nimcp_prefetch_manager_t* nimcp_prefetch_create(
    nimcp_gpu_context_t* ctx,
    const size_t* tensor_sizes,
    int num_tensors);

/**
 * @brief Create prefetch manager with options
 *
 * @param ctx GPU context
 * @param tensor_sizes Array of tensor sizes
 * @param num_tensors Number of tensors per batch
 * @param queue_depth Number of batches to prefetch ahead
 * @param transfer_mgr Optional transfer manager (NULL to create new)
 * @return Prefetch manager or NULL on failure
 */
NIMCP_EXPORT nimcp_prefetch_manager_t* nimcp_prefetch_create_ex(
    nimcp_gpu_context_t* ctx,
    const size_t* tensor_sizes,
    int num_tensors,
    int queue_depth,
    nimcp_transfer_manager_t* transfer_mgr);

/**
 * @brief Destroy prefetch manager
 *
 * @param mgr Prefetch manager (can be NULL)
 *
 * NOTE: Stops prefetching if running
 */
NIMCP_EXPORT void nimcp_prefetch_destroy(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Check if prefetch manager is valid
 *
 * @param mgr Prefetch manager
 * @return true if valid and initialized
 */
NIMCP_EXPORT bool nimcp_prefetch_is_valid(const nimcp_prefetch_manager_t* mgr);

//=============================================================================
// Prefetch Control API
//=============================================================================

/**
 * @brief Start prefetching
 *
 * WHAT: Starts background prefetch thread
 * WHY:  Begin loading data ahead of consumption
 * HOW:  Creates thread that calls data_loader
 *
 * @param mgr Prefetch manager
 * @param data_loader User function to load batch data
 * @param user_data Context passed to data_loader
 * @return 0 on success, -1 on failure
 *
 * NOTE: data_loader is called for each (batch, tensor) pair
 *
 * EXAMPLE:
 *   void* my_loader(int batch, int tensor, size_t size, void* ctx) {
 *       MyDataset* ds = (MyDataset*)ctx;
 *       return ds->get_tensor(batch, tensor);
 *   }
 *   nimcp_prefetch_start(mgr, my_loader, dataset);
 */
NIMCP_EXPORT int nimcp_prefetch_start(
    nimcp_prefetch_manager_t* mgr,
    nimcp_data_loader_fn data_loader,
    void* user_data);

/**
 * @brief Start with total batch count
 *
 * @param mgr Prefetch manager
 * @param data_loader User function to load batch data
 * @param user_data Context passed to data_loader
 * @param total_batches Total batches to load (-1 for infinite)
 * @return 0 on success, -1 on failure
 */
NIMCP_EXPORT int nimcp_prefetch_start_with_count(
    nimcp_prefetch_manager_t* mgr,
    nimcp_data_loader_fn data_loader,
    void* user_data,
    int total_batches);

/**
 * @brief Stop prefetching
 *
 * @param mgr Prefetch manager
 *
 * NOTE: Blocks until background thread exits
 */
NIMCP_EXPORT void nimcp_prefetch_stop(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Pause prefetching
 *
 * @param mgr Prefetch manager
 */
NIMCP_EXPORT void nimcp_prefetch_pause(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Resume prefetching
 *
 * @param mgr Prefetch manager
 */
NIMCP_EXPORT void nimcp_prefetch_resume(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Reset for new epoch
 *
 * @param mgr Prefetch manager
 *
 * NOTE: Clears queue and resets batch index to 0
 */
NIMCP_EXPORT void nimcp_prefetch_reset(nimcp_prefetch_manager_t* mgr);

//=============================================================================
// Batch Access API
//=============================================================================

/**
 * @brief Get next batch for processing
 *
 * WHAT: Returns GPU pointers for next batch of tensors
 * WHY:  Consume prefetched data
 * HOW:  Waits if batch not ready, returns GPU pointers
 *
 * @param mgr Prefetch manager
 * @return Array of GPU pointers (one per tensor), or NULL on error/end
 *
 * THREAD SAFETY: Thread-safe (but single consumer pattern recommended)
 *
 * NOTE: Must call nimcp_prefetch_release_batch after processing
 *
 * EXAMPLE:
 *   void** batch = nimcp_prefetch_get_batch(mgr);
 *   if (batch) {
 *       void* input = batch[0];
 *       void* target = batch[1];
 *       // ... process batch ...
 *       nimcp_prefetch_release_batch(mgr);
 *   }
 */
NIMCP_EXPORT void** nimcp_prefetch_get_batch(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Get batch with timeout
 *
 * @param mgr Prefetch manager
 * @param timeout_ms Maximum time to wait (0 = no wait, -1 = infinite)
 * @return Array of GPU pointers or NULL on timeout/error
 */
NIMCP_EXPORT void** nimcp_prefetch_get_batch_timeout(
    nimcp_prefetch_manager_t* mgr, int timeout_ms);

/**
 * @brief Try to get batch (non-blocking)
 *
 * @param mgr Prefetch manager
 * @return Array of GPU pointers or NULL if not ready
 */
NIMCP_EXPORT void** nimcp_prefetch_try_get_batch(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Release batch after processing
 *
 * WHAT: Signals that batch processing is complete
 * WHY:  Allow buffer reuse for next prefetch
 * HOW:  Frees slot for producer
 *
 * @param mgr Prefetch manager
 */
NIMCP_EXPORT void nimcp_prefetch_release_batch(nimcp_prefetch_manager_t* mgr);

/**
 * @brief Get current batch index
 *
 * @param mgr Prefetch manager
 * @return Current batch index being consumed (-1 if none)
 */
NIMCP_EXPORT int nimcp_prefetch_get_current_batch_idx(const nimcp_prefetch_manager_t* mgr);

/**
 * @brief Check if more batches available
 *
 * @param mgr Prefetch manager
 * @return true if more batches available
 */
NIMCP_EXPORT bool nimcp_prefetch_has_more(const nimcp_prefetch_manager_t* mgr);

//=============================================================================
// Callback API
//=============================================================================

/**
 * @brief Set batch ready callback
 *
 * @param mgr Prefetch manager
 * @param callback Called when each batch finishes prefetching
 * @param user_data Context for callback
 */
NIMCP_EXPORT void nimcp_prefetch_set_callback(
    nimcp_prefetch_manager_t* mgr,
    nimcp_prefetch_callback_fn callback,
    void* user_data);

//=============================================================================
// Status API
//=============================================================================

/**
 * @brief Get prefetch state
 *
 * @param mgr Prefetch manager
 * @return Current state
 */
NIMCP_EXPORT nimcp_prefetch_state_t nimcp_prefetch_get_state(
    const nimcp_prefetch_manager_t* mgr);

/**
 * @brief Get queue status
 *
 * @param mgr Prefetch manager
 * @param depth_out Output: queue depth
 * @param count_out Output: items currently in queue
 * @param ready_out Output: items ready for consumption
 */
NIMCP_EXPORT void nimcp_prefetch_queue_status(
    const nimcp_prefetch_manager_t* mgr,
    int* depth_out, int* count_out, int* ready_out);

/**
 * @brief Check if prefetch is running
 *
 * @param mgr Prefetch manager
 * @return true if running
 */
NIMCP_EXPORT bool nimcp_prefetch_is_running(const nimcp_prefetch_manager_t* mgr);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Prefetch statistics
 */
typedef struct nimcp_prefetch_stats {
    uint64_t batches_loaded;               /**< Total batches loaded */
    uint64_t batches_consumed;             /**< Total batches consumed */
    uint64_t stalls;                       /**< Consumer had to wait */
    double avg_load_time_ms;               /**< Average load time per batch */
    double avg_wait_time_ms;               /**< Average consumer wait time */
    double throughput_batches_per_sec;     /**< Batches per second */
    int queue_depth;                       /**< Queue capacity */
    int queue_fill;                        /**< Current queue fill level */
} nimcp_prefetch_stats_t;

/**
 * @brief Get prefetch statistics
 *
 * @param mgr Prefetch manager
 * @param stats Output statistics structure
 */
NIMCP_EXPORT void nimcp_prefetch_get_stats(
    const nimcp_prefetch_manager_t* mgr,
    nimcp_prefetch_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param mgr Prefetch manager
 */
NIMCP_EXPORT void nimcp_prefetch_reset_stats(nimcp_prefetch_manager_t* mgr);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get state name
 *
 * @param state Prefetch state
 * @return State name string
 */
NIMCP_EXPORT const char* nimcp_prefetch_state_name(nimcp_prefetch_state_t state);

/**
 * @brief Print prefetch manager info
 *
 * @param mgr Prefetch manager
 */
NIMCP_EXPORT void nimcp_prefetch_print_info(const nimcp_prefetch_manager_t* mgr);

/**
 * @brief Print statistics
 *
 * @param mgr Prefetch manager
 */
NIMCP_EXPORT void nimcp_prefetch_print_stats(const nimcp_prefetch_manager_t* mgr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREFETCH_H */
