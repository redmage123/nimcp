/**
 * @file nimcp_prefetch.cu
 * @brief Data Prefetching Manager Implementation
 *
 * WHAT: Implementation of background data prefetching for GPU training
 * WHY:  Hide data loading latency by prefetching ahead of consumption
 * HOW:  Background thread, producer-consumer queue, double buffering
 *
 * COMPILATION:
 * - Compiled only when CUDAToolkit is found (CMake conditional)
 * - CPU fallback stubs provided for non-CUDA builds
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

// Include CUDA headers FIRST to avoid extern "C" conflicts
#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

#include "gpu/transfer/nimcp_prefetch.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#define LOG_MODULE "PREFETCH"

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

//=============================================================================
// CUDA Implementation
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Check CUDA error and log
 */
static inline bool check_cuda(cudaError_t err, const char* op) {
    if (err != cudaSuccess) {
        LOG_ERROR("CUDA error in %s: %s", op, cudaGetErrorString(err));
        return false;
    }
    return true;
}

//=============================================================================
// Background Loader Thread
//=============================================================================

/**
 * @brief Background loader thread function
 */
static void* prefetch_loader_thread(void* arg) {
    nimcp_prefetch_manager_t* mgr = (nimcp_prefetch_manager_t*)arg;

    LOG_INFO("Prefetch loader thread started");

    cudaSetDevice(mgr->ctx->device_id);

    while (mgr->running) {
        pthread_mutex_lock(&mgr->mutex);

        // Wait for slot to be available
        while (mgr->running && mgr->queue_count >= mgr->queue_depth) {
            pthread_cond_wait(&mgr->cond_producer, &mgr->mutex);
        }

        // Check if we should stop
        if (!mgr->running) {
            pthread_mutex_unlock(&mgr->mutex);
            break;
        }

        // Check if we should pause
        if (mgr->state == NIMCP_PREFETCH_STATE_PAUSED) {
            pthread_mutex_unlock(&mgr->mutex);
            continue;
        }

        // Check if we've reached the end
        if (mgr->total_batches >= 0 && mgr->current_batch_idx >= mgr->total_batches) {
            pthread_mutex_unlock(&mgr->mutex);
            // Signal consumer that we're done
            pthread_cond_signal(&mgr->cond_consumer);
            break;
        }

        int batch_idx = mgr->current_batch_idx;
        int slot_idx = mgr->queue_head;
        nimcp_prefetch_slot_t* slot = &mgr->queue[slot_idx];

        slot->batch_idx = batch_idx;
        slot->loading = true;
        slot->ready = false;
        slot->in_use = false;

        mgr->current_batch_idx++;

        pthread_mutex_unlock(&mgr->mutex);

        // Load data for all tensors in this batch
        uint64_t load_start = get_time_ns();
        bool load_success = true;

        for (int t = 0; t < mgr->num_tensors && load_success; t++) {
            nimcp_prefetch_buffer_t* buffer = &mgr->buffers[t];
            size_t size = buffer->tensor_size;

            // Get host data from user callback
            void* host_data = mgr->data_loader(batch_idx, t, size, mgr->loader_user_data);

            if (!host_data) {
                LOG_DEBUG("Data loader returned NULL for batch %d, tensor %d", batch_idx, t);
                load_success = false;
                break;
            }

            // Start async transfer to GPU
            int result;
            if (buffer->use_staging && buffer->staging_buffer) {
                // Copy through staging buffer (for non-pinned host data)
                memcpy(buffer->staging_buffer, host_data, size);
                result = nimcp_double_buffer_start_load(
                    buffer->double_buffer, buffer->staging_buffer, size);
            } else {
                // Direct transfer (host data is pinned or we hope for the best)
                result = nimcp_double_buffer_start_load(
                    buffer->double_buffer, host_data, size);
            }

            if (result != 0) {
                LOG_ERROR("Failed to start load for batch %d, tensor %d", batch_idx, t);
                load_success = false;
                break;
            }

            // Wait for this tensor to complete
            void* gpu_ptr = nimcp_double_buffer_wait_load(buffer->double_buffer);
            if (!gpu_ptr) {
                LOG_ERROR("Failed to get GPU pointer for batch %d, tensor %d", batch_idx, t);
                load_success = false;
                break;
            }

            // Store GPU pointer for this slot
            slot->tensor_ptrs[t] = gpu_ptr;

            // Swap double buffer for next iteration
            nimcp_double_buffer_swap(buffer->double_buffer);
        }

        uint64_t load_end = get_time_ns();

        pthread_mutex_lock(&mgr->mutex);

        slot->loading = false;

        if (load_success) {
            slot->ready = true;
            mgr->queue_head = (mgr->queue_head + 1) % mgr->queue_depth;
            mgr->queue_count++;
            mgr->batches_loaded++;
            mgr->total_load_time_ns += (load_end - load_start);

            LOG_DEBUG("Batch %d prefetched, queue count: %d", batch_idx, mgr->queue_count);

            // Notify consumer
            pthread_cond_signal(&mgr->cond_consumer);

            // Call callback if set
            if (mgr->on_batch_ready) {
                pthread_mutex_unlock(&mgr->mutex);
                mgr->on_batch_ready(batch_idx, mgr->callback_user_data);
                pthread_mutex_lock(&mgr->mutex);
            }
        } else {
            // Load failed - could be end of data
            if (mgr->total_batches < 0) {
                // Infinite mode - treat NULL as end of data
                mgr->total_batches = batch_idx;
            }
            mgr->current_batch_idx--;  // Undo increment
        }

        pthread_mutex_unlock(&mgr->mutex);
    }

    LOG_INFO("Prefetch loader thread exiting");

    pthread_mutex_lock(&mgr->mutex);
    mgr->state = NIMCP_PREFETCH_STATE_STOPPED;
    pthread_cond_broadcast(&mgr->cond_consumer);
    pthread_mutex_unlock(&mgr->mutex);

    return NULL;
}

//=============================================================================
// Prefetch Manager Implementation
//=============================================================================

nimcp_prefetch_manager_t* nimcp_prefetch_create(
    nimcp_gpu_context_t* ctx,
    const size_t* tensor_sizes,
    int num_tensors)
{
    return nimcp_prefetch_create_ex(ctx, tensor_sizes, num_tensors,
                                     NIMCP_PREFETCH_DEFAULT_QUEUE_DEPTH, NULL);
}

nimcp_prefetch_manager_t* nimcp_prefetch_create_ex(
    nimcp_gpu_context_t* ctx,
    const size_t* tensor_sizes,
    int num_tensors,
    int queue_depth,
    nimcp_transfer_manager_t* transfer_mgr)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx)) {
        LOG_ERROR("Invalid GPU context");
        return NULL;
    }

    if (!tensor_sizes || num_tensors <= 0 || num_tensors > NIMCP_PREFETCH_MAX_TENSORS) {
        LOG_ERROR("Invalid tensor configuration: num_tensors=%d", num_tensors);
        return NULL;
    }

    // Clamp queue depth
    if (queue_depth < 1) queue_depth = 1;
    if (queue_depth > NIMCP_PREFETCH_MAX_QUEUE_DEPTH) {
        queue_depth = NIMCP_PREFETCH_MAX_QUEUE_DEPTH;
    }

    nimcp_prefetch_manager_t* mgr = (nimcp_prefetch_manager_t*)nimcp_calloc(
        1, sizeof(nimcp_prefetch_manager_t));
    if (!mgr) {
        LOG_ERROR("Failed to allocate prefetch manager");
        return NULL;
    }

    mgr->ctx = ctx;
    mgr->num_tensors = num_tensors;
    mgr->queue_depth = queue_depth;
    mgr->queue_head = 0;
    mgr->queue_tail = 0;
    mgr->queue_count = 0;
    mgr->state = NIMCP_PREFETCH_STATE_IDLE;
    mgr->current_batch_idx = 0;
    mgr->total_batches = -1;  // Infinite by default
    mgr->running = false;
    mgr->thread_created = false;

    // Copy tensor sizes
    mgr->tensor_sizes = (size_t*)nimcp_malloc(num_tensors * sizeof(size_t));
    if (!mgr->tensor_sizes) {
        nimcp_free(mgr);
        return NULL;
    }
    memcpy(mgr->tensor_sizes, tensor_sizes, num_tensors * sizeof(size_t));

    // Create or use transfer manager
    if (transfer_mgr) {
        mgr->transfer_mgr = transfer_mgr;
        mgr->owns_transfer_mgr = false;
    } else {
        // Calculate total pinned pool size (2x total tensor size for double buffering)
        size_t total_size = 0;
        for (int i = 0; i < num_tensors; i++) {
            total_size += tensor_sizes[i];
        }
        size_t pinned_size = total_size * 2;

        mgr->transfer_mgr = nimcp_transfer_manager_create(ctx, 2, pinned_size);
        if (!mgr->transfer_mgr) {
            LOG_ERROR("Failed to create transfer manager");
            nimcp_free(mgr->tensor_sizes);
            nimcp_free(mgr);
            return NULL;
        }
        mgr->owns_transfer_mgr = true;
    }

    // Create double buffers for each tensor
    for (int i = 0; i < num_tensors; i++) {
        nimcp_prefetch_buffer_t* buf = &mgr->buffers[i];
        buf->tensor_size = tensor_sizes[i];

        buf->double_buffer = nimcp_double_buffer_create(ctx, tensor_sizes[i]);
        if (!buf->double_buffer) {
            LOG_ERROR("Failed to create double buffer for tensor %d", i);
            // Cleanup already created buffers
            for (int j = 0; j < i; j++) {
                nimcp_double_buffer_destroy(mgr->buffers[j].double_buffer);
                if (mgr->buffers[j].staging_buffer) {
                    nimcp_transfer_free_pinned(mgr->transfer_mgr,
                                               mgr->buffers[j].staging_buffer);
                }
            }
            if (mgr->owns_transfer_mgr) {
                nimcp_transfer_manager_destroy(mgr->transfer_mgr);
            }
            nimcp_free(mgr->tensor_sizes);
            nimcp_free(mgr);
            return NULL;
        }

        // Allocate staging buffer for non-pinned host data
        buf->staging_buffer = nimcp_transfer_alloc_pinned(mgr->transfer_mgr,
                                                           tensor_sizes[i]);
        buf->use_staging = (buf->staging_buffer != NULL);

        if (!buf->staging_buffer) {
            LOG_WARN("Could not allocate staging buffer for tensor %d, "
                     "transfers may be slower", i);
        }
    }

    // Initialize queue slots
    for (int i = 0; i < queue_depth; i++) {
        mgr->queue[i].batch_idx = -1;
        mgr->queue[i].ready = false;
        mgr->queue[i].in_use = false;
        mgr->queue[i].loading = false;
        for (int t = 0; t < num_tensors; t++) {
            mgr->queue[i].tensor_ptrs[t] = NULL;
        }
    }

    // Initialize synchronization primitives
    int result = pthread_mutex_init(&mgr->mutex, NULL);
    if (result != 0) {
        LOG_ERROR("Failed to init mutex: %d", result);
        goto cleanup;
    }

    result = pthread_cond_init(&mgr->cond_producer, NULL);
    if (result != 0) {
        LOG_ERROR("Failed to init producer condition: %d", result);
        pthread_mutex_destroy(&mgr->mutex);
        goto cleanup;
    }

    result = pthread_cond_init(&mgr->cond_consumer, NULL);
    if (result != 0) {
        LOG_ERROR("Failed to init consumer condition: %d", result);
        pthread_cond_destroy(&mgr->cond_producer);
        pthread_mutex_destroy(&mgr->mutex);
        goto cleanup;
    }

    // Initialize statistics
    mgr->batches_loaded = 0;
    mgr->batches_consumed = 0;
    mgr->stalls = 0;
    mgr->total_load_time_ns = 0;
    mgr->total_wait_time_ns = 0;

    mgr->initialized = true;

    LOG_INFO("Prefetch manager created: %d tensors, queue depth %d",
             num_tensors, queue_depth);

    return mgr;

cleanup:
    for (int i = 0; i < num_tensors; i++) {
        if (mgr->buffers[i].double_buffer) {
            nimcp_double_buffer_destroy(mgr->buffers[i].double_buffer);
        }
        if (mgr->buffers[i].staging_buffer) {
            nimcp_transfer_free_pinned(mgr->transfer_mgr, mgr->buffers[i].staging_buffer);
        }
    }
    if (mgr->owns_transfer_mgr && mgr->transfer_mgr) {
        nimcp_transfer_manager_destroy(mgr->transfer_mgr);
    }
    nimcp_free(mgr->tensor_sizes);
    nimcp_free(mgr);
    return NULL;
}

void nimcp_prefetch_destroy(nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return;

    // Stop if running
    nimcp_prefetch_stop(mgr);

    // Destroy synchronization primitives
    pthread_cond_destroy(&mgr->cond_consumer);
    pthread_cond_destroy(&mgr->cond_producer);
    pthread_mutex_destroy(&mgr->mutex);

    // Destroy double buffers and staging
    for (int i = 0; i < mgr->num_tensors; i++) {
        if (mgr->buffers[i].double_buffer) {
            nimcp_double_buffer_destroy(mgr->buffers[i].double_buffer);
        }
        if (mgr->buffers[i].staging_buffer) {
            nimcp_transfer_free_pinned(mgr->transfer_mgr, mgr->buffers[i].staging_buffer);
        }
    }

    // Destroy transfer manager if we own it
    if (mgr->owns_transfer_mgr && mgr->transfer_mgr) {
        nimcp_transfer_manager_destroy(mgr->transfer_mgr);
    }

    nimcp_free(mgr->tensor_sizes);

    LOG_INFO("Prefetch manager destroyed: loaded %lu, consumed %lu batches",
             (unsigned long)mgr->batches_loaded,
             (unsigned long)mgr->batches_consumed);

    nimcp_free(mgr);
}

bool nimcp_prefetch_is_valid(const nimcp_prefetch_manager_t* mgr) {
    return mgr && mgr->initialized && nimcp_gpu_context_is_valid(mgr->ctx);
}

//=============================================================================
// Prefetch Control
//=============================================================================

int nimcp_prefetch_start(
    nimcp_prefetch_manager_t* mgr,
    nimcp_data_loader_fn data_loader,
    void* user_data)
{
    return nimcp_prefetch_start_with_count(mgr, data_loader, user_data, -1);
}

int nimcp_prefetch_start_with_count(
    nimcp_prefetch_manager_t* mgr,
    nimcp_data_loader_fn data_loader,
    void* user_data,
    int total_batches)
{
    if (!nimcp_prefetch_is_valid(mgr)) {
        LOG_ERROR("Invalid prefetch manager");
        return -1;
    }

    if (!data_loader) {
        LOG_ERROR("Data loader callback is required");
        return -1;
    }

    pthread_mutex_lock(&mgr->mutex);

    if (mgr->running) {
        pthread_mutex_unlock(&mgr->mutex);
        LOG_WARN("Prefetch already running");
        return -1;
    }

    mgr->data_loader = data_loader;
    mgr->loader_user_data = user_data;
    mgr->total_batches = total_batches;
    mgr->current_batch_idx = 0;
    mgr->running = true;
    mgr->state = NIMCP_PREFETCH_STATE_RUNNING;

    pthread_mutex_unlock(&mgr->mutex);

    // Create background thread
    int result = nimcp_thread_create(&mgr->loader_thread,
                                     prefetch_loader_thread, mgr, NULL);
    if (result != 0) {
        LOG_ERROR("Failed to create loader thread: %d", result);
        pthread_mutex_lock(&mgr->mutex);
        mgr->running = false;
        mgr->state = NIMCP_PREFETCH_STATE_ERROR;
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    mgr->thread_created = true;

    LOG_INFO("Prefetch started: %s batches",
             total_batches < 0 ? "infinite" : "finite");

    return 0;
}

void nimcp_prefetch_stop(nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return;

    pthread_mutex_lock(&mgr->mutex);

    if (!mgr->running) {
        pthread_mutex_unlock(&mgr->mutex);
        return;
    }

    mgr->running = false;
    mgr->state = NIMCP_PREFETCH_STATE_STOPPING;

    // Wake up producer thread
    pthread_cond_signal(&mgr->cond_producer);

    pthread_mutex_unlock(&mgr->mutex);

    // Wait for thread to exit
    if (mgr->thread_created) {
        nimcp_thread_join(mgr->loader_thread, NULL);
        mgr->thread_created = false;
    }

    mgr->state = NIMCP_PREFETCH_STATE_STOPPED;

    LOG_INFO("Prefetch stopped");
}

void nimcp_prefetch_pause(nimcp_prefetch_manager_t* mgr) {
    if (!nimcp_prefetch_is_valid(mgr)) return;

    pthread_mutex_lock(&mgr->mutex);
    if (mgr->state == NIMCP_PREFETCH_STATE_RUNNING) {
        mgr->state = NIMCP_PREFETCH_STATE_PAUSED;
        LOG_INFO("Prefetch paused");
    }
    pthread_mutex_unlock(&mgr->mutex);
}

void nimcp_prefetch_resume(nimcp_prefetch_manager_t* mgr) {
    if (!nimcp_prefetch_is_valid(mgr)) return;

    pthread_mutex_lock(&mgr->mutex);
    if (mgr->state == NIMCP_PREFETCH_STATE_PAUSED) {
        mgr->state = NIMCP_PREFETCH_STATE_RUNNING;
        pthread_cond_signal(&mgr->cond_producer);
        LOG_INFO("Prefetch resumed");
    }
    pthread_mutex_unlock(&mgr->mutex);
}

void nimcp_prefetch_reset(nimcp_prefetch_manager_t* mgr) {
    if (!nimcp_prefetch_is_valid(mgr)) return;

    // Stop if running
    nimcp_prefetch_stop(mgr);

    pthread_mutex_lock(&mgr->mutex);

    // Reset queue
    mgr->queue_head = 0;
    mgr->queue_tail = 0;
    mgr->queue_count = 0;
    mgr->current_batch_idx = 0;

    for (int i = 0; i < mgr->queue_depth; i++) {
        mgr->queue[i].batch_idx = -1;
        mgr->queue[i].ready = false;
        mgr->queue[i].in_use = false;
        mgr->queue[i].loading = false;
    }

    mgr->state = NIMCP_PREFETCH_STATE_IDLE;

    pthread_mutex_unlock(&mgr->mutex);

    LOG_INFO("Prefetch reset for new epoch");
}

//=============================================================================
// Batch Access
//=============================================================================

void** nimcp_prefetch_get_batch(nimcp_prefetch_manager_t* mgr) {
    return nimcp_prefetch_get_batch_timeout(mgr, -1);
}

void** nimcp_prefetch_get_batch_timeout(nimcp_prefetch_manager_t* mgr, int timeout_ms) {
    if (!nimcp_prefetch_is_valid(mgr)) return NULL;

    pthread_mutex_lock(&mgr->mutex);

    uint64_t wait_start = get_time_ns();
    bool stalled = false;

    // Wait for batch to be ready
    while (mgr->queue_count == 0 || !mgr->queue[mgr->queue_tail].ready) {
        // Check if we're done
        if (!mgr->running && mgr->queue_count == 0) {
            pthread_mutex_unlock(&mgr->mutex);
            return NULL;  // End of data
        }

        if (timeout_ms == 0) {
            // Non-blocking
            pthread_mutex_unlock(&mgr->mutex);
            return NULL;
        }

        if (!stalled) {
            stalled = true;
            mgr->stalls++;
        }

        if (timeout_ms > 0) {
            // Timed wait
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }

            int result = pthread_cond_timedwait(&mgr->cond_consumer, &mgr->mutex, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&mgr->mutex);
                return NULL;
            }
        } else {
            // Infinite wait
            pthread_cond_wait(&mgr->cond_consumer, &mgr->mutex);
        }
    }

    uint64_t wait_end = get_time_ns();
    if (stalled) {
        mgr->total_wait_time_ns += (wait_end - wait_start);
    }

    // Get the ready batch
    nimcp_prefetch_slot_t* slot = &mgr->queue[mgr->queue_tail];
    slot->in_use = true;

    pthread_mutex_unlock(&mgr->mutex);

    return slot->tensor_ptrs;
}

void** nimcp_prefetch_try_get_batch(nimcp_prefetch_manager_t* mgr) {
    return nimcp_prefetch_get_batch_timeout(mgr, 0);
}

void nimcp_prefetch_release_batch(nimcp_prefetch_manager_t* mgr) {
    if (!nimcp_prefetch_is_valid(mgr)) return;

    pthread_mutex_lock(&mgr->mutex);

    if (mgr->queue_count > 0) {
        nimcp_prefetch_slot_t* slot = &mgr->queue[mgr->queue_tail];
        slot->in_use = false;
        slot->ready = false;

        mgr->queue_tail = (mgr->queue_tail + 1) % mgr->queue_depth;
        mgr->queue_count--;
        mgr->batches_consumed++;

        // Notify producer that slot is available
        pthread_cond_signal(&mgr->cond_producer);
    }

    pthread_mutex_unlock(&mgr->mutex);
}

int nimcp_prefetch_get_current_batch_idx(const nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return -1;

    pthread_mutex_lock((pthread_mutex_t*)&mgr->mutex);
    int idx = -1;
    if (mgr->queue_count > 0) {
        idx = mgr->queue[mgr->queue_tail].batch_idx;
    }
    pthread_mutex_unlock((pthread_mutex_t*)&mgr->mutex);

    return idx;
}

bool nimcp_prefetch_has_more(const nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return false;

    pthread_mutex_lock((pthread_mutex_t*)&mgr->mutex);
    bool has_more = mgr->running || mgr->queue_count > 0;
    pthread_mutex_unlock((pthread_mutex_t*)&mgr->mutex);

    return has_more;
}

//=============================================================================
// Callback API
//=============================================================================

void nimcp_prefetch_set_callback(
    nimcp_prefetch_manager_t* mgr,
    nimcp_prefetch_callback_fn callback,
    void* user_data)
{
    if (!mgr) return;

    pthread_mutex_lock(&mgr->mutex);
    mgr->on_batch_ready = callback;
    mgr->callback_user_data = user_data;
    pthread_mutex_unlock(&mgr->mutex);
}

//=============================================================================
// Status API
//=============================================================================

nimcp_prefetch_state_t nimcp_prefetch_get_state(const nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return NIMCP_PREFETCH_STATE_ERROR;
    return mgr->state;
}

void nimcp_prefetch_queue_status(
    const nimcp_prefetch_manager_t* mgr,
    int* depth_out, int* count_out, int* ready_out)
{
    if (!mgr) {
        if (depth_out) *depth_out = 0;
        if (count_out) *count_out = 0;
        if (ready_out) *ready_out = 0;
        return;
    }

    pthread_mutex_lock((pthread_mutex_t*)&mgr->mutex);

    if (depth_out) *depth_out = mgr->queue_depth;
    if (count_out) *count_out = mgr->queue_count;

    if (ready_out) {
        int ready = 0;
        for (int i = 0; i < mgr->queue_depth; i++) {
            if (mgr->queue[i].ready && !mgr->queue[i].in_use) {
                ready++;
            }
        }
        *ready_out = ready;
    }

    pthread_mutex_unlock((pthread_mutex_t*)&mgr->mutex);
}

bool nimcp_prefetch_is_running(const nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return false;
    return mgr->running;
}

//=============================================================================
// Statistics
//=============================================================================

void nimcp_prefetch_get_stats(
    const nimcp_prefetch_manager_t* mgr,
    nimcp_prefetch_stats_t* stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));

    if (!mgr) return;

    pthread_mutex_lock((pthread_mutex_t*)&mgr->mutex);

    stats->batches_loaded = mgr->batches_loaded;
    stats->batches_consumed = mgr->batches_consumed;
    stats->stalls = mgr->stalls;
    stats->queue_depth = mgr->queue_depth;
    stats->queue_fill = mgr->queue_count;

    if (mgr->batches_loaded > 0) {
        stats->avg_load_time_ms =
            (double)mgr->total_load_time_ns / (double)mgr->batches_loaded / 1000000.0;
    }

    if (mgr->stalls > 0) {
        stats->avg_wait_time_ms =
            (double)mgr->total_wait_time_ns / (double)mgr->stalls / 1000000.0;
    }

    // Calculate throughput (very rough estimate)
    if (mgr->total_load_time_ns > 0) {
        double total_seconds = (double)mgr->total_load_time_ns / 1000000000.0;
        stats->throughput_batches_per_sec = (double)mgr->batches_loaded / total_seconds;
    }

    pthread_mutex_unlock((pthread_mutex_t*)&mgr->mutex);
}

void nimcp_prefetch_reset_stats(nimcp_prefetch_manager_t* mgr) {
    if (!mgr) return;

    pthread_mutex_lock(&mgr->mutex);
    mgr->batches_loaded = 0;
    mgr->batches_consumed = 0;
    mgr->stalls = 0;
    mgr->total_load_time_ns = 0;
    mgr->total_wait_time_ns = 0;
    pthread_mutex_unlock(&mgr->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_prefetch_state_name(nimcp_prefetch_state_t state) {
    switch (state) {
        case NIMCP_PREFETCH_STATE_IDLE: return "IDLE";
        case NIMCP_PREFETCH_STATE_RUNNING: return "RUNNING";
        case NIMCP_PREFETCH_STATE_PAUSED: return "PAUSED";
        case NIMCP_PREFETCH_STATE_STOPPING: return "STOPPING";
        case NIMCP_PREFETCH_STATE_STOPPED: return "STOPPED";
        case NIMCP_PREFETCH_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void nimcp_prefetch_print_info(const nimcp_prefetch_manager_t* mgr) {
    if (!mgr) {
        printf("Prefetch Manager: NULL\n");
        return;
    }

    printf("Prefetch Manager:\n");
    printf("  Initialized: %s\n", mgr->initialized ? "yes" : "no");
    printf("  State: %s\n", nimcp_prefetch_state_name(mgr->state));
    printf("  Tensors: %d\n", mgr->num_tensors);
    printf("  Queue depth: %d\n", mgr->queue_depth);
    printf("  Queue fill: %d\n", mgr->queue_count);
    printf("  Total batches: %d\n", mgr->total_batches);
    printf("  Current batch: %d\n", mgr->current_batch_idx);

    printf("  Tensor sizes:\n");
    for (int i = 0; i < mgr->num_tensors; i++) {
        printf("    Tensor %d: %zu bytes (%.2f MB)\n",
               i, mgr->buffers[i].tensor_size,
               (double)mgr->buffers[i].tensor_size / (1024.0 * 1024.0));
    }
}

void nimcp_prefetch_print_stats(const nimcp_prefetch_manager_t* mgr) {
    nimcp_prefetch_stats_t stats;
    nimcp_prefetch_get_stats(mgr, &stats);

    printf("Prefetch Statistics:\n");
    printf("  Batches loaded: %lu\n", (unsigned long)stats.batches_loaded);
    printf("  Batches consumed: %lu\n", (unsigned long)stats.batches_consumed);
    printf("  Consumer stalls: %lu\n", (unsigned long)stats.stalls);
    printf("  Avg load time: %.3f ms\n", stats.avg_load_time_ms);
    printf("  Avg wait time: %.3f ms\n", stats.avg_wait_time_ms);
    printf("  Throughput: %.1f batches/sec\n", stats.throughput_batches_per_sec);
    printf("  Queue depth: %d\n", stats.queue_depth);
    printf("  Queue fill: %d\n", stats.queue_fill);
}

#else /* !NIMCP_ENABLE_CUDA */

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

nimcp_prefetch_manager_t* nimcp_prefetch_create(
    nimcp_gpu_context_t* ctx, const size_t* tensor_sizes, int num_tensors)
{
    (void)ctx; (void)tensor_sizes; (void)num_tensors;
    return NULL;
}

nimcp_prefetch_manager_t* nimcp_prefetch_create_ex(
    nimcp_gpu_context_t* ctx, const size_t* tensor_sizes, int num_tensors,
    int queue_depth, nimcp_transfer_manager_t* transfer_mgr)
{
    (void)ctx; (void)tensor_sizes; (void)num_tensors;
    (void)queue_depth; (void)transfer_mgr;
    return NULL;
}

void nimcp_prefetch_destroy(nimcp_prefetch_manager_t* mgr) { (void)mgr; }
bool nimcp_prefetch_is_valid(const nimcp_prefetch_manager_t* mgr) { (void)mgr; return false; }

int nimcp_prefetch_start(nimcp_prefetch_manager_t* mgr, nimcp_data_loader_fn data_loader, void* user_data)
{
    (void)mgr; (void)data_loader; (void)user_data;
    return -1;
}

int nimcp_prefetch_start_with_count(nimcp_prefetch_manager_t* mgr, nimcp_data_loader_fn data_loader, void* user_data, int total_batches)
{
    (void)mgr; (void)data_loader; (void)user_data; (void)total_batches;
    return -1;
}

void nimcp_prefetch_stop(nimcp_prefetch_manager_t* mgr) { (void)mgr; }
void nimcp_prefetch_pause(nimcp_prefetch_manager_t* mgr) { (void)mgr; }
void nimcp_prefetch_resume(nimcp_prefetch_manager_t* mgr) { (void)mgr; }
void nimcp_prefetch_reset(nimcp_prefetch_manager_t* mgr) { (void)mgr; }

void** nimcp_prefetch_get_batch(nimcp_prefetch_manager_t* mgr) { (void)mgr; return NULL; }
void** nimcp_prefetch_get_batch_timeout(nimcp_prefetch_manager_t* mgr, int timeout_ms) { (void)mgr; (void)timeout_ms; return NULL; }
void** nimcp_prefetch_try_get_batch(nimcp_prefetch_manager_t* mgr) { (void)mgr; return NULL; }
void nimcp_prefetch_release_batch(nimcp_prefetch_manager_t* mgr) { (void)mgr; }
int nimcp_prefetch_get_current_batch_idx(const nimcp_prefetch_manager_t* mgr) { (void)mgr; return -1; }
bool nimcp_prefetch_has_more(const nimcp_prefetch_manager_t* mgr) { (void)mgr; return false; }

void nimcp_prefetch_set_callback(nimcp_prefetch_manager_t* mgr, nimcp_prefetch_callback_fn callback, void* user_data)
{
    (void)mgr; (void)callback; (void)user_data;
}

nimcp_prefetch_state_t nimcp_prefetch_get_state(const nimcp_prefetch_manager_t* mgr) { (void)mgr; return NIMCP_PREFETCH_STATE_ERROR; }
void nimcp_prefetch_queue_status(const nimcp_prefetch_manager_t* mgr, int* depth_out, int* count_out, int* ready_out)
{
    (void)mgr;
    if (depth_out) *depth_out = 0;
    if (count_out) *count_out = 0;
    if (ready_out) *ready_out = 0;
}
bool nimcp_prefetch_is_running(const nimcp_prefetch_manager_t* mgr) { (void)mgr; return false; }

void nimcp_prefetch_get_stats(const nimcp_prefetch_manager_t* mgr, nimcp_prefetch_stats_t* stats)
{
    (void)mgr;
    if (stats) memset(stats, 0, sizeof(*stats));
}
void nimcp_prefetch_reset_stats(nimcp_prefetch_manager_t* mgr) { (void)mgr; }

const char* nimcp_prefetch_state_name(nimcp_prefetch_state_t state) {
    switch (state) {
        case NIMCP_PREFETCH_STATE_IDLE: return "IDLE";
        case NIMCP_PREFETCH_STATE_RUNNING: return "RUNNING";
        case NIMCP_PREFETCH_STATE_PAUSED: return "PAUSED";
        case NIMCP_PREFETCH_STATE_STOPPING: return "STOPPING";
        case NIMCP_PREFETCH_STATE_STOPPED: return "STOPPED";
        case NIMCP_PREFETCH_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void nimcp_prefetch_print_info(const nimcp_prefetch_manager_t* mgr) { (void)mgr; printf("Prefetch Manager: CUDA not available\n"); }
void nimcp_prefetch_print_stats(const nimcp_prefetch_manager_t* mgr) { (void)mgr; printf("Prefetch Statistics: CUDA not available\n"); }

#endif /* NIMCP_ENABLE_CUDA */
