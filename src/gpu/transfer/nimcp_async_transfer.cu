/**
 * @file nimcp_async_transfer.cu
 * @brief Async Transfer Manager Implementation
 *
 * WHAT: Implementation of async GPU memory transfers with double-buffering and pipelines
 * WHY:  Maximize GPU utilization by overlapping compute and data transfer
 * HOW:  CUDA streams, events, pinned memory, and careful synchronization
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

#include "gpu/transfer/nimcp_async_transfer.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define LOG_MODULE "ASYNC_TRANSFER"

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
// Transfer Manager Implementation
//=============================================================================

nimcp_transfer_manager_t* nimcp_transfer_manager_create(
    nimcp_gpu_context_t* ctx,
    int num_streams,
    size_t pinned_pool_size)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx)) {
        LOG_ERROR("Invalid GPU context");
        return NULL;
    }

    // Clamp num_streams
    if (num_streams < 1) num_streams = 1;
    if (num_streams > NIMCP_TRANSFER_MAX_STREAMS) num_streams = NIMCP_TRANSFER_MAX_STREAMS;

    nimcp_transfer_manager_t* mgr = (nimcp_transfer_manager_t*)nimcp_calloc(
        1, sizeof(nimcp_transfer_manager_t));
    if (!mgr) {
        LOG_ERROR("Failed to allocate transfer manager");
        return NULL;
    }

    mgr->ctx = ctx;
    mgr->num_streams = num_streams;
    mgr->next_stream = 0;
    mgr->pending_count = 0;
    mgr->next_request_id = 1;

    // Set device
    cudaError_t err = cudaSetDevice(ctx->device_id);
    if (!check_cuda(err, "cudaSetDevice")) {
        nimcp_free(mgr);
        return NULL;
    }

    // Create transfer streams
    for (int i = 0; i < num_streams; i++) {
        err = cudaStreamCreate(&mgr->transfer_streams[i]);
        if (!check_cuda(err, "cudaStreamCreate")) {
            // Cleanup already created streams
            for (int j = 0; j < i; j++) {
                cudaStreamDestroy(mgr->transfer_streams[j]);
            }
            nimcp_free(mgr);
            return NULL;
        }
    }

    // Initialize pending requests
    for (size_t i = 0; i < NIMCP_TRANSFER_MAX_PENDING; i++) {
        mgr->pending[i].request_id = NIMCP_TRANSFER_INVALID_ID;
        mgr->pending[i].status = NIMCP_TRANSFER_STATUS_COMPLETED;
        mgr->pending[i].completion_event = NULL;
    }

    // Create pinned memory pool
    mgr->pinned_pool_size = pinned_pool_size;
    mgr->pinned_pool_used = 0;
    mgr->pinned_pool = NULL;
    mgr->pinned_allocs = NULL;
    mgr->pinned_alloc_count = 0;

    if (pinned_pool_size > 0) {
        err = cudaHostAlloc(&mgr->pinned_pool, pinned_pool_size, cudaHostAllocDefault);
        if (!check_cuda(err, "cudaHostAlloc")) {
            LOG_WARN("Failed to allocate pinned pool of %zu bytes, continuing without pool",
                     pinned_pool_size);
            mgr->pinned_pool = NULL;
            mgr->pinned_pool_size = 0;
        }
    }

    // Initialize statistics
    mgr->total_bytes_transferred = 0;
    mgr->total_transfers = 0;
    mgr->total_transfer_time_ms = 0.0;
    mgr->h2d_bytes = 0;
    mgr->d2h_bytes = 0;
    mgr->d2d_bytes = 0;

    mgr->initialized = true;

    LOG_INFO("Transfer manager created: %d streams, %zu MB pinned pool",
             num_streams, pinned_pool_size / (1024 * 1024));

    return mgr;
}

void nimcp_transfer_manager_destroy(nimcp_transfer_manager_t* mgr) {
    if (!mgr) return;

    // Wait for all pending transfers
    nimcp_transfer_wait_all(mgr);

    if (mgr->ctx) {
        cudaSetDevice(mgr->ctx->device_id);
    }

    // Destroy streams
    for (int i = 0; i < mgr->num_streams; i++) {
        if (mgr->transfer_streams[i]) {
            cudaStreamDestroy(mgr->transfer_streams[i]);
        }
    }

    // Destroy pending request events
    for (size_t i = 0; i < NIMCP_TRANSFER_MAX_PENDING; i++) {
        if (mgr->pending[i].completion_event) {
            cudaEventDestroy(mgr->pending[i].completion_event);
        }
    }

    // Free pinned memory pool
    if (mgr->pinned_pool) {
        cudaFreeHost(mgr->pinned_pool);
    }

    // Free pinned allocation tracking
    nimcp_pinned_alloc_t* alloc = mgr->pinned_allocs;
    while (alloc) {
        nimcp_pinned_alloc_t* next = alloc->next;
        // Free directly allocated pinned memory (not from pool)
        if (alloc->ptr && (alloc->ptr < mgr->pinned_pool ||
            (char*)alloc->ptr >= (char*)mgr->pinned_pool + mgr->pinned_pool_size)) {
            cudaFreeHost(alloc->ptr);
        }
        nimcp_free(alloc);
        alloc = next;
    }

    LOG_INFO("Transfer manager destroyed: %zu bytes transferred in %zu transfers",
             mgr->total_bytes_transferred, mgr->total_transfers);

    nimcp_free(mgr);
}

bool nimcp_transfer_manager_is_valid(const nimcp_transfer_manager_t* mgr) {
    return mgr && mgr->initialized && nimcp_gpu_context_is_valid(mgr->ctx);
}

//=============================================================================
// Async Transfer Operations
//=============================================================================

/**
 * @brief Find free slot in pending array
 */
static int find_free_slot(nimcp_transfer_manager_t* mgr) {
    // First, try to find a completed slot
    for (size_t i = 0; i < NIMCP_TRANSFER_MAX_PENDING; i++) {
        if (mgr->pending[i].status == NIMCP_TRANSFER_STATUS_COMPLETED ||
            mgr->pending[i].status == NIMCP_TRANSFER_STATUS_FAILED ||
            mgr->pending[i].status == NIMCP_TRANSFER_STATUS_CANCELLED) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Find request by ID
 */
static nimcp_transfer_request_t* find_request(nimcp_transfer_manager_t* mgr, uint64_t request_id) {
    for (size_t i = 0; i < NIMCP_TRANSFER_MAX_PENDING; i++) {
        if (mgr->pending[i].request_id == request_id) {
            return &mgr->pending[i];
        }
    }
    return NULL;
}

uint64_t nimcp_transfer_async(
    nimcp_transfer_manager_t* mgr,
    void* dst, const void* src, size_t size,
    nimcp_transfer_direction_t direction,
    nimcp_transfer_callback_t callback, void* callback_data)
{
    if (!nimcp_transfer_manager_is_valid(mgr)) {
        LOG_ERROR("Invalid transfer manager");
        return NIMCP_TRANSFER_INVALID_ID;
    }
    if (!dst || !src || size == 0) {
        LOG_ERROR("Invalid transfer parameters");
        return NIMCP_TRANSFER_INVALID_ID;
    }

    // Poll for completed transfers first
    nimcp_transfer_poll(mgr);

    // Find free slot
    int slot = find_free_slot(mgr);
    if (slot < 0) {
        LOG_ERROR("No free transfer slots, waiting for completion");
        nimcp_transfer_wait_all(mgr);
        slot = find_free_slot(mgr);
        if (slot < 0) {
            LOG_ERROR("Failed to find free slot after wait");
            return NIMCP_TRANSFER_INVALID_ID;
        }
    }

    nimcp_transfer_request_t* req = &mgr->pending[slot];

    // Set device
    cudaSetDevice(mgr->ctx->device_id);

    // Create completion event if needed
    if (!req->completion_event) {
        cudaError_t err = cudaEventCreate(&req->completion_event);
        if (!check_cuda(err, "cudaEventCreate")) {
            return NIMCP_TRANSFER_INVALID_ID;
        }
    }

    // Select stream (round-robin)
    cudaStream_t stream = mgr->transfer_streams[mgr->next_stream];
    mgr->next_stream = (mgr->next_stream + 1) % mgr->num_streams;

    // Determine copy kind
    cudaMemcpyKind kind;
    switch (direction) {
        case NIMCP_TRANSFER_H2D: kind = cudaMemcpyHostToDevice; break;
        case NIMCP_TRANSFER_D2H: kind = cudaMemcpyDeviceToHost; break;
        case NIMCP_TRANSFER_D2D: kind = cudaMemcpyDeviceToDevice; break;
        case NIMCP_TRANSFER_P2P: kind = cudaMemcpyDeviceToDevice; break;
        default:
            LOG_ERROR("Invalid transfer direction: %d", direction);
            return NIMCP_TRANSFER_INVALID_ID;
    }

    // Fill request
    req->host_ptr = (direction == NIMCP_TRANSFER_H2D) ? (void*)src : dst;
    req->device_ptr = (direction == NIMCP_TRANSFER_H2D) ? dst : (void*)src;
    req->size = size;
    req->direction = direction;
    req->stream = stream;
    req->callback = callback;
    req->callback_data = callback_data;
    req->request_id = mgr->next_request_id++;
    req->status = NIMCP_TRANSFER_STATUS_IN_PROGRESS;
    req->error_code = 0;
    req->start_time_ns = get_time_ns();
    req->end_time_ns = 0;

    // Start async copy
    cudaError_t err = cudaMemcpyAsync(dst, src, size, kind, stream);
    if (!check_cuda(err, "cudaMemcpyAsync")) {
        req->status = NIMCP_TRANSFER_STATUS_FAILED;
        req->error_code = (int)err;
        return NIMCP_TRANSFER_INVALID_ID;
    }

    // Record completion event
    err = cudaEventRecord(req->completion_event, stream);
    if (!check_cuda(err, "cudaEventRecord")) {
        req->status = NIMCP_TRANSFER_STATUS_FAILED;
        req->error_code = (int)err;
        return NIMCP_TRANSFER_INVALID_ID;
    }

    mgr->pending_count++;

    LOG_DEBUG("Transfer queued: id=%lu, %s, %zu bytes",
              (unsigned long)req->request_id,
              nimcp_transfer_direction_name(direction), size);

    return req->request_id;
}

void nimcp_transfer_wait(nimcp_transfer_manager_t* mgr, uint64_t request_id) {
    if (!nimcp_transfer_manager_is_valid(mgr)) return;
    if (request_id == NIMCP_TRANSFER_INVALID_ID) return;

    nimcp_transfer_request_t* req = find_request(mgr, request_id);
    if (!req) return;

    if (req->status == NIMCP_TRANSFER_STATUS_IN_PROGRESS) {
        cudaSetDevice(mgr->ctx->device_id);
        cudaEventSynchronize(req->completion_event);

        req->end_time_ns = get_time_ns();
        req->status = NIMCP_TRANSFER_STATUS_COMPLETED;

        // Update statistics
        mgr->total_bytes_transferred += req->size;
        mgr->total_transfers++;
        double time_ms = (req->end_time_ns - req->start_time_ns) / 1000000.0;
        mgr->total_transfer_time_ms += time_ms;

        switch (req->direction) {
            case NIMCP_TRANSFER_H2D: mgr->h2d_bytes += req->size; break;
            case NIMCP_TRANSFER_D2H: mgr->d2h_bytes += req->size; break;
            case NIMCP_TRANSFER_D2D: case NIMCP_TRANSFER_P2P: mgr->d2d_bytes += req->size; break;
        }

        // Call callback
        if (req->callback) {
            req->callback(req->callback_data);
        }

        if (mgr->pending_count > 0) mgr->pending_count--;
    }
}

void nimcp_transfer_wait_all(nimcp_transfer_manager_t* mgr) {
    if (!nimcp_transfer_manager_is_valid(mgr)) return;

    cudaSetDevice(mgr->ctx->device_id);

    // Synchronize all streams
    for (int i = 0; i < mgr->num_streams; i++) {
        cudaStreamSynchronize(mgr->transfer_streams[i]);
    }

    // Process all completions
    nimcp_transfer_poll(mgr);
}

bool nimcp_transfer_is_complete(nimcp_transfer_manager_t* mgr, uint64_t request_id) {
    if (!nimcp_transfer_manager_is_valid(mgr)) return true;
    if (request_id == NIMCP_TRANSFER_INVALID_ID) return true;

    nimcp_transfer_request_t* req = find_request(mgr, request_id);
    if (!req) return true;

    if (req->status == NIMCP_TRANSFER_STATUS_IN_PROGRESS) {
        cudaError_t err = cudaEventQuery(req->completion_event);
        if (err == cudaSuccess) {
            req->end_time_ns = get_time_ns();
            req->status = NIMCP_TRANSFER_STATUS_COMPLETED;

            // Update statistics
            mgr->total_bytes_transferred += req->size;
            mgr->total_transfers++;
            double time_ms = (req->end_time_ns - req->start_time_ns) / 1000000.0;
            mgr->total_transfer_time_ms += time_ms;

            switch (req->direction) {
                case NIMCP_TRANSFER_H2D: mgr->h2d_bytes += req->size; break;
                case NIMCP_TRANSFER_D2H: mgr->d2h_bytes += req->size; break;
                case NIMCP_TRANSFER_D2D: case NIMCP_TRANSFER_P2P: mgr->d2d_bytes += req->size; break;
            }

            if (mgr->pending_count > 0) mgr->pending_count--;
            return true;
        }
        return false;
    }

    return req->status != NIMCP_TRANSFER_STATUS_PENDING;
}

nimcp_transfer_status_t nimcp_transfer_get_status(
    nimcp_transfer_manager_t* mgr, uint64_t request_id)
{
    if (!nimcp_transfer_manager_is_valid(mgr)) return NIMCP_TRANSFER_STATUS_FAILED;

    nimcp_transfer_request_t* req = find_request(mgr, request_id);
    if (!req) return NIMCP_TRANSFER_STATUS_FAILED;

    // Check if completed
    if (req->status == NIMCP_TRANSFER_STATUS_IN_PROGRESS) {
        nimcp_transfer_is_complete(mgr, request_id);
    }

    return req->status;
}

bool nimcp_transfer_cancel(nimcp_transfer_manager_t* mgr, uint64_t request_id) {
    if (!nimcp_transfer_manager_is_valid(mgr)) return false;

    nimcp_transfer_request_t* req = find_request(mgr, request_id);
    if (!req) return false;

    // Can only cancel pending (not yet started)
    if (req->status == NIMCP_TRANSFER_STATUS_PENDING) {
        req->status = NIMCP_TRANSFER_STATUS_CANCELLED;
        if (mgr->pending_count > 0) mgr->pending_count--;
        return true;
    }

    return false;
}

int nimcp_transfer_poll(nimcp_transfer_manager_t* mgr) {
    if (!nimcp_transfer_manager_is_valid(mgr)) return 0;

    int processed = 0;
    cudaSetDevice(mgr->ctx->device_id);

    for (size_t i = 0; i < NIMCP_TRANSFER_MAX_PENDING; i++) {
        nimcp_transfer_request_t* req = &mgr->pending[i];
        if (req->status == NIMCP_TRANSFER_STATUS_IN_PROGRESS) {
            cudaError_t err = cudaEventQuery(req->completion_event);
            if (err == cudaSuccess) {
                req->end_time_ns = get_time_ns();
                req->status = NIMCP_TRANSFER_STATUS_COMPLETED;

                // Update statistics
                mgr->total_bytes_transferred += req->size;
                mgr->total_transfers++;
                double time_ms = (req->end_time_ns - req->start_time_ns) / 1000000.0;
                mgr->total_transfer_time_ms += time_ms;

                switch (req->direction) {
                    case NIMCP_TRANSFER_H2D: mgr->h2d_bytes += req->size; break;
                    case NIMCP_TRANSFER_D2H: mgr->d2h_bytes += req->size; break;
                    case NIMCP_TRANSFER_D2D: case NIMCP_TRANSFER_P2P: mgr->d2d_bytes += req->size; break;
                }

                // Call callback
                if (req->callback) {
                    req->callback(req->callback_data);
                }

                if (mgr->pending_count > 0) mgr->pending_count--;
                processed++;
            }
        }
    }

    return processed;
}

//=============================================================================
// Pinned Memory API
//=============================================================================

void* nimcp_transfer_alloc_pinned(nimcp_transfer_manager_t* mgr, size_t size) {
    if (!nimcp_transfer_manager_is_valid(mgr) || size == 0) return NULL;

    void* ptr = NULL;

    // Try to allocate from pool first
    if (mgr->pinned_pool && mgr->pinned_pool_used + size <= mgr->pinned_pool_size) {
        ptr = (char*)mgr->pinned_pool + mgr->pinned_pool_used;
        mgr->pinned_pool_used += size;

        // Track allocation
        nimcp_pinned_alloc_t* alloc = (nimcp_pinned_alloc_t*)nimcp_calloc(
            1, sizeof(nimcp_pinned_alloc_t));
        if (alloc) {
            alloc->ptr = ptr;
            alloc->size = size;
            alloc->in_use = true;
            alloc->next = mgr->pinned_allocs;
            mgr->pinned_allocs = alloc;
            mgr->pinned_alloc_count++;
        }

        LOG_DEBUG("Allocated %zu bytes from pinned pool", size);
        return ptr;
    }

    // Fall back to direct allocation
    cudaSetDevice(mgr->ctx->device_id);
    cudaError_t err = cudaHostAlloc(&ptr, size, cudaHostAllocDefault);
    if (!check_cuda(err, "cudaHostAlloc")) {
        return NULL;
    }

    // Track allocation
    nimcp_pinned_alloc_t* alloc = (nimcp_pinned_alloc_t*)nimcp_calloc(
        1, sizeof(nimcp_pinned_alloc_t));
    if (alloc) {
        alloc->ptr = ptr;
        alloc->size = size;
        alloc->in_use = true;
        alloc->next = mgr->pinned_allocs;
        mgr->pinned_allocs = alloc;
        mgr->pinned_alloc_count++;
    }

    LOG_DEBUG("Allocated %zu bytes of pinned memory (direct)", size);
    return ptr;
}

void nimcp_transfer_free_pinned(nimcp_transfer_manager_t* mgr, void* ptr) {
    if (!mgr || !ptr) return;

    // Find allocation
    nimcp_pinned_alloc_t* prev = NULL;
    nimcp_pinned_alloc_t* alloc = mgr->pinned_allocs;
    while (alloc) {
        if (alloc->ptr == ptr) {
            // Check if from pool
            bool from_pool = (ptr >= mgr->pinned_pool &&
                             (char*)ptr < (char*)mgr->pinned_pool + mgr->pinned_pool_size);

            if (!from_pool) {
                // Direct allocation - free it
                cudaSetDevice(mgr->ctx->device_id);
                cudaFreeHost(ptr);
            } else {
                // Pool allocation - just mark as free
                // Note: simple pool doesn't support reuse, just tracking
            }

            // Remove from list
            if (prev) {
                prev->next = alloc->next;
            } else {
                mgr->pinned_allocs = alloc->next;
            }
            nimcp_free(alloc);
            if (mgr->pinned_alloc_count > 0) mgr->pinned_alloc_count--;
            return;
        }
        prev = alloc;
        alloc = alloc->next;
    }

    LOG_WARN("Attempt to free unknown pinned pointer: %p", ptr);
}

void nimcp_transfer_pinned_stats(
    const nimcp_transfer_manager_t* mgr,
    size_t* total_out, size_t* used_out, size_t* alloc_count_out)
{
    if (!mgr) {
        if (total_out) *total_out = 0;
        if (used_out) *used_out = 0;
        if (alloc_count_out) *alloc_count_out = 0;
        return;
    }

    if (total_out) *total_out = mgr->pinned_pool_size;
    if (used_out) *used_out = mgr->pinned_pool_used;
    if (alloc_count_out) *alloc_count_out = mgr->pinned_alloc_count;
}

//=============================================================================
// Transfer Statistics
//=============================================================================

void nimcp_transfer_get_stats(
    const nimcp_transfer_manager_t* mgr,
    size_t* total_bytes, size_t* total_count, double* avg_time_ms)
{
    if (!mgr) {
        if (total_bytes) *total_bytes = 0;
        if (total_count) *total_count = 0;
        if (avg_time_ms) *avg_time_ms = 0.0;
        return;
    }

    if (total_bytes) *total_bytes = mgr->total_bytes_transferred;
    if (total_count) *total_count = mgr->total_transfers;
    if (avg_time_ms) {
        *avg_time_ms = (mgr->total_transfers > 0)
            ? mgr->total_transfer_time_ms / (double)mgr->total_transfers
            : 0.0;
    }
}

void nimcp_transfer_reset_stats(nimcp_transfer_manager_t* mgr) {
    if (!mgr) return;

    mgr->total_bytes_transferred = 0;
    mgr->total_transfers = 0;
    mgr->total_transfer_time_ms = 0.0;
    mgr->h2d_bytes = 0;
    mgr->d2h_bytes = 0;
    mgr->d2d_bytes = 0;
}

//=============================================================================
// Double Buffer Implementation
//=============================================================================

nimcp_double_buffer_t* nimcp_double_buffer_create(
    nimcp_gpu_context_t* ctx, size_t buffer_size)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx) || buffer_size == 0) {
        LOG_ERROR("Invalid parameters for double buffer");
        return NULL;
    }

    nimcp_double_buffer_t* db = (nimcp_double_buffer_t*)nimcp_calloc(
        1, sizeof(nimcp_double_buffer_t));
    if (!db) {
        LOG_ERROR("Failed to allocate double buffer");
        return NULL;
    }

    db->ctx = ctx;
    db->buffer_size = buffer_size;
    db->active_index = 0;
    db->loading_index = 1;
    db->initialized = false;
    db->loading_in_progress = false;

    cudaError_t err = cudaSetDevice(ctx->device_id);
    if (!check_cuda(err, "cudaSetDevice")) {
        nimcp_free(db);
        return NULL;
    }

    // Allocate device buffers
    for (int i = 0; i < 2; i++) {
        err = cudaMalloc(&db->buffers[i], buffer_size);
        if (!check_cuda(err, "cudaMalloc")) {
            // Cleanup
            if (i > 0) cudaFree(db->buffers[0]);
            nimcp_free(db);
            return NULL;
        }
    }

    // Create streams
    err = cudaStreamCreate(&db->compute_stream);
    if (!check_cuda(err, "cudaStreamCreate(compute)")) {
        cudaFree(db->buffers[0]);
        cudaFree(db->buffers[1]);
        nimcp_free(db);
        return NULL;
    }

    err = cudaStreamCreate(&db->transfer_stream);
    if (!check_cuda(err, "cudaStreamCreate(transfer)")) {
        cudaStreamDestroy(db->compute_stream);
        cudaFree(db->buffers[0]);
        cudaFree(db->buffers[1]);
        nimcp_free(db);
        return NULL;
    }

    // Create events
    for (int i = 0; i < 2; i++) {
        err = cudaEventCreate(&db->transfer_done[i]);
        if (!check_cuda(err, "cudaEventCreate(transfer)")) {
            // Cleanup events created so far
            for (int j = 0; j < i; j++) {
                cudaEventDestroy(db->transfer_done[j]);
            }
            cudaStreamDestroy(db->transfer_stream);
            cudaStreamDestroy(db->compute_stream);
            cudaFree(db->buffers[0]);
            cudaFree(db->buffers[1]);
            nimcp_free(db);
            return NULL;
        }

        err = cudaEventCreate(&db->compute_done[i]);
        if (!check_cuda(err, "cudaEventCreate(compute)")) {
            // Cleanup
            cudaEventDestroy(db->transfer_done[i]);
            for (int j = 0; j < i; j++) {
                cudaEventDestroy(db->transfer_done[j]);
                cudaEventDestroy(db->compute_done[j]);
            }
            cudaStreamDestroy(db->transfer_stream);
            cudaStreamDestroy(db->compute_stream);
            cudaFree(db->buffers[0]);
            cudaFree(db->buffers[1]);
            nimcp_free(db);
            return NULL;
        }
    }

    db->initialized = true;

    LOG_INFO("Double buffer created: %zu bytes per buffer", buffer_size);

    return db;
}

void nimcp_double_buffer_destroy(nimcp_double_buffer_t* db) {
    if (!db) return;

    // Sync before cleanup
    nimcp_double_buffer_sync(db);

    if (db->ctx) {
        cudaSetDevice(db->ctx->device_id);
    }

    // Destroy events
    for (int i = 0; i < 2; i++) {
        if (db->transfer_done[i]) cudaEventDestroy(db->transfer_done[i]);
        if (db->compute_done[i]) cudaEventDestroy(db->compute_done[i]);
    }

    // Destroy streams
    if (db->transfer_stream) cudaStreamDestroy(db->transfer_stream);
    if (db->compute_stream) cudaStreamDestroy(db->compute_stream);

    // Free buffers
    for (int i = 0; i < 2; i++) {
        if (db->buffers[i]) cudaFree(db->buffers[i]);
    }

    LOG_INFO("Double buffer destroyed");

    nimcp_free(db);
}

int nimcp_double_buffer_start_load(
    nimcp_double_buffer_t* db,
    const void* host_data, size_t size)
{
    if (!db || !db->initialized || !host_data) {
        LOG_ERROR("Invalid double buffer or parameters");
        return -1;
    }

    if (size > db->buffer_size) {
        LOG_ERROR("Transfer size %zu exceeds buffer size %zu", size, db->buffer_size);
        return -1;
    }

    cudaSetDevice(db->ctx->device_id);

    // Wait for previous compute on loading buffer to complete
    cudaStreamWaitEvent(db->transfer_stream, db->compute_done[db->loading_index], 0);

    // Start async copy to loading buffer
    cudaError_t err = cudaMemcpyAsync(
        db->buffers[db->loading_index], host_data, size,
        cudaMemcpyHostToDevice, db->transfer_stream);
    if (!check_cuda(err, "cudaMemcpyAsync")) {
        return -1;
    }

    // Record transfer completion event
    err = cudaEventRecord(db->transfer_done[db->loading_index], db->transfer_stream);
    if (!check_cuda(err, "cudaEventRecord")) {
        return -1;
    }

    db->loading_in_progress = true;

    LOG_DEBUG("Started loading into buffer %d", db->loading_index);

    return 0;
}

int nimcp_double_buffer_start_load_callback(
    nimcp_double_buffer_t* db,
    const void* host_data, size_t size,
    nimcp_transfer_callback_t callback, void* user_data)
{
    int result = nimcp_double_buffer_start_load(db, host_data, size);
    if (result != 0) return result;

    // For callback support, we'd need to add a host callback mechanism
    // Using CUDA host functions (cudaLaunchHostFunc) or polling
    // For now, callback is called synchronously after wait
    if (callback) {
        // This is a simplified implementation - a production version would
        // use cudaLaunchHostFunc for true async callback
        cudaStreamSynchronize(db->transfer_stream);
        callback(user_data);
    }

    return 0;
}

void* nimcp_double_buffer_get_compute_buffer(nimcp_double_buffer_t* db) {
    if (!db || !db->initialized) return NULL;

    cudaSetDevice(db->ctx->device_id);

    // Wait for transfer to complete on active buffer (if any transfer happened)
    cudaStreamWaitEvent(db->compute_stream, db->transfer_done[db->active_index], 0);

    return db->buffers[db->active_index];
}

void* nimcp_double_buffer_wait_load(nimcp_double_buffer_t* db) {
    if (!db || !db->initialized) return NULL;

    cudaSetDevice(db->ctx->device_id);

    // Wait for loading to complete
    cudaEventSynchronize(db->transfer_done[db->loading_index]);
    db->loading_in_progress = false;

    return db->buffers[db->loading_index];
}

void nimcp_double_buffer_swap(nimcp_double_buffer_t* db) {
    if (!db || !db->initialized) return;

    cudaSetDevice(db->ctx->device_id);

    // Record compute completion on current active buffer
    cudaEventRecord(db->compute_done[db->active_index], db->compute_stream);

    // Swap indices
    int temp = db->active_index;
    db->active_index = db->loading_index;
    db->loading_index = temp;

    LOG_DEBUG("Swapped buffers: active=%d, loading=%d", db->active_index, db->loading_index);
}

void nimcp_double_buffer_sync(nimcp_double_buffer_t* db) {
    if (!db || !db->initialized) return;

    cudaSetDevice(db->ctx->device_id);
    cudaStreamSynchronize(db->compute_stream);
    cudaStreamSynchronize(db->transfer_stream);
    db->loading_in_progress = false;
}

nimcp_cuda_stream_t nimcp_double_buffer_get_compute_stream(nimcp_double_buffer_t* db) {
    if (!db || !db->initialized) return NULL;
    return db->compute_stream;
}

nimcp_cuda_stream_t nimcp_double_buffer_get_transfer_stream(nimcp_double_buffer_t* db) {
    if (!db || !db->initialized) return NULL;
    return db->transfer_stream;
}

bool nimcp_double_buffer_is_loading(const nimcp_double_buffer_t* db) {
    if (!db) return false;
    return db->loading_in_progress;
}

//=============================================================================
// Pipeline Implementation
//=============================================================================

nimcp_pipeline_t* nimcp_pipeline_create(nimcp_gpu_context_t* ctx, int num_stages) {
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx)) {
        LOG_ERROR("Invalid GPU context for pipeline");
        return NULL;
    }

    if (num_stages < 1) num_stages = 1;
    if (num_stages > NIMCP_PIPELINE_MAX_STAGES) num_stages = NIMCP_PIPELINE_MAX_STAGES;

    nimcp_pipeline_t* pipeline = (nimcp_pipeline_t*)nimcp_calloc(
        1, sizeof(nimcp_pipeline_t));
    if (!pipeline) {
        LOG_ERROR("Failed to allocate pipeline");
        return NULL;
    }

    pipeline->ctx = ctx;
    pipeline->num_stages = 0;
    pipeline->initialized = true;

    cudaSetDevice(ctx->device_id);

    // Initialize all stage slots
    for (int i = 0; i < num_stages; i++) {
        pipeline->stages[i].name = NULL;
        pipeline->stages[i].stream = NULL;
        pipeline->stages[i].start_event = NULL;
        pipeline->stages[i].end_event = NULL;
        pipeline->stages[i].execute = NULL;
        pipeline->stages[i].params = NULL;
        pipeline->stages[i].active = false;
    }

    LOG_INFO("Pipeline created with capacity for %d stages", num_stages);

    return pipeline;
}

void nimcp_pipeline_destroy(nimcp_pipeline_t* pipeline) {
    if (!pipeline) return;

    nimcp_pipeline_sync(pipeline);

    if (pipeline->ctx) {
        cudaSetDevice(pipeline->ctx->device_id);
    }

    // Destroy stage resources
    for (int i = 0; i < NIMCP_PIPELINE_MAX_STAGES; i++) {
        nimcp_pipeline_stage_t* stage = &pipeline->stages[i];
        if (stage->active) {
            if (stage->stream) cudaStreamDestroy(stage->stream);
            if (stage->start_event) cudaEventDestroy(stage->start_event);
            if (stage->end_event) cudaEventDestroy(stage->end_event);
        }
    }

    LOG_INFO("Pipeline destroyed with %d stages", pipeline->num_stages);

    nimcp_free(pipeline);
}

int nimcp_pipeline_add_stage(
    nimcp_pipeline_t* pipeline,
    const char* name,
    nimcp_pipeline_execute_fn execute,
    void* params)
{
    if (!pipeline || !pipeline->initialized) {
        LOG_ERROR("Invalid pipeline");
        return -1;
    }

    if (pipeline->num_stages >= NIMCP_PIPELINE_MAX_STAGES) {
        LOG_ERROR("Pipeline at maximum capacity (%d stages)", NIMCP_PIPELINE_MAX_STAGES);
        return -1;
    }

    if (!execute) {
        LOG_ERROR("Stage execute function is required");
        return -1;
    }

    int stage_idx = pipeline->num_stages;
    nimcp_pipeline_stage_t* stage = &pipeline->stages[stage_idx];

    cudaSetDevice(pipeline->ctx->device_id);

    // Create stream
    cudaError_t err = cudaStreamCreate(&stage->stream);
    if (!check_cuda(err, "cudaStreamCreate")) {
        return -1;
    }

    // Create events
    err = cudaEventCreate(&stage->start_event);
    if (!check_cuda(err, "cudaEventCreate(start)")) {
        cudaStreamDestroy(stage->stream);
        return -1;
    }

    err = cudaEventCreate(&stage->end_event);
    if (!check_cuda(err, "cudaEventCreate(end)")) {
        cudaEventDestroy(stage->start_event);
        cudaStreamDestroy(stage->stream);
        return -1;
    }

    stage->name = name ? name : "unnamed";
    stage->execute = execute;
    stage->params = params;
    stage->active = true;

    pipeline->num_stages++;

    LOG_INFO("Added pipeline stage %d: %s", stage_idx, stage->name);

    return stage_idx;
}

int nimcp_pipeline_get_stage_count(const nimcp_pipeline_t* pipeline) {
    if (!pipeline) return 0;
    return pipeline->num_stages;
}

nimcp_pipeline_stage_t* nimcp_pipeline_get_stage(nimcp_pipeline_t* pipeline, int index) {
    if (!pipeline || index < 0 || index >= pipeline->num_stages) return NULL;
    return &pipeline->stages[index];
}

void nimcp_pipeline_execute(
    nimcp_pipeline_t* pipeline,
    void** inputs, void** outputs, int num_batches)
{
    if (!pipeline || !pipeline->initialized || num_batches <= 0) return;
    if (!inputs || !outputs) return;
    if (pipeline->num_stages == 0) return;

    cudaSetDevice(pipeline->ctx->device_id);

    // For each batch
    for (int batch = 0; batch < num_batches; batch++) {
        void* input = inputs[batch];
        void* output = outputs[batch];

        // For each stage
        for (int s = 0; s < pipeline->num_stages; s++) {
            nimcp_pipeline_stage_t* stage = &pipeline->stages[s];
            if (!stage->active) continue;

            // Wait for previous stage of this batch (if not first stage)
            if (s > 0) {
                nimcp_pipeline_stage_t* prev_stage = &pipeline->stages[s - 1];
                cudaStreamWaitEvent(stage->stream, prev_stage->end_event, 0);
            }

            // Wait for this stage's completion of previous batch (if not first batch)
            if (batch > 0) {
                cudaStreamWaitEvent(stage->stream, stage->end_event, 0);
            }

            // Record start event
            cudaEventRecord(stage->start_event, stage->stream);

            // Execute stage
            // For intermediate stages, use temporary buffers
            // Simplified: just pass input/output directly
            if (stage->execute) {
                stage->execute(input, output, stage->params);
            }

            // Record end event
            cudaEventRecord(stage->end_event, stage->stream);
        }
    }

    // Sync all stages
    nimcp_pipeline_sync(pipeline);
}

void nimcp_pipeline_execute_single(
    nimcp_pipeline_t* pipeline,
    void* input, void* output)
{
    void* inputs[1] = { input };
    void* outputs[1] = { output };
    nimcp_pipeline_execute(pipeline, inputs, outputs, 1);
}

void nimcp_pipeline_sync(nimcp_pipeline_t* pipeline) {
    if (!pipeline || !pipeline->initialized) return;

    cudaSetDevice(pipeline->ctx->device_id);

    for (int i = 0; i < pipeline->num_stages; i++) {
        if (pipeline->stages[i].active && pipeline->stages[i].stream) {
            cudaStreamSynchronize(pipeline->stages[i].stream);
        }
    }
}

void nimcp_pipeline_reset(nimcp_pipeline_t* pipeline) {
    if (!pipeline) return;
    // Currently no state to reset between runs
    // Future: could reset timing statistics
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_transfer_direction_name(nimcp_transfer_direction_t direction) {
    switch (direction) {
        case NIMCP_TRANSFER_H2D: return "H2D";
        case NIMCP_TRANSFER_D2H: return "D2H";
        case NIMCP_TRANSFER_D2D: return "D2D";
        case NIMCP_TRANSFER_P2P: return "P2P";
        default: return "UNKNOWN";
    }
}

const char* nimcp_transfer_status_name(nimcp_transfer_status_t status) {
    switch (status) {
        case NIMCP_TRANSFER_STATUS_PENDING: return "PENDING";
        case NIMCP_TRANSFER_STATUS_IN_PROGRESS: return "IN_PROGRESS";
        case NIMCP_TRANSFER_STATUS_COMPLETED: return "COMPLETED";
        case NIMCP_TRANSFER_STATUS_FAILED: return "FAILED";
        case NIMCP_TRANSFER_STATUS_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

void nimcp_transfer_manager_print_info(const nimcp_transfer_manager_t* mgr) {
    if (!mgr) {
        printf("Transfer Manager: NULL\n");
        return;
    }

    printf("Transfer Manager:\n");
    printf("  Initialized: %s\n", mgr->initialized ? "yes" : "no");
    printf("  Streams: %d\n", mgr->num_streams);
    printf("  Pending transfers: %zu\n", mgr->pending_count);
    printf("  Statistics:\n");
    printf("    Total transfers: %zu\n", mgr->total_transfers);
    printf("    Total bytes: %zu (%.2f MB)\n",
           mgr->total_bytes_transferred,
           (double)mgr->total_bytes_transferred / (1024.0 * 1024.0));
    printf("    H2D bytes: %zu\n", mgr->h2d_bytes);
    printf("    D2H bytes: %zu\n", mgr->d2h_bytes);
    printf("    D2D bytes: %zu\n", mgr->d2d_bytes);
    if (mgr->total_transfers > 0) {
        printf("    Avg time: %.3f ms\n",
               mgr->total_transfer_time_ms / (double)mgr->total_transfers);
    }
    printf("  Pinned pool:\n");
    printf("    Size: %zu bytes (%.2f MB)\n",
           mgr->pinned_pool_size,
           (double)mgr->pinned_pool_size / (1024.0 * 1024.0));
    printf("    Used: %zu bytes (%.1f%%)\n",
           mgr->pinned_pool_used,
           mgr->pinned_pool_size > 0
               ? (double)mgr->pinned_pool_used / (double)mgr->pinned_pool_size * 100.0
               : 0.0);
    printf("    Allocations: %zu\n", mgr->pinned_alloc_count);
}

void nimcp_double_buffer_print_info(const nimcp_double_buffer_t* db) {
    if (!db) {
        printf("Double Buffer: NULL\n");
        return;
    }

    printf("Double Buffer:\n");
    printf("  Initialized: %s\n", db->initialized ? "yes" : "no");
    printf("  Buffer size: %zu bytes (%.2f MB)\n",
           db->buffer_size, (double)db->buffer_size / (1024.0 * 1024.0));
    printf("  Active index: %d\n", db->active_index);
    printf("  Loading index: %d\n", db->loading_index);
    printf("  Loading in progress: %s\n", db->loading_in_progress ? "yes" : "no");
}

void nimcp_pipeline_print_info(const nimcp_pipeline_t* pipeline) {
    if (!pipeline) {
        printf("Pipeline: NULL\n");
        return;
    }

    printf("Pipeline:\n");
    printf("  Initialized: %s\n", pipeline->initialized ? "yes" : "no");
    printf("  Stages: %d\n", pipeline->num_stages);
    for (int i = 0; i < pipeline->num_stages; i++) {
        const nimcp_pipeline_stage_t* stage = &pipeline->stages[i];
        printf("    Stage %d: %s (active=%s)\n",
               i, stage->name ? stage->name : "unnamed",
               stage->active ? "yes" : "no");
    }
}

#else /* !NIMCP_ENABLE_CUDA */

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

nimcp_transfer_manager_t* nimcp_transfer_manager_create(
    nimcp_gpu_context_t* ctx, int num_streams, size_t pinned_pool_size)
{
    (void)ctx; (void)num_streams; (void)pinned_pool_size;
    return NULL;
}

void nimcp_transfer_manager_destroy(nimcp_transfer_manager_t* mgr) { (void)mgr; }
bool nimcp_transfer_manager_is_valid(const nimcp_transfer_manager_t* mgr) { (void)mgr; return false; }

uint64_t nimcp_transfer_async(
    nimcp_transfer_manager_t* mgr, void* dst, const void* src, size_t size,
    nimcp_transfer_direction_t direction,
    nimcp_transfer_callback_t callback, void* callback_data)
{
    (void)mgr; (void)dst; (void)src; (void)size; (void)direction;
    (void)callback; (void)callback_data;
    return NIMCP_TRANSFER_INVALID_ID;
}

void nimcp_transfer_wait(nimcp_transfer_manager_t* mgr, uint64_t request_id) { (void)mgr; (void)request_id; }
void nimcp_transfer_wait_all(nimcp_transfer_manager_t* mgr) { (void)mgr; }
bool nimcp_transfer_is_complete(nimcp_transfer_manager_t* mgr, uint64_t request_id) { (void)mgr; (void)request_id; return true; }
nimcp_transfer_status_t nimcp_transfer_get_status(nimcp_transfer_manager_t* mgr, uint64_t request_id) { (void)mgr; (void)request_id; return NIMCP_TRANSFER_STATUS_FAILED; }
bool nimcp_transfer_cancel(nimcp_transfer_manager_t* mgr, uint64_t request_id) { (void)mgr; (void)request_id; return false; }
int nimcp_transfer_poll(nimcp_transfer_manager_t* mgr) { (void)mgr; return 0; }

void* nimcp_transfer_alloc_pinned(nimcp_transfer_manager_t* mgr, size_t size) { (void)mgr; (void)size; return NULL; }
void nimcp_transfer_free_pinned(nimcp_transfer_manager_t* mgr, void* ptr) { (void)mgr; (void)ptr; }
void nimcp_transfer_pinned_stats(const nimcp_transfer_manager_t* mgr, size_t* total_out, size_t* used_out, size_t* alloc_count_out) {
    (void)mgr;
    if (total_out) *total_out = 0;
    if (used_out) *used_out = 0;
    if (alloc_count_out) *alloc_count_out = 0;
}

void nimcp_transfer_get_stats(const nimcp_transfer_manager_t* mgr, size_t* total_bytes, size_t* total_count, double* avg_time_ms) {
    (void)mgr;
    if (total_bytes) *total_bytes = 0;
    if (total_count) *total_count = 0;
    if (avg_time_ms) *avg_time_ms = 0.0;
}
void nimcp_transfer_reset_stats(nimcp_transfer_manager_t* mgr) { (void)mgr; }

nimcp_double_buffer_t* nimcp_double_buffer_create(nimcp_gpu_context_t* ctx, size_t buffer_size) { (void)ctx; (void)buffer_size; return NULL; }
void nimcp_double_buffer_destroy(nimcp_double_buffer_t* db) { (void)db; }
int nimcp_double_buffer_start_load(nimcp_double_buffer_t* db, const void* host_data, size_t size) { (void)db; (void)host_data; (void)size; return -1; }
int nimcp_double_buffer_start_load_callback(nimcp_double_buffer_t* db, const void* host_data, size_t size, nimcp_transfer_callback_t callback, void* user_data) { (void)db; (void)host_data; (void)size; (void)callback; (void)user_data; return -1; }
void* nimcp_double_buffer_get_compute_buffer(nimcp_double_buffer_t* db) { (void)db; return NULL; }
void* nimcp_double_buffer_wait_load(nimcp_double_buffer_t* db) { (void)db; return NULL; }
void nimcp_double_buffer_swap(nimcp_double_buffer_t* db) { (void)db; }
void nimcp_double_buffer_sync(nimcp_double_buffer_t* db) { (void)db; }
nimcp_cuda_stream_t nimcp_double_buffer_get_compute_stream(nimcp_double_buffer_t* db) { (void)db; return NULL; }
nimcp_cuda_stream_t nimcp_double_buffer_get_transfer_stream(nimcp_double_buffer_t* db) { (void)db; return NULL; }
bool nimcp_double_buffer_is_loading(const nimcp_double_buffer_t* db) { (void)db; return false; }

nimcp_pipeline_t* nimcp_pipeline_create(nimcp_gpu_context_t* ctx, int num_stages) { (void)ctx; (void)num_stages; return NULL; }
void nimcp_pipeline_destroy(nimcp_pipeline_t* pipeline) { (void)pipeline; }
int nimcp_pipeline_add_stage(nimcp_pipeline_t* pipeline, const char* name, nimcp_pipeline_execute_fn execute, void* params) { (void)pipeline; (void)name; (void)execute; (void)params; return -1; }
int nimcp_pipeline_get_stage_count(const nimcp_pipeline_t* pipeline) { (void)pipeline; return 0; }
nimcp_pipeline_stage_t* nimcp_pipeline_get_stage(nimcp_pipeline_t* pipeline, int index) { (void)pipeline; (void)index; return NULL; }
void nimcp_pipeline_execute(nimcp_pipeline_t* pipeline, void** inputs, void** outputs, int num_batches) { (void)pipeline; (void)inputs; (void)outputs; (void)num_batches; }
void nimcp_pipeline_execute_single(nimcp_pipeline_t* pipeline, void* input, void* output) { (void)pipeline; (void)input; (void)output; }
void nimcp_pipeline_sync(nimcp_pipeline_t* pipeline) { (void)pipeline; }
void nimcp_pipeline_reset(nimcp_pipeline_t* pipeline) { (void)pipeline; }

const char* nimcp_transfer_direction_name(nimcp_transfer_direction_t direction) {
    switch (direction) {
        case NIMCP_TRANSFER_H2D: return "H2D";
        case NIMCP_TRANSFER_D2H: return "D2H";
        case NIMCP_TRANSFER_D2D: return "D2D";
        case NIMCP_TRANSFER_P2P: return "P2P";
        default: return "UNKNOWN";
    }
}

const char* nimcp_transfer_status_name(nimcp_transfer_status_t status) {
    switch (status) {
        case NIMCP_TRANSFER_STATUS_PENDING: return "PENDING";
        case NIMCP_TRANSFER_STATUS_IN_PROGRESS: return "IN_PROGRESS";
        case NIMCP_TRANSFER_STATUS_COMPLETED: return "COMPLETED";
        case NIMCP_TRANSFER_STATUS_FAILED: return "FAILED";
        case NIMCP_TRANSFER_STATUS_CANCELLED: return "CANCELLED";
        default: return "UNKNOWN";
    }
}

void nimcp_transfer_manager_print_info(const nimcp_transfer_manager_t* mgr) { (void)mgr; printf("Transfer Manager: CUDA not available\n"); }
void nimcp_double_buffer_print_info(const nimcp_double_buffer_t* db) { (void)db; printf("Double Buffer: CUDA not available\n"); }
void nimcp_pipeline_print_info(const nimcp_pipeline_t* pipeline) { (void)pipeline; printf("Pipeline: CUDA not available\n"); }

#endif /* NIMCP_ENABLE_CUDA */
