/**
 * @file nimcp_gpu_detect.h
 * @brief Runtime GPU Capability Detection (CUDA, OpenCL, ROCm)
 *
 * WHAT: Detects GPU hardware and driver capabilities at runtime
 * WHY:  Enables optimal GPU backend selection without compile-time dependencies
 * HOW:  Runtime library loading and API probing for each GPU backend
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------+
 *   |                  GPU CAPABILITY DETECTION                   |
 *   |                                                            |
 *   |  +---------------+    +---------------+    +---------------+|
 *   |  |    CUDA       |    |   OpenCL      |    |    ROCm       ||
 *   |  |  (NVIDIA)     |    | (Cross-Plat)  |    |    (AMD)      ||
 *   |  +---------------+    +---------------+    +---------------+|
 *   |         |                   |                    |         |
 *   |         +-------------------+--------------------+         |
 *   |                            |                               |
 *   |               +------------------------+                   |
 *   |               |  gpu_capabilities_t    |                   |
 *   |               |  (Unified Results)     |                   |
 *   |               +------------------------+                   |
 *   +------------------------------------------------------------+
 *
 * DETECTION APPROACH:
 * - CUDA: Runtime library loading (libcuda.so/nvcuda.dll) + Driver API
 * - OpenCL: Runtime library loading (libOpenCL.so/OpenCL.dll) + clGetPlatformIDs
 * - ROCm: Runtime library loading (libamdhip64.so) + HIP API
 *
 * FALLBACK BEHAVIOR:
 * - If no GPU runtime available: All caps set to unavailable
 * - If driver not installed: Respective backend marked unavailable
 * - Graceful degradation to CPU-only execution
 *
 * THREAD SAFETY: Thread-safe (detection is read-only after init)
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_GPU_DETECT_H
#define NIMCP_GPU_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// GPU Backend Types
//=============================================================================

/**
 * @brief GPU backend types (bitmask for detected backends)
 */
typedef enum {
    GPU_BACKEND_NONE    = 0,
    GPU_BACKEND_CUDA    = (1 << 0),  /**< NVIDIA CUDA */
    GPU_BACKEND_OPENCL  = (1 << 1),  /**< OpenCL (cross-platform) */
    GPU_BACKEND_ROCM    = (1 << 2),  /**< AMD ROCm/HIP */
    GPU_BACKEND_METAL   = (1 << 3),  /**< Apple Metal (reserved) */
    GPU_BACKEND_VULKAN  = (1 << 4),  /**< Vulkan compute (reserved) */
    GPU_BACKEND_NEURON  = (1 << 5),  /**< AWS Inferentia NeuronCore */
} gpu_backend_t;

/**
 * @brief GPU vendor types
 */
typedef enum {
    GPU_VENDOR_UNKNOWN  = 0,
    GPU_VENDOR_NVIDIA   = 1,
    GPU_VENDOR_AMD      = 2,
    GPU_VENDOR_INTEL    = 3,
    GPU_VENDOR_APPLE    = 4,
    GPU_VENDOR_OTHER    = 5,
    GPU_VENDOR_AWS      = 6,
} gpu_vendor_t;

//=============================================================================
// GPU Device Information
//=============================================================================

/**
 * @brief Information about a single GPU device
 *
 * WHAT: Comprehensive GPU device properties
 * WHY:  Needed for work distribution and memory management
 * HOW:  Populated via backend-specific queries
 */
typedef struct {
    // Device identification
    int device_index;                /**< Index within backend (0-based) */
    char name[256];                  /**< Device name (e.g., "NVIDIA GeForce RTX 4090") */
    gpu_vendor_t vendor;             /**< GPU vendor */
    gpu_backend_t backend;           /**< Backend used to access this device */

    // Compute capability
    uint32_t compute_capability_major; /**< Major version (CUDA) or feature level */
    uint32_t compute_capability_minor; /**< Minor version */
    uint32_t compute_units;          /**< SMs (NVIDIA), CUs (AMD), EUs (Intel) */
    uint32_t max_threads_per_block;  /**< Max threads per block/workgroup */
    uint32_t max_block_dim_x;        /**< Max block dimension X */
    uint32_t max_block_dim_y;        /**< Max block dimension Y */
    uint32_t max_block_dim_z;        /**< Max block dimension Z */
    uint32_t warp_size;              /**< Warp size (32 for NVIDIA, 64 for AMD) */

    // Memory
    uint64_t total_memory_bytes;     /**< Total device memory */
    uint64_t free_memory_bytes;      /**< Available device memory */
    uint32_t memory_bus_width;       /**< Memory bus width in bits */
    uint32_t memory_clock_mhz;       /**< Memory clock speed in MHz */
    float memory_bandwidth_gbps;     /**< Estimated memory bandwidth in GB/s */

    // Clock speeds
    uint32_t core_clock_mhz;         /**< Core clock speed in MHz */
    uint32_t boost_clock_mhz;        /**< Boost clock speed in MHz */

    // Features
    bool supports_unified_memory;    /**< Unified memory / managed memory */
    bool supports_concurrent_kernels; /**< Concurrent kernel execution */
    bool supports_fp16;              /**< Half-precision floating point */
    bool supports_fp64;              /**< Double-precision floating point */
    bool supports_tensor_cores;      /**< Tensor cores (NVIDIA) / matrix engines */
    bool supports_p2p;               /**< Peer-to-peer memory access */

    // Current state
    float compute_utilization;       /**< Current compute utilization [0, 1] */
    float memory_utilization;        /**< Current memory utilization [0, 1] */
    float temperature_celsius;       /**< Current temperature */
    float power_watts;               /**< Current power consumption */
} gpu_device_info_t;

//=============================================================================
// GPU Capabilities Structure
//=============================================================================

/**
 * @brief Detailed GPU detection result structure
 *
 * WHAT: Contains detected GPU features across all backends
 * WHY:  Unified representation for backend selection and mode decision
 * HOW:  Populated by gpu_detect_capabilities()
 *
 * NOTE: This is distinct from gpu_capabilities_t in nimcp_system_resources.h
 *       which is a simpler summary structure.
 */
typedef struct {
    // Backend availability
    uint32_t available_backends;     /**< Bitmask of gpu_backend_t */
    bool cuda_available;             /**< CUDA runtime available */
    bool opencl_available;           /**< OpenCL runtime available */
    bool rocm_available;             /**< ROCm/HIP runtime available */

    // CUDA-specific
    int cuda_driver_version;         /**< CUDA driver version (e.g., 12040 for 12.4) */
    int cuda_runtime_version;        /**< CUDA runtime version */
    uint32_t cuda_device_count;      /**< Number of CUDA devices */

    // OpenCL-specific
    uint32_t opencl_platform_count;  /**< Number of OpenCL platforms */
    uint32_t opencl_device_count;    /**< Total OpenCL devices across platforms */
    char opencl_platform_names[4][128]; /**< Names of first 4 OpenCL platforms */

    // ROCm-specific
    int rocm_version;                /**< ROCm version (e.g., 50601 for 5.6.1) */
    uint32_t rocm_device_count;      /**< Number of ROCm/HIP devices */

    // Device list (all backends combined)
    gpu_device_info_t devices[16];   /**< Device info array */
    uint32_t device_count;           /**< Total number of devices */

    // Best available device
    int best_device_index;           /**< Index of most capable device */
    gpu_backend_t best_backend;      /**< Backend of best device */

    // Aggregate capabilities
    uint64_t total_gpu_memory_bytes; /**< Sum of all GPU memory */
    uint32_t total_compute_units;    /**< Sum of all compute units */
    float estimated_tflops;          /**< Estimated total TFLOPS */

    // Recommended settings
    gpu_backend_t recommended_backend; /**< Recommended backend to use */

    // AWS Neuron/Inferentia
    bool neuron_available;           /**< NRT runtime available */
    uint32_t neuron_device_count;    /**< Number of Neuron devices */
    uint32_t neuron_cores_per_device; /**< NeuronCores per device */
} gpu_detect_result_t;

//=============================================================================
// Detection API
//=============================================================================

/**
 * @brief Detect GPU capabilities
 *
 * WHAT: Probes system for GPU hardware and driver availability
 * WHY:  Need to know GPU capabilities for execution mode selection
 * HOW:  Runtime library loading and API queries for each backend
 *
 * @param caps Output structure for capabilities (must not be NULL)
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe (read-only after detection)
 * CACHING: Results are cached after first call
 *
 * DETECTION ORDER:
 * 1. CUDA (NVIDIA) - via libcuda.so / nvcuda.dll
 * 2. ROCm (AMD) - via libamdhip64.so
 * 3. OpenCL (cross-platform) - via libOpenCL.so / OpenCL.dll
 *
 * EXAMPLE:
 *   gpu_detect_result_t caps;
 *   if (gpu_detect_capabilities(&caps)) {
 *       if (caps.cuda_available) {
 *           printf("CUDA available with %u devices\n", caps.cuda_device_count);
 *       }
 *       if (caps.recommended_backend != GPU_BACKEND_NONE) {
 *           use_gpu_mode(caps.recommended_backend);
 *       }
 *   }
 */
NIMCP_EXPORT bool gpu_detect_capabilities(gpu_detect_result_t* caps);

/**
 * @brief Check if any GPU is available
 *
 * @return true if at least one GPU backend is available
 */
NIMCP_EXPORT bool gpu_is_available(void);

/**
 * @brief Check if specific GPU backend is available
 *
 * @param backend Backend to check
 * @return true if backend is available
 */
NIMCP_EXPORT bool gpu_backend_is_available(gpu_backend_t backend);

/**
 * @brief Get number of available GPU devices
 *
 * @return Total number of GPU devices across all backends
 */
NIMCP_EXPORT uint32_t gpu_get_device_count(void);

/**
 * @brief Get GPU device information by index
 *
 * @param device_index Device index (0 to device_count-1)
 * @param info Output device info structure
 * @return true on success, false if index invalid
 */
NIMCP_EXPORT bool gpu_get_device_info(uint32_t device_index, gpu_device_info_t* info);

/**
 * @brief Get best available GPU device
 *
 * WHAT: Returns index of most capable GPU device
 * WHY:  Simplifies device selection for single-GPU workloads
 * HOW:  Ranks devices by compute capability, memory, and features
 *
 * @return Device index, or -1 if no GPU available
 */
NIMCP_EXPORT int gpu_get_best_device(void);

/**
 * @brief Get recommended GPU backend
 *
 * WHAT: Returns the backend recommended for current system
 * WHY:  Simplifies backend selection
 * HOW:  Considers driver availability, device capability, and stability
 *
 * @return Recommended backend, or GPU_BACKEND_NONE if no GPU available
 */
NIMCP_EXPORT gpu_backend_t gpu_get_recommended_backend(void);

/**
 * @brief Get GPU backend name
 *
 * @param backend Backend type
 * @return Human-readable name (e.g., "CUDA", "OpenCL", "ROCm")
 */
NIMCP_EXPORT const char* gpu_backend_name(gpu_backend_t backend);

/**
 * @brief Get GPU vendor name
 *
 * @param vendor Vendor type
 * @return Human-readable name (e.g., "NVIDIA", "AMD", "Intel")
 */
NIMCP_EXPORT const char* gpu_vendor_name(gpu_vendor_t vendor);

/**
 * @brief Get GPU capabilities summary string
 *
 * @param buffer Output buffer for summary string
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 *
 * EXAMPLE OUTPUT:
 *   "CUDA: 2 devices (RTX 4090, RTX 3080), 48GB total, 164 TFLOPS"
 */
NIMCP_EXPORT size_t gpu_capabilities_string(char* buffer, size_t size);

/**
 * @brief Refresh GPU capabilities (re-detect)
 *
 * WHAT: Forces re-detection of GPU capabilities
 * WHY:  Useful after hot-plug events or driver updates
 * HOW:  Clears cache and re-runs detection
 *
 * @return true on success
 *
 * NOTE: Thread safety not guaranteed during refresh
 */
NIMCP_EXPORT bool gpu_refresh_capabilities(void);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Check if CUDA is available (common check)
 */
#define GPU_HAS_CUDA() gpu_backend_is_available(GPU_BACKEND_CUDA)

/**
 * @brief Check if OpenCL is available (common check)
 */
#define GPU_HAS_OPENCL() gpu_backend_is_available(GPU_BACKEND_OPENCL)

/**
 * @brief Check if ROCm is available (common check)
 */
#define GPU_HAS_ROCM() gpu_backend_is_available(GPU_BACKEND_ROCM)

/**
 * @brief Check if Neuron (AWS Inferentia) is available (common check)
 */
#define GPU_HAS_NEURON() gpu_backend_is_available(GPU_BACKEND_NEURON)

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GPU_DETECT_H
