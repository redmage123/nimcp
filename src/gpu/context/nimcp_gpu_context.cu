/**
 * @file nimcp_gpu_context.cu
 * @brief GPU Context Manager Implementation
 *
 * WHAT: Implementation of GPU context management for CUDA operations
 * WHY:  Centralizes GPU resource management and provides unified API
 * HOW:  Uses CUDA runtime, cuBLAS, and cuFFT libraries
 *
 * COMPILATION:
 * - Compiled only when CUDAToolkit is found (CMake conditional)
 * - CPU fallback stubs provided for non-CUDA builds
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

// Include CUDA headers FIRST to avoid extern "C" conflicts with C++ operators
#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cufft.h>
#endif

// Then include project headers (which have extern "C" blocks)
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LOG_MODULE "GPU_CONTEXT"

//=============================================================================
// CUDA Implementation
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Helper to check CUDA errors
 */
static inline bool check_cuda_error(nimcp_gpu_context_t* ctx, cudaError_t err, const char* op) {
    if (err != cudaSuccess) {
        ctx->last_error = (int)err;
        snprintf(ctx->last_error_msg, sizeof(ctx->last_error_msg),
                 "CUDA error in %s: %s", op, cudaGetErrorString(err));
        LOG_ERROR("%s", ctx->last_error_msg);
        return false;
    }
    return true;
}

/**
 * @brief Helper to check cuBLAS errors
 */
static inline bool check_cublas_error(nimcp_gpu_context_t* ctx, cublasStatus_t status, const char* op) {
    if (status != CUBLAS_STATUS_SUCCESS) {
        ctx->last_error = (int)status;
        snprintf(ctx->last_error_msg, sizeof(ctx->last_error_msg),
                 "cuBLAS error in %s: status=%d", op, (int)status);
        LOG_ERROR("%s", ctx->last_error_msg);
        return false;
    }
    return true;
}

nimcp_gpu_context_t* nimcp_gpu_context_create(int device_id) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Check if CUDA is available
    if (!gpu_is_available()) {
        LOG_INFO("No GPU available, GPU context creation skipped");
        return NULL;
    }

    // Get device count
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    if (err != cudaSuccess || device_count == 0) {
        LOG_INFO("No CUDA devices found");
        return NULL;
    }

    // Auto-select best device if requested
    if (device_id < 0) {
        device_id = gpu_get_best_device();
        if (device_id < 0) device_id = 0;
    }

    // Validate device ID
    if (device_id >= device_count) {
        LOG_ERROR("Invalid device ID %d (only %d devices available)", device_id, device_count);
        return NULL;
    }

    // Allocate context
    nimcp_gpu_context_t* ctx = (nimcp_gpu_context_t*)nimcp_calloc(1, sizeof(nimcp_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate GPU context");
        return NULL;
    }

    ctx->device_id = device_id;

    // Set device
    err = cudaSetDevice(device_id);
    if (!check_cuda_error(ctx, err, "cudaSetDevice")) {
        nimcp_free(ctx);
        return NULL;
    }

    // Get device info
    if (!gpu_get_device_info((uint32_t)device_id, &ctx->device_info)) {
        // Fallback: get basic info from CUDA
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, device_id) == cudaSuccess) {
            strncpy(ctx->device_info.name, prop.name, sizeof(ctx->device_info.name) - 1);
            ctx->device_info.total_memory_bytes = prop.totalGlobalMem;
            ctx->device_info.compute_units = prop.multiProcessorCount;
            ctx->device_info.max_threads_per_block = prop.maxThreadsPerBlock;
            ctx->device_info.warp_size = prop.warpSize;
            ctx->device_info.compute_capability_major = prop.major;
            ctx->device_info.compute_capability_minor = prop.minor;
        }
    }

    // Create compute stream
    err = cudaStreamCreate(&ctx->compute_stream);
    if (!check_cuda_error(ctx, err, "cudaStreamCreate(compute)")) {
        nimcp_free(ctx);
        return NULL;
    }

    // Create transfer stream
    err = cudaStreamCreate(&ctx->transfer_stream);
    if (!check_cuda_error(ctx, err, "cudaStreamCreate(transfer)")) {
        cudaStreamDestroy(ctx->compute_stream);
        nimcp_free(ctx);
        return NULL;
    }

    // Create auxiliary stream
    err = cudaStreamCreate(&ctx->aux_stream);
    if (!check_cuda_error(ctx, err, "cudaStreamCreate(aux)")) {
        cudaStreamDestroy(ctx->transfer_stream);
        cudaStreamDestroy(ctx->compute_stream);
        nimcp_free(ctx);
        return NULL;
    }

    // Initialize cuBLAS
    cublasStatus_t cublas_status = cublasCreate(&ctx->cublas_handle);
    if (cublas_status == CUBLAS_STATUS_SUCCESS) {
        // Set cuBLAS to use our compute stream
        cublasSetStream(ctx->cublas_handle, ctx->compute_stream);
        ctx->cublas_initialized = true;
    } else {
        LOG_WARN("Failed to initialize cuBLAS (status=%d), BLAS ops will use fallback",
                 (int)cublas_status);
        ctx->cublas_initialized = false;
    }

    // cuFFT plans will be created on-demand
    ctx->cufft_initialized = false;

    // Memory tracking
    ctx->total_memory = ctx->device_info.total_memory_bytes;
    ctx->allocated_memory = 0;
    ctx->peak_memory = 0;
    ctx->allocation_count = 0;
    ctx->deallocation_count = 0;

    // Kernel config defaults
    ctx->default_block_size = 256;
    ctx->max_blocks_per_grid = 65535;
    ctx->warp_size = ctx->device_info.warp_size > 0 ? ctx->device_info.warp_size : 32;

    // Execution mode
    ctx->execution_mode = EXEC_MODE_GPU_CUDA;

    // Stream pool for concurrent kernel execution
    ctx->stream_pool_count = 0;
    __atomic_store_n(&ctx->stream_pool_next, 0, __ATOMIC_RELAXED);
    for (uint32_t i = 0; i < NIMCP_GPU_STREAM_POOL_SIZE; i++) {
        cudaError_t serr = cudaStreamCreateWithFlags(&ctx->stream_pool[i], cudaStreamNonBlocking);
        if (serr != cudaSuccess) {
            LOG_WARN("Failed to create stream pool[%u]: %s", i, cudaGetErrorString(serr));
            break;
        }
        ctx->stream_pool_count++;
    }
    LOG_INFO("GPU stream pool: %u/%d streams created", ctx->stream_pool_count, NIMCP_GPU_STREAM_POOL_SIZE);

    ctx->initialized = true;
    ctx->last_error = 0;
    ctx->last_error_msg[0] = '\0';

    LOG_INFO("GPU context created: device %d (%s), %lu MB memory, CC %d.%d",
             device_id, ctx->device_info.name,
             (unsigned long)(ctx->total_memory / (1024 * 1024)),
             ctx->device_info.compute_capability_major,
             ctx->device_info.compute_capability_minor);

    return ctx;
}

nimcp_gpu_context_t* nimcp_gpu_context_create_auto(void) {
    return nimcp_gpu_context_create(-1);
}

void nimcp_gpu_context_destroy(nimcp_gpu_context_t* ctx) {
    if (!ctx) return;

    // Synchronize before cleanup
    if (ctx->initialized) {
        cudaSetDevice(ctx->device_id);
        cudaDeviceSynchronize();
    }

    // Destroy cuFFT plans
    if (ctx->cufft_initialized) {
        if (ctx->cufft_plan_1d) cufftDestroy(ctx->cufft_plan_1d);
        if (ctx->cufft_plan_2d) cufftDestroy(ctx->cufft_plan_2d);
    }

    // Destroy cuBLAS handle
    if (ctx->cublas_initialized) {
        cublasDestroy(ctx->cublas_handle);
    }

    // Destroy stream pool
    for (uint32_t i = 0; i < ctx->stream_pool_count; i++) {
        if (ctx->stream_pool[i]) cudaStreamDestroy(ctx->stream_pool[i]);
    }
    ctx->stream_pool_count = 0;

    // Destroy streams
    if (ctx->aux_stream) cudaStreamDestroy(ctx->aux_stream);
    if (ctx->transfer_stream) cudaStreamDestroy(ctx->transfer_stream);
    if (ctx->compute_stream) cudaStreamDestroy(ctx->compute_stream);

    LOG_INFO("GPU context destroyed: device %d, peak memory %lu MB, %lu allocs, %lu frees",
             ctx->device_id,
             (unsigned long)(ctx->peak_memory / (1024 * 1024)),
             (unsigned long)ctx->allocation_count,
             (unsigned long)ctx->deallocation_count);

    nimcp_free(ctx);
}

bool nimcp_gpu_context_is_valid(const nimcp_gpu_context_t* ctx) {
    return ctx && ctx->initialized;
}

int nimcp_gpu_context_set_device(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx)) return -1;

    cudaError_t err = cudaSetDevice(ctx->device_id);
    if (!check_cuda_error(ctx, err, "cudaSetDevice")) {
        return -1;
    }
    return 0;
}

int nimcp_gpu_context_synchronize(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx)) return -1;

    cudaError_t err = cudaDeviceSynchronize();
    if (!check_cuda_error(ctx, err, "cudaDeviceSynchronize")) {
        return -1;
    }
    return 0;
}

const char* nimcp_gpu_context_get_error(const nimcp_gpu_context_t* ctx) {
    if (!ctx) return "NULL context";
    if (ctx->last_error_msg[0] == '\0') return "No error";
    return ctx->last_error_msg;
}

void* nimcp_gpu_malloc(nimcp_gpu_context_t* ctx, size_t size_bytes) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx) || size_bytes == 0) return NULL;

    cudaSetDevice(ctx->device_id);

    void* dev_ptr = NULL;
    cudaError_t err = cudaMalloc(&dev_ptr, size_bytes);
    if (err != cudaSuccess) {
        // Try recovery for OOM - validate result before retrying
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)
            && result.success) {
            err = cudaMalloc(&dev_ptr, size_bytes);
        }
        if (!check_cuda_error(ctx, err, "cudaMalloc")) {
            return NULL;
        }
    }

    /* P2: Use atomic operations for thread-safe memory counter updates */
    __atomic_fetch_add(&ctx->allocated_memory, size_bytes, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&ctx->allocation_count, 1, __ATOMIC_SEQ_CST);
    size_t current_allocated = __atomic_load_n(&ctx->allocated_memory, __ATOMIC_SEQ_CST);
    size_t current_peak = __atomic_load_n(&ctx->peak_memory, __ATOMIC_SEQ_CST);
    while (current_allocated > current_peak) {
        if (__atomic_compare_exchange_n(&ctx->peak_memory, &current_peak,
                                         current_allocated, false,
                                         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            break;
        }
    }

    return dev_ptr;
}

void nimcp_gpu_free(nimcp_gpu_context_t* ctx, void* dev_ptr) {
    if (!nimcp_gpu_context_is_valid(ctx) || !dev_ptr) return;

    cudaSetDevice(ctx->device_id);
    cudaFree(dev_ptr);
    /* P2: GPU memory tracking - cannot decrement allocated_memory without knowing
     * the allocation size. Would need a size-tracking hash table for full tracking.
     * For now, deallocation_count is tracked atomically. */
    __atomic_fetch_add(&ctx->deallocation_count, 1, __ATOMIC_SEQ_CST);
}

int nimcp_gpu_memcpy(nimcp_gpu_context_t* ctx,
                     void* dst, const void* src,
                     size_t size_bytes,
                     nimcp_gpu_memcpy_kind_t kind) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx)) return -1;
    if (!dst || !src || size_bytes == 0) return -1;

    cudaSetDevice(ctx->device_id);

    cudaMemcpyKind cuda_kind;
    switch (kind) {
        case GPU_MEMCPY_HOST_TO_DEVICE:   cuda_kind = cudaMemcpyHostToDevice; break;
        case GPU_MEMCPY_DEVICE_TO_HOST:   cuda_kind = cudaMemcpyDeviceToHost; break;
        case GPU_MEMCPY_DEVICE_TO_DEVICE: cuda_kind = cudaMemcpyDeviceToDevice; break;
        case GPU_MEMCPY_HOST_TO_HOST:     cuda_kind = cudaMemcpyHostToHost; break;
        default: return -1;
    }

    cudaError_t err = cudaMemcpy(dst, src, size_bytes, cuda_kind);
    if (err != cudaSuccess) {
        // Try recovery - validate result before retrying
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &result)
            && result.success) {
            err = cudaMemcpy(dst, src, size_bytes, cuda_kind);
        }
        if (!check_cuda_error(ctx, err, "cudaMemcpy")) {
            return -1;
        }
    }
    return 0;
}

int nimcp_gpu_memcpy_async(nimcp_gpu_context_t* ctx,
                           void* dst, const void* src,
                           size_t size_bytes,
                           nimcp_gpu_memcpy_kind_t kind,
                           bool use_transfer_stream) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx)) return -1;
    if (!dst || !src || size_bytes == 0) return -1;

    cudaSetDevice(ctx->device_id);

    cudaMemcpyKind cuda_kind;
    switch (kind) {
        case GPU_MEMCPY_HOST_TO_DEVICE:   cuda_kind = cudaMemcpyHostToDevice; break;
        case GPU_MEMCPY_DEVICE_TO_HOST:   cuda_kind = cudaMemcpyDeviceToHost; break;
        case GPU_MEMCPY_DEVICE_TO_DEVICE: cuda_kind = cudaMemcpyDeviceToDevice; break;
        case GPU_MEMCPY_HOST_TO_HOST:     cuda_kind = cudaMemcpyHostToHost; break;
        default: return -1;
    }

    cudaStream_t stream = use_transfer_stream ? ctx->transfer_stream : ctx->compute_stream;
    cudaError_t err = cudaMemcpyAsync(dst, src, size_bytes, cuda_kind, stream);
    if (err != cudaSuccess) {
        // Try recovery - validate result before retrying
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &result)
            && result.success) {
            err = cudaMemcpyAsync(dst, src, size_bytes, cuda_kind, stream);
        }
        if (!check_cuda_error(ctx, err, "cudaMemcpyAsync")) {
            return -1;
        }
    }
    return 0;
}

int nimcp_gpu_memset(nimcp_gpu_context_t* ctx,
                     void* dev_ptr, int value,
                     size_t size_bytes) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_context_is_valid(ctx)) return -1;
    if (!dev_ptr || size_bytes == 0) return -1;

    cudaSetDevice(ctx->device_id);

    cudaError_t err = cudaMemset(dev_ptr, value, size_bytes);
    if (err != cudaSuccess) {
        // Try recovery - validate result before retrying
        nimcp_gpu_recovery_result_t result = {0};
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_CUDA_RUNTIME, err, &result)
            && result.success) {
            err = cudaMemset(dev_ptr, value, size_bytes);
        }
        if (!check_cuda_error(ctx, err, "cudaMemset")) {
            return -1;
        }
    }
    return 0;
}

void nimcp_gpu_memory_stats(const nimcp_gpu_context_t* ctx,
                            size_t* allocated_out,
                            size_t* peak_out,
                            size_t* free_out) {
    if (!nimcp_gpu_context_is_valid(ctx)) {
        if (allocated_out) *allocated_out = 0;
        if (peak_out) *peak_out = 0;
        if (free_out) *free_out = 0;
        return;
    }

    if (allocated_out) *allocated_out = ctx->allocated_memory;
    if (peak_out) *peak_out = ctx->peak_memory;

    if (free_out) {
        size_t free_mem = 0, total_mem = 0;
        cudaSetDevice(ctx->device_id);
        cudaMemGetInfo(&free_mem, &total_mem);
        *free_out = free_mem;
    }
}

/* P3: Add const to pure getter function parameter types */
nimcp_cuda_stream_t nimcp_gpu_get_compute_stream(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx)) return NULL;
    return ctx->compute_stream;
}

nimcp_cuda_stream_t nimcp_gpu_get_transfer_stream(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx)) return NULL;
    return ctx->transfer_stream;
}

int nimcp_gpu_stream_synchronize(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx)) return -1;

    cudaSetDevice(ctx->device_id);
    cudaError_t err = cudaStreamSynchronize(ctx->compute_stream);
    if (!check_cuda_error(ctx, err, "cudaStreamSynchronize")) {
        return -1;
    }
    return 0;
}

nimcp_cuda_stream_t nimcp_gpu_get_pool_stream(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx) || ctx->stream_pool_count == 0) {
        return ctx ? ctx->compute_stream : NULL;
    }
    uint32_t idx = __atomic_fetch_add(&ctx->stream_pool_next, 1, __ATOMIC_RELAXED) % ctx->stream_pool_count;
    return ctx->stream_pool[idx];
}

void nimcp_gpu_sync_pool(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx)) return;
    cudaSetDevice(ctx->device_id);
    for (uint32_t i = 0; i < ctx->stream_pool_count; i++) {
        if (ctx->stream_pool[i]) {
            cudaStreamSynchronize(ctx->stream_pool[i]);
        }
    }
}

nimcp_cublas_handle_t nimcp_gpu_get_cublas(nimcp_gpu_context_t* ctx) {
    if (!nimcp_gpu_context_is_valid(ctx) || !ctx->cublas_initialized) return NULL;
    return ctx->cublas_handle;
}

nimcp_cufft_handle_t nimcp_gpu_get_cufft_1d(nimcp_gpu_context_t* ctx, int n) {
    if (!nimcp_gpu_context_is_valid(ctx)) return 0;

    /* P2-C1: Double-checked locking for thread-safe cuFFT plan creation.
     * Use atomic load/store to avoid data race on cufft_initialized flag. */
    if (!__atomic_load_n(&ctx->cufft_initialized, __ATOMIC_ACQUIRE) || ctx->cufft_plan_1d == 0) {
        /* Per-context spinlock for cuFFT plan creation.
         * Was a global static spinlock — caused races when multiple contexts
         * created plans concurrently (each context's plan is independent). */
        while (__atomic_exchange_n(&ctx->cufft_creating, 1, __ATOMIC_ACQUIRE) != 0) {
            /* Spin - cuFFT plan creation is rare and fast */
        }
        /* Re-check after acquiring per-context spinlock */
        if (!__atomic_load_n(&ctx->cufft_initialized, __ATOMIC_ACQUIRE) || ctx->cufft_plan_1d == 0) {
            cufftResult result = cufftPlan1d(&ctx->cufft_plan_1d, n, CUFFT_C2C, 1);
            if (result != CUFFT_SUCCESS) {
                LOG_ERROR("Failed to create cuFFT 1D plan (result=%d)", (int)result);
                __atomic_store_n(&ctx->cufft_creating, 0, __ATOMIC_RELEASE);
                return 0;
            }
            cufftSetStream(ctx->cufft_plan_1d, ctx->compute_stream);
            __atomic_store_n(&ctx->cufft_initialized, true, __ATOMIC_RELEASE);
        }
        __atomic_store_n(&ctx->cufft_creating, 0, __ATOMIC_RELEASE);
    }

    return ctx->cufft_plan_1d;
}

void nimcp_gpu_calc_launch_config(const nimcp_gpu_context_t* ctx,
                                  uint32_t num_elements,
                                  uint32_t* block_size_out,
                                  uint32_t* grid_size_out) {
    if (!ctx || !block_size_out || !grid_size_out) return;

    uint32_t block_size = ctx->default_block_size;
    uint32_t grid_size = (num_elements + block_size - 1) / block_size;

    // Clamp grid size
    if (grid_size > ctx->max_blocks_per_grid) {
        grid_size = ctx->max_blocks_per_grid;
    }

    *block_size_out = block_size;
    *grid_size_out = grid_size;
}

uint32_t nimcp_gpu_get_optimal_block_size(const nimcp_gpu_context_t* ctx,
                                          size_t shared_mem_per_block) {
    if (!ctx) return 256;

    // Simple heuristic: reduce block size if using lots of shared memory
    /* P1-19: Fix condition ordering - check larger value first so 32768 branch is reachable */
    uint32_t block_size = ctx->default_block_size;
    if (shared_mem_per_block > 32768) {
        block_size = 64;
    } else if (shared_mem_per_block > 16384) {
        block_size = 128;
    }

    return block_size;
}

void nimcp_gpu_context_print_info(const nimcp_gpu_context_t* ctx) {
    if (!ctx) {
        printf("GPU Context: NULL\n");
        return;
    }

    printf("GPU Context:\n");
    printf("  Device: %d (%s)\n", ctx->device_id, ctx->device_info.name);
    printf("  Compute Capability: %d.%d\n",
           ctx->device_info.compute_capability_major,
           ctx->device_info.compute_capability_minor);
    printf("  Memory: %lu MB total, %lu MB allocated, %lu MB peak\n",
           (unsigned long)(ctx->total_memory / (1024 * 1024)),
           (unsigned long)(ctx->allocated_memory / (1024 * 1024)),
           (unsigned long)(ctx->peak_memory / (1024 * 1024)));
    printf("  Allocations: %lu alloc, %lu free\n",
           (unsigned long)ctx->allocation_count,
           (unsigned long)ctx->deallocation_count);
    printf("  cuBLAS: %s\n", ctx->cublas_initialized ? "initialized" : "not available");
    printf("  cuFFT: %s\n", ctx->cufft_initialized ? "initialized" : "not yet created");
}

int nimcp_gpu_context_get_info_string(const nimcp_gpu_context_t* ctx,
                                      char* buffer, size_t size) {
    if (!buffer || size == 0) return 0;

    if (!ctx) {
        return snprintf(buffer, size, "GPU Context: NULL");
    }

    return snprintf(buffer, size,
                    "GPU %d: %s (CC %d.%d, %lu MB, cuBLAS=%s)",
                    ctx->device_id, ctx->device_info.name,
                    ctx->device_info.compute_capability_major,
                    ctx->device_info.compute_capability_minor,
                    (unsigned long)(ctx->total_memory / (1024 * 1024)),
                    ctx->cublas_initialized ? "yes" : "no");
}

#else /* !NIMCP_ENABLE_CUDA */

//=============================================================================
// CPU Fallback Stubs (when CUDA is not available)
//=============================================================================

nimcp_gpu_context_t* nimcp_gpu_context_create(int device_id) {
    (void)device_id;
    return NULL;  // No GPU available
}

nimcp_gpu_context_t* nimcp_gpu_context_create_auto(void) {
    return NULL;  // No GPU available
}

void nimcp_gpu_context_destroy(nimcp_gpu_context_t* ctx) {
    (void)ctx;
}

bool nimcp_gpu_context_is_valid(const nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return false;
}

int nimcp_gpu_context_set_device(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return -1;
}

int nimcp_gpu_context_synchronize(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return -1;
}

const char* nimcp_gpu_context_get_error(const nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return "CUDA not available";
}

void* nimcp_gpu_malloc(nimcp_gpu_context_t* ctx, size_t size_bytes) {
    (void)ctx;
    (void)size_bytes;
    return NULL;
}

void nimcp_gpu_free(nimcp_gpu_context_t* ctx, void* dev_ptr) {
    (void)ctx;
    (void)dev_ptr;
}

int nimcp_gpu_memcpy(nimcp_gpu_context_t* ctx,
                     void* dst, const void* src,
                     size_t size_bytes,
                     nimcp_gpu_memcpy_kind_t kind) {
    (void)ctx;
    (void)dst;
    (void)src;
    (void)size_bytes;
    (void)kind;
    return -1;
}

int nimcp_gpu_memcpy_async(nimcp_gpu_context_t* ctx,
                           void* dst, const void* src,
                           size_t size_bytes,
                           nimcp_gpu_memcpy_kind_t kind,
                           bool use_transfer_stream) {
    (void)ctx;
    (void)dst;
    (void)src;
    (void)size_bytes;
    (void)kind;
    (void)use_transfer_stream;
    return -1;
}

int nimcp_gpu_memset(nimcp_gpu_context_t* ctx,
                     void* dev_ptr, int value,
                     size_t size_bytes) {
    (void)ctx;
    (void)dev_ptr;
    (void)value;
    (void)size_bytes;
    return -1;
}

void nimcp_gpu_memory_stats(const nimcp_gpu_context_t* ctx,
                            size_t* allocated_out,
                            size_t* peak_out,
                            size_t* free_out) {
    (void)ctx;
    if (allocated_out) *allocated_out = 0;
    if (peak_out) *peak_out = 0;
    if (free_out) *free_out = 0;
}

nimcp_cuda_stream_t nimcp_gpu_get_compute_stream(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return NULL;
}

nimcp_cuda_stream_t nimcp_gpu_get_transfer_stream(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return NULL;
}

int nimcp_gpu_stream_synchronize(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return -1;
}

nimcp_cuda_stream_t nimcp_gpu_get_pool_stream(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return NULL;
}

void nimcp_gpu_sync_pool(nimcp_gpu_context_t* ctx) {
    (void)ctx;
}

nimcp_cublas_handle_t nimcp_gpu_get_cublas(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return NULL;
}

nimcp_cufft_handle_t nimcp_gpu_get_cufft_1d(nimcp_gpu_context_t* ctx, int n) {
    (void)ctx;
    (void)n;
    return 0;
}

void nimcp_gpu_calc_launch_config(const nimcp_gpu_context_t* ctx,
                                  uint32_t num_elements,
                                  uint32_t* block_size_out,
                                  uint32_t* grid_size_out) {
    (void)ctx;
    (void)num_elements;
    if (block_size_out) *block_size_out = 0;
    if (grid_size_out) *grid_size_out = 0;
}

uint32_t nimcp_gpu_get_optimal_block_size(const nimcp_gpu_context_t* ctx,
                                          size_t shared_mem_per_block) {
    (void)ctx;
    (void)shared_mem_per_block;
    return 0;
}

void nimcp_gpu_context_print_info(const nimcp_gpu_context_t* ctx) {
    (void)ctx;
    printf("GPU Context: CUDA not available\n");
}

int nimcp_gpu_context_get_info_string(const nimcp_gpu_context_t* ctx,
                                      char* buffer, size_t size) {
    (void)ctx;
    if (!buffer || size == 0) return 0;
    return snprintf(buffer, size, "GPU Context: CUDA not available");
}

#endif /* NIMCP_ENABLE_CUDA */
