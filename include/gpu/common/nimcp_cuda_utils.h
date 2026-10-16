/**
 * @file nimcp_cuda_utils.h
 * @brief Standardized CUDA Error Handling and Utilities
 *
 * WHAT: Common CUDA error checking macros and utilities
 * WHY:  Eliminate inconsistent error handling across 60+ GPU kernel files
 * HOW:  Standardized macros with consistent error reporting
 *
 * USAGE:
 *   #include "gpu/common/nimcp_cuda_utils.h"
 *
 *   NIMCP_CUDA_CHECK(cudaMalloc(&ptr, size));
 *   NIMCP_CUDA_CHECK_RETURN(cudaMemcpy(...));
 *
 * CONSOLIDATES DUPLICATES FROM:
 *   - ALL .cu files in src/gpu/
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_CUDA_UTILS_H
#define NIMCP_CUDA_UTILS_H

#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Standard Kernel Configuration
//=============================================================================

/** Default CUDA block size (threads per block) */
#define NIMCP_CUDA_BLOCK_SIZE 256

/** Maximum supported block size */
#define NIMCP_CUDA_MAX_BLOCK_SIZE 1024

/** Warp size (fixed on all NVIDIA GPUs) */
#define NIMCP_CUDA_WARP_SIZE 32

/** Calculate grid size for n elements */
#define NIMCP_CUDA_GRID_SIZE(n, block) (((n) + (block) - 1) / (block))

/** Calculate grid size with default block */
#define NIMCP_CUDA_GRID_SIZE_DEFAULT(n) NIMCP_CUDA_GRID_SIZE(n, NIMCP_CUDA_BLOCK_SIZE)

//=============================================================================
// Error Checking Macros
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Check CUDA call, log error, and return false on failure
 *
 * Use in functions returning bool.
 */
#define NIMCP_CUDA_CHECK(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, cudaGetErrorString(_err)); \
        return false; \
    } \
} while(0)

/**
 * @brief Check CUDA call, log error, and return NULL on failure
 *
 * Use in functions returning pointers.
 */
#define NIMCP_CUDA_CHECK_NULL(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, cudaGetErrorString(_err)); \
        return NULL; \
    } \
} while(0)

/**
 * @brief Check CUDA call, log error, and return error code on failure
 *
 * Use in functions returning nimcp_error_t.
 */
#define NIMCP_CUDA_CHECK_ERROR(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, cudaGetErrorString(_err)); \
        return NIMCP_ERROR_GPU; \
    } \
} while(0)

/**
 * @brief Check CUDA call, log error, and goto cleanup label on failure
 *
 * Use when cleanup is needed before return.
 */
#define NIMCP_CUDA_CHECK_GOTO(call, label) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, cudaGetErrorString(_err)); \
        goto label; \
    } \
} while(0)

/**
 * @brief Check CUDA call and store result, but don't return
 *
 * For non-critical errors or when custom handling is needed.
 */
#define NIMCP_CUDA_CHECK_WARN(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA WARN] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, cudaGetErrorString(_err)); \
    } \
} while(0)

/**
 * @brief Check last CUDA error (for kernel launches)
 */
#define NIMCP_CUDA_CHECK_LAST() do { \
    cudaError_t _err = cudaGetLastError(); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: kernel error: %s\n", \
                __FILE__, __LINE__, cudaGetErrorString(_err)); \
        return false; \
    } \
} while(0)

/**
 * @brief Check last CUDA error, return NULL
 */
#define NIMCP_CUDA_CHECK_LAST_NULL() do { \
    cudaError_t _err = cudaGetLastError(); \
    if (_err != cudaSuccess) { \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: kernel error: %s\n", \
                __FILE__, __LINE__, cudaGetErrorString(_err)); \
        return NULL; \
    } \
} while(0)

/**
 * @brief Synchronize and check for errors
 */
#define NIMCP_CUDA_SYNC_CHECK() do { \
    NIMCP_CUDA_CHECK(cudaDeviceSynchronize()); \
    NIMCP_CUDA_CHECK_LAST(); \
} while(0)

#else // !NIMCP_ENABLE_CUDA

// No-op macros when CUDA is disabled
#define NIMCP_CUDA_CHECK(call) ((void)0)
#define NIMCP_CUDA_CHECK_NULL(call) ((void)0)
#define NIMCP_CUDA_CHECK_ERROR(call) (NIMCP_ERROR_GPU)
#define NIMCP_CUDA_CHECK_GOTO(call, label) ((void)0)
#define NIMCP_CUDA_CHECK_WARN(call) ((void)0)
#define NIMCP_CUDA_CHECK_LAST() ((void)0)
#define NIMCP_CUDA_CHECK_LAST_NULL() ((void)0)
#define NIMCP_CUDA_SYNC_CHECK() ((void)0)

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Immune-Integrated CUDA Error Checking
//=============================================================================
//
// WHAT: CUDA error macros that present errors to the brain immune system
// WHY:  Enable automatic recovery from GPU errors via immune response
// HOW:  Call NIMCP_THROW_TO_IMMUNE on CUDA failures before returning
//
// These macros require including utils/exception/nimcp_exception_macros.h
// Use these in code paths where immune system recovery is desired.
//
// Example:
//   #include "utils/exception/nimcp_exception_macros.h"
//   NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&ptr, size));  // Presents to immune on failure
//

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Check CUDA call, present to immune system, and return false on failure
 *
 * Use in functions returning bool where immune recovery is desired.
 */
#define NIMCP_CUDA_CHECK_IMMUNE(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        const char* _err_str = cudaGetErrorString(_err); \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, _err_str); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA error: %s - %s", #call, _err_str); \
        return false; \
    } \
} while(0)

/**
 * @brief Check CUDA call, present to immune system, and return NULL on failure
 *
 * Use in functions returning pointers where immune recovery is desired.
 */
#define NIMCP_CUDA_CHECK_IMMUNE_NULL(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        const char* _err_str = cudaGetErrorString(_err); \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, _err_str); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA error: %s - %s", #call, _err_str); \
        return NULL; \
    } \
} while(0)

/**
 * @brief Check CUDA call, present to immune system, and return error code on failure
 *
 * Use in functions returning nimcp_error_t where immune recovery is desired.
 */
#define NIMCP_CUDA_CHECK_IMMUNE_ERROR(call) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        const char* _err_str = cudaGetErrorString(_err); \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, _err_str); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA error: %s - %s", #call, _err_str); \
        return NIMCP_ERROR_GPU; \
    } \
} while(0)

/**
 * @brief Check last CUDA error (kernel launches), present to immune system
 *
 * Call after kernel launches where immune recovery is desired.
 */
#define NIMCP_CUDA_CHECK_IMMUNE_LAST() do { \
    cudaError_t _err = cudaGetLastError(); \
    if (_err != cudaSuccess) { \
        const char* _err_str = cudaGetErrorString(_err); \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: kernel error: %s\n", \
                __FILE__, __LINE__, _err_str); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA kernel error: %s", _err_str); \
        return false; \
    } \
} while(0)

/**
 * @brief CUDA error check with immune presentation and goto cleanup
 *
 * Use when cleanup is needed before return and immune recovery is desired.
 */
#define NIMCP_CUDA_CHECK_IMMUNE_GOTO(call, label) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        const char* _err_str = cudaGetErrorString(_err); \
        fprintf(stderr, "[NIMCP CUDA ERROR] %s:%d: %s returned %s\n", \
                __FILE__, __LINE__, #call, _err_str); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA error: %s - %s", #call, _err_str); \
        goto label; \
    } \
} while(0)

/**
 * @brief Synchronize and check with immune presentation
 */
#define NIMCP_CUDA_SYNC_CHECK_IMMUNE() do { \
    NIMCP_CUDA_CHECK_IMMUNE(cudaDeviceSynchronize()); \
    NIMCP_CUDA_CHECK_IMMUNE_LAST(); \
} while(0)

/**
 * @brief Alias for NIMCP_CUDA_CHECK_IMMUNE (returns bool)
 *
 * Use in functions explicitly returning bool.
 */
#define NIMCP_CUDA_CHECK_IMMUNE_BOOL(call) NIMCP_CUDA_CHECK_IMMUNE(call)

#else // !NIMCP_ENABLE_CUDA

// No-op macros when CUDA is disabled
#define NIMCP_CUDA_CHECK_IMMUNE(call) ((void)0)
#define NIMCP_CUDA_CHECK_IMMUNE_NULL(call) ((void)0)
#define NIMCP_CUDA_CHECK_IMMUNE_ERROR(call) (NIMCP_ERROR_GPU)
#define NIMCP_CUDA_CHECK_IMMUNE_LAST() ((void)0)
#define NIMCP_CUDA_CHECK_IMMUNE_GOTO(call, label) ((void)0)
#define NIMCP_CUDA_CHECK_IMMUNE_BOOL(call) ((void)0)
#define NIMCP_CUDA_SYNC_CHECK_IMMUNE() ((void)0)

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// cuBLAS/cuSPARSE/cuRAND Error Checking
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Check cuBLAS call
 */
#define NIMCP_CUBLAS_CHECK(call) do { \
    cublasStatus_t _status = (call); \
    if (_status != CUBLAS_STATUS_SUCCESS) { \
        fprintf(stderr, "[NIMCP cuBLAS ERROR] %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #call, _status); \
        return false; \
    } \
} while(0)

/**
 * @brief Check cuSPARSE call
 */
#define NIMCP_CUSPARSE_CHECK(call) do { \
    cusparseStatus_t _status = (call); \
    if (_status != CUSPARSE_STATUS_SUCCESS) { \
        fprintf(stderr, "[NIMCP cuSPARSE ERROR] %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #call, _status); \
        return false; \
    } \
} while(0)

/**
 * @brief Check cuRAND call
 */
#define NIMCP_CURAND_CHECK(call) do { \
    curandStatus_t _status = (call); \
    if (_status != CURAND_STATUS_SUCCESS) { \
        fprintf(stderr, "[NIMCP cuRAND ERROR] %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #call, _status); \
        return false; \
    } \
} while(0)

#else

#define NIMCP_CUBLAS_CHECK(call) ((void)0)
#define NIMCP_CUSPARSE_CHECK(call) ((void)0)
#define NIMCP_CURAND_CHECK(call) ((void)0)

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Immune-Integrated cuBLAS/cuSPARSE/cuRAND Error Checking
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Check cuBLAS call with immune system presentation
 */
#define NIMCP_CUBLAS_CHECK_IMMUNE(call) do { \
    cublasStatus_t _status = (call); \
    if (_status != CUBLAS_STATUS_SUCCESS) { \
        fprintf(stderr, "[NIMCP cuBLAS ERROR] %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #call, _status); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "cuBLAS error: %s returned %d", #call, _status); \
        return false; \
    } \
} while(0)

/**
 * @brief Check cuSPARSE call with immune system presentation
 */
#define NIMCP_CUSPARSE_CHECK_IMMUNE(call) do { \
    cusparseStatus_t _status = (call); \
    if (_status != CUSPARSE_STATUS_SUCCESS) { \
        fprintf(stderr, "[NIMCP cuSPARSE ERROR] %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #call, _status); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "cuSPARSE error: %s returned %d", #call, _status); \
        return false; \
    } \
} while(0)

/**
 * @brief Check cuRAND call with immune system presentation
 */
#define NIMCP_CURAND_CHECK_IMMUNE(call) do { \
    curandStatus_t _status = (call); \
    if (_status != CURAND_STATUS_SUCCESS) { \
        fprintf(stderr, "[NIMCP cuRAND ERROR] %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #call, _status); \
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "cuRAND error: %s returned %d", #call, _status); \
        return false; \
    } \
} while(0)

#else

#define NIMCP_CUBLAS_CHECK_IMMUNE(call) ((void)0)
#define NIMCP_CUSPARSE_CHECK_IMMUNE(call) ((void)0)
#define NIMCP_CURAND_CHECK_IMMUNE(call) ((void)0)

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Kernel Launch Helpers
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Calculate optimal block size for occupancy
 *
 * @param kernel Kernel function pointer
 * @param dynamic_smem Dynamic shared memory per block
 * @return Recommended block size
 */
static inline int nimcp_cuda_optimal_block_size(void* kernel, size_t dynamic_smem)
{
    int min_grid_size, block_size;
    cudaOccupancyMaxPotentialBlockSize(&min_grid_size, &block_size, kernel, dynamic_smem, 0);
    (void)min_grid_size;
    return block_size;
}

/**
 * @brief Get current device compute capability
 *
 * @param major Output: major version
 * @param minor Output: minor version
 * @return true on success
 */
static inline bool nimcp_cuda_get_compute_capability(int* major, int* minor)
{
    int device;
    if (cudaGetDevice(&device) != cudaSuccess) return false;

    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, device) != cudaSuccess) return false;

    *major = prop.major;
    *minor = prop.minor;
    return true;
}

/**
 * @brief Get available GPU memory
 *
 * @param free_bytes Output: free memory in bytes
 * @param total_bytes Output: total memory in bytes
 * @return true on success
 */
static inline bool nimcp_cuda_get_memory_info(size_t* free_bytes, size_t* total_bytes)
{
    return cudaMemGetInfo(free_bytes, total_bytes) == cudaSuccess;
}

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Debug Helpers
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief Print GPU memory usage to stderr
 */
static inline void nimcp_cuda_print_memory_usage(void)
{
    size_t free_mem, total_mem;
    if (cudaMemGetInfo(&free_mem, &total_mem) == cudaSuccess) {
        fprintf(stderr, "[NIMCP GPU Memory] Free: %.2f GB / Total: %.2f GB (%.1f%% used)\n",
                free_mem / (1024.0 * 1024.0 * 1024.0),
                total_mem / (1024.0 * 1024.0 * 1024.0),
                100.0 * (1.0 - (double)free_mem / total_mem));
    }
}

/**
 * @brief Print device info to stderr
 */
static inline void nimcp_cuda_print_device_info(void)
{
    int device;
    if (cudaGetDevice(&device) != cudaSuccess) return;

    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, device) != cudaSuccess) return;

    fprintf(stderr, "[NIMCP GPU] Device %d: %s (SM %d.%d, %.2f GB, %d SMs)\n",
            device, prop.name, prop.major, prop.minor,
            prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0),
            prop.multiProcessorCount);
}

#else

static inline void nimcp_cuda_print_memory_usage(void) {}
static inline void nimcp_cuda_print_device_info(void) {}

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Self-Healing Recovery Macros
//=============================================================================
//
// WHAT: CUDA error macros that attempt recovery before failing
// WHY:  Enable self-healing GPU operations that don't halt the system
// HOW:  On error: categorize -> select strategy -> execute recovery -> retry
//
// These macros require including:
//   - gpu/recovery/nimcp_gpu_recovery.h
//   - utils/exception/nimcp_exception_macros.h
//
// Example:
//   #include "gpu/recovery/nimcp_gpu_recovery.h"
//   NIMCP_CUDA_RECOVER(cudaMalloc(&ptr, size), GPU_ERROR_OUT_OF_MEMORY);
//

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief CUDA call with full recovery support, returns false on unrecoverable failure
 *
 * Attempts recovery on failure before returning. If recovery succeeds,
 * retries the operation. Only fails if recovery is exhausted.
 *
 * @param call CUDA call to execute
 * @param error_cat Error category for recovery strategy selection
 */
#define NIMCP_CUDA_RECOVER(call, error_cat) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            /* Recovery succeeded - retry */ \
            _err = (call); \
        } \
        if (_err != cudaSuccess) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: %s - %s\n", \
                    __FILE__, __LINE__, #call, _err_str); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU error (unrecoverable): %s - %s", #call, _err_str); \
            return false; \
        } \
    } \
} while(0)

/**
 * @brief CUDA call with recovery, returns NULL on unrecoverable failure
 */
#define NIMCP_CUDA_RECOVER_NULL(call, error_cat) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            _err = (call); \
        } \
        if (_err != cudaSuccess) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: %s - %s\n", \
                    __FILE__, __LINE__, #call, _err_str); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU error (unrecoverable): %s - %s", #call, _err_str); \
            return NULL; \
        } \
    } \
} while(0)

/**
 * @brief CUDA call with recovery, returns error code on unrecoverable failure
 */
#define NIMCP_CUDA_RECOVER_ERROR(call, error_cat) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            _err = (call); \
        } \
        if (_err != cudaSuccess) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: %s - %s\n", \
                    __FILE__, __LINE__, #call, _err_str); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU error (unrecoverable): %s - %s", #call, _err_str); \
            return NIMCP_ERROR_GPU; \
        } \
    } \
} while(0)

/**
 * @brief Check last CUDA error with recovery
 */
#define NIMCP_CUDA_RECOVER_LAST(error_cat) do { \
    cudaError_t _err = cudaGetLastError(); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (!nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: kernel error - %s\n", \
                    __FILE__, __LINE__, _err_str); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU kernel error (unrecoverable): %s", _err_str); \
            return false; \
        } \
    } \
} while(0)

/**
 * @brief Check last CUDA error with recovery, returning NULL on failure
 * Use in functions that return pointers instead of bool
 */
#define NIMCP_CUDA_RECOVER_LAST_NULL(error_cat) do { \
    cudaError_t _err = cudaGetLastError(); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (!nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: kernel error - %s\n", \
                    __FILE__, __LINE__, _err_str); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU kernel error (unrecoverable): %s", _err_str); \
            return NULL; \
        } \
    } \
} while(0)

/**
 * @brief CUDA call with recovery and goto cleanup on unrecoverable failure
 */
#define NIMCP_CUDA_RECOVER_GOTO(call, error_cat, label) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            _err = (call); \
        } \
        if (_err != cudaSuccess) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: %s - %s\n", \
                    __FILE__, __LINE__, #call, _err_str); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, _err, \
                "GPU error (unrecoverable): %s - %s", #call, _err_str); \
            goto label; \
        } \
    } \
} while(0)

/**
 * @brief cuBLAS call with recovery
 */
#define NIMCP_CUBLAS_RECOVER(call, error_cat) do { \
    cublasStatus_t _status = (call); \
    if (_status != CUBLAS_STATUS_SUCCESS) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), cudaErrorUnknown, &_result)) { \
            _status = (call); \
        } \
        if (_status != CUBLAS_STATUS_SUCCESS) { \
            fprintf(stderr, "[NIMCP cuBLAS UNRECOVERABLE] %s:%d: %s returned %d\n", \
                    __FILE__, __LINE__, #call, _status); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 0, \
                "cuBLAS error (unrecoverable): %s returned %d", #call, _status); \
            return false; \
        } \
    } \
} while(0)

/**
 * @brief cuRAND call with recovery
 */
#define NIMCP_CURAND_RECOVER(call, error_cat) do { \
    curandStatus_t _status = (call); \
    if (_status != CURAND_STATUS_SUCCESS) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), cudaErrorUnknown, &_result)) { \
            _status = (call); \
        } \
        if (_status != CURAND_STATUS_SUCCESS) { \
            fprintf(stderr, "[NIMCP cuRAND UNRECOVERABLE] %s:%d: %s returned %d\n", \
                    __FILE__, __LINE__, #call, _status); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 0, \
                "cuRAND error (unrecoverable): %s returned %d", #call, _status); \
            return false; \
        } \
    } \
} while(0)

/**
 * @brief cuSOLVER call with recovery
 */
#define NIMCP_CUSOLVER_RECOVER(call, error_cat) do { \
    cusolverStatus_t _status = (call); \
    if (_status != CUSOLVER_STATUS_SUCCESS) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), cudaErrorUnknown, &_result)) { \
            _status = (call); \
        } \
        if (_status != CUSOLVER_STATUS_SUCCESS) { \
            fprintf(stderr, "[NIMCP cuSOLVER UNRECOVERABLE] %s:%d: %s returned %d\n", \
                    __FILE__, __LINE__, #call, _status); \
            NIMCP_THROW_GPU(NIMCP_ERROR_GPU, 0, 0, \
                "cuSOLVER error (unrecoverable): %s returned %d", #call, _status); \
            return false; \
        } \
    } \
} while(0)

/**
 * @brief Validate parameters and auto-correct if enabled
 *
 * Use at the start of GPU functions to validate and auto-correct parameters.
 * If correction fails, throws to immune system.
 *
 * @param cond Condition that must be true for valid params
 * @param param_name Name of parameter being checked
 */
#define NIMCP_GPU_VALIDATE_PARAM(cond, param_name) do { \
    if (!(cond)) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &_result)) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, \
                "Invalid parameter: %s", param_name); \
            return NULL; \
        } \
    } \
} while(0)

/**
 * @brief Ensure GPU memory available, attempt recovery if not
 *
 * @param bytes Required bytes
 */
#define NIMCP_GPU_ENSURE_MEMORY(bytes) do { \
    if (!nimcp_gpu_ensure_memory_available(bytes)) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, &_result)) { \
            NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, cudaErrorMemoryAllocation, \
                "Insufficient GPU memory: need %zu bytes", (size_t)(bytes)); \
            return NULL; \
        } \
    } \
} while(0)

#else // !NIMCP_ENABLE_CUDA

/* No-op stubs when CUDA disabled */
#define NIMCP_CUDA_RECOVER(call, error_cat) ((void)0)
#define NIMCP_CUDA_RECOVER_NULL(call, error_cat) ((void)0)
#define NIMCP_CUDA_RECOVER_ERROR(call, error_cat) (NIMCP_ERROR_GPU)
#define NIMCP_CUDA_RECOVER_LAST(error_cat) ((void)0)
#define NIMCP_CUDA_RECOVER_LAST_NULL(error_cat) ((void)0)
#define NIMCP_CUDA_RECOVER_GOTO(call, error_cat, label) ((void)0)
#define NIMCP_CUBLAS_RECOVER(call, error_cat) ((void)0)
#define NIMCP_CURAND_RECOVER(call, error_cat) ((void)0)
#define NIMCP_CUSOLVER_RECOVER(call, error_cat) ((void)0)
#define NIMCP_GPU_VALIDATE_PARAM(cond, param_name) ((void)0)
#define NIMCP_GPU_ENSURE_MEMORY(bytes) ((void)0)

#endif // NIMCP_ENABLE_CUDA

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CUDA_UTILS_H
