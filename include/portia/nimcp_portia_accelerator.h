/**
 * @file nimcp_portia_accelerator.h
 * @brief Hardware Accelerator Detection and Configuration for Portia Spider
 *
 * WHAT: Detects and manages hardware accelerators (GPU, NPU, DSP, FPGA, TPU)
 * WHY:  Enable optimal hardware selection for neural processing workloads
 * HOW:  Platform-specific detection via /sys, /dev, and vendor libraries
 *
 * DETECTION METHODS:
 * - GPU: /dev/nvidia*, /dev/dri, dlopen libcuda/libOpenCL/libvulkan
 * - NPU: /dev/npu*, vendor-specific paths (Intel/Qualcomm/Apple)
 * - DSP: /dev/dsp*, TI/Qualcomm specific devices
 * - FPGA: /dev/fpga*, Intel/Xilinx specific
 * - TPU: Google TPU via vendor libraries
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────┐
 * │              Portia Accelerator Registry                │
 * │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
 * │  │   GPU    │ │   NPU    │ │   DSP    │ │  FPGA    │  │
 * │  │ Detector │ │ Detector │ │ Detector │ │ Detector │  │
 * │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘  │
 * │       │            │            │            │         │
 * │       └────────────┴────────────┴────────────┘         │
 * │                    │                                    │
 * │           ┌────────▼────────┐                          │
 * │           │ Capability Info │                          │
 * │           │  - Compute Units│                          │
 * │           │  - Memory       │                          │
 * │           │  - TFlops       │                          │
 * │           │  - Power Budget │                          │
 * │           └─────────────────┘                          │
 * └─────────────────────────────────────────────────────────┘
 * ```
 *
 * SECURITY:
 * - All pointers validated with bbb_validate_pointer()
 * - Device access logged with bbb_audit_log()
 * - Power limits enforced
 * - Safe fallback to CPU when no accelerators available
 *
 * BIO-ASYNC INTEGRATION:
 * - Broadcasts accelerator discovery events
 * - Publishes capability changes
 * - Handles power management requests
 *
 * @author NIMCP Portia Team
 * @date 2025-12-08
 */

#ifndef NIMCP_PORTIA_ACCELERATOR_H
#define NIMCP_PORTIA_ACCELERATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Accelerator Types
//=============================================================================

/**
 * @brief Hardware accelerator types (bitmask)
 */
typedef enum {
    ACCELERATOR_TYPE_NONE = 0,
    ACCELERATOR_TYPE_GPU = 1,      /**< Graphics Processing Unit */
    ACCELERATOR_TYPE_NPU = 2,      /**< Neural Processing Unit */
    ACCELERATOR_TYPE_DSP = 4,      /**< Digital Signal Processor */
    ACCELERATOR_TYPE_FPGA = 8,     /**< Field Programmable Gate Array */
    ACCELERATOR_TYPE_TPU = 16      /**< Tensor Processing Unit */
} accelerator_type_t;

/**
 * @brief GPU vendor types
 */
typedef enum {
    GPU_VENDOR_UNKNOWN = 0,
    GPU_VENDOR_NVIDIA,
    GPU_VENDOR_AMD,
    GPU_VENDOR_INTEL,
    GPU_VENDOR_APPLE,
    GPU_VENDOR_QUALCOMM
} gpu_vendor_t;

/**
 * @brief NPU vendor types
 */
typedef enum {
    NPU_VENDOR_UNKNOWN = 0,
    NPU_VENDOR_INTEL_MOVIDIUS,
    NPU_VENDOR_QUALCOMM_HEXAGON,
    NPU_VENDOR_APPLE_ANE,
    NPU_VENDOR_GOOGLE_EDGE_TPU,
    NPU_VENDOR_NVIDIA_DLA
} npu_vendor_t;

//=============================================================================
// Accelerator Information
//=============================================================================

/**
 * @brief Information about a single accelerator
 */
typedef struct {
    accelerator_type_t type;       /**< Accelerator type */
    char name[64];                 /**< Device name */
    char vendor[32];               /**< Vendor name */
    uint32_t compute_units;        /**< Number of compute units */
    uint64_t memory_bytes;         /**< Total memory in bytes */
    float peak_tflops;             /**< Peak performance in TFlops */
    float power_watts;             /**< Typical power consumption */
    bool available;                /**< Currently available */
    bool initialized;              /**< Successfully initialized */
    uint32_t device_id;            /**< Device ID (vendor specific) */
    uint32_t pci_bus;              /**< PCI bus number (if applicable) */
    uint32_t pci_device;           /**< PCI device number */
    char driver_version[32];       /**< Driver version string */
    char api_version[32];          /**< API version (CUDA, OpenCL, etc.) */
} accelerator_info_t;

/**
 * @brief Registry of all detected accelerators
 */
typedef struct {
    accelerator_info_t* accelerators; /**< Array of accelerators */
    uint32_t count;                   /**< Number of accelerators */
    uint32_t capacity;                /**< Array capacity */
    uint32_t type_mask;               /**< Bitmask of available types */
    accelerator_type_t preferred;     /**< Best for current workload */
} accelerator_registry_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Accelerator detection and selection configuration
 */
typedef struct {
    bool detect_gpu;               /**< Enable GPU detection */
    bool detect_npu;               /**< Enable NPU detection */
    bool detect_dsp;               /**< Enable DSP detection */
    bool detect_fpga;              /**< Enable FPGA detection */
    bool detect_tpu;               /**< Enable TPU detection */
    bool auto_select;              /**< Auto-pick best accelerator */
    float power_budget_watts;      /**< Maximum power budget */
    float min_memory_gb;           /**< Minimum memory requirement */
    float min_tflops;              /**< Minimum performance requirement */
    bool prefer_low_power;         /**< Prefer power efficiency */
    bool require_fp16;             /**< Require FP16 support */
    bool require_int8;             /**< Require INT8 support */
} portia_accelerator_config_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct portia_accelerator_system_struct* portia_accelerator_system_t;

//=============================================================================
// System Management API
//=============================================================================

/**
 * @brief Get default accelerator configuration
 * @return Default configuration
 */
NIMCP_EXPORT portia_accelerator_config_t portia_accelerator_default_config(void);

/**
 * @brief Initialize accelerator detection system
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 */
NIMCP_EXPORT portia_accelerator_system_t portia_accelerator_init(
    const portia_accelerator_config_t* config);

/**
 * @brief Shutdown accelerator system and free resources
 * @param system System handle
 */
NIMCP_EXPORT void portia_accelerator_shutdown(portia_accelerator_system_t system);

/**
 * @brief Detect all available accelerators
 * @param system System handle
 * @return Number of accelerators detected
 */
NIMCP_EXPORT uint32_t portia_accelerator_detect_all(portia_accelerator_system_t system);

//=============================================================================
// Detection API
//=============================================================================

/**
 * @brief Detect GPUs (CUDA, OpenCL, Vulkan)
 * @param system System handle
 * @return Number of GPUs detected
 */
NIMCP_EXPORT uint32_t portia_accelerator_detect_gpu(portia_accelerator_system_t system);

/**
 * @brief Detect NPUs (Neural Processing Units)
 * @param system System handle
 * @return Number of NPUs detected
 */
NIMCP_EXPORT uint32_t portia_accelerator_detect_npu(portia_accelerator_system_t system);

/**
 * @brief Detect DSPs (Digital Signal Processors)
 * @param system System handle
 * @return Number of DSPs detected
 */
NIMCP_EXPORT uint32_t portia_accelerator_detect_dsp(portia_accelerator_system_t system);

/**
 * @brief Detect FPGAs (Field Programmable Gate Arrays)
 * @param system System handle
 * @return Number of FPGAs detected
 */
NIMCP_EXPORT uint32_t portia_accelerator_detect_fpga(portia_accelerator_system_t system);

/**
 * @brief Detect TPUs (Tensor Processing Units)
 * @param system System handle
 * @return Number of TPUs detected
 */
NIMCP_EXPORT uint32_t portia_accelerator_detect_tpu(portia_accelerator_system_t system);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get information about a specific accelerator
 * @param system System handle
 * @param index Accelerator index
 * @param info Output accelerator info
 * @return true on success
 */
NIMCP_EXPORT bool portia_accelerator_get_info(portia_accelerator_system_t system,
                                               uint32_t index,
                                               accelerator_info_t* info);

/**
 * @brief Get the best accelerator for current workload
 * @param system System handle
 * @param info Output accelerator info
 * @return true if accelerator found
 */
NIMCP_EXPORT bool portia_accelerator_get_best(portia_accelerator_system_t system,
                                               accelerator_info_t* info);

/**
 * @brief Get accelerator by type
 * @param system System handle
 * @param type Accelerator type
 * @param info Output accelerator info
 * @return true if found
 */
NIMCP_EXPORT bool portia_accelerator_get_by_type(portia_accelerator_system_t system,
                                                  accelerator_type_t type,
                                                  accelerator_info_t* info);

/**
 * @brief Get total count of detected accelerators
 * @param system System handle
 * @return Number of accelerators
 */
NIMCP_EXPORT uint32_t portia_accelerator_get_count(portia_accelerator_system_t system);

/**
 * @brief Get bitmask of available accelerator types
 * @param system System handle
 * @return Bitmask of accelerator_type_t values
 */
NIMCP_EXPORT uint32_t portia_accelerator_get_type_mask(portia_accelerator_system_t system);

//=============================================================================
// Selection API
//=============================================================================

/**
 * @brief Override automatic accelerator selection
 * @param system System handle
 * @param type Preferred accelerator type
 * @return true on success
 */
NIMCP_EXPORT bool portia_accelerator_set_preferred(portia_accelerator_system_t system,
                                                    accelerator_type_t type);

/**
 * @brief Get currently preferred accelerator type
 * @param system System handle
 * @return Preferred type
 */
NIMCP_EXPORT accelerator_type_t portia_accelerator_get_preferred(
    portia_accelerator_system_t system);

/**
 * @brief Check if specific accelerator type is available
 * @param system System handle
 * @param type Accelerator type to check
 * @return true if available
 */
NIMCP_EXPORT bool portia_accelerator_is_available(portia_accelerator_system_t system,
                                                   accelerator_type_t type);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get accelerator type name string
 * @param type Accelerator type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* portia_accelerator_type_name(accelerator_type_t type);

/**
 * @brief Print accelerator information to stdout
 * @param info Accelerator info to print
 */
NIMCP_EXPORT void portia_accelerator_print_info(const accelerator_info_t* info);

/**
 * @brief Print all detected accelerators
 * @param system System handle
 */
NIMCP_EXPORT void portia_accelerator_print_all(portia_accelerator_system_t system);

/**
 * @brief Estimate power consumption for workload
 * @param info Accelerator info
 * @param utilization_percent Expected utilization (0-100)
 * @return Estimated power in watts
 */
NIMCP_EXPORT float portia_accelerator_estimate_power(const accelerator_info_t* info,
                                                      float utilization_percent);

/**
 * @brief Calculate accelerator score for workload
 * @param info Accelerator info
 * @param config Configuration with requirements
 * @return Score (0-1, higher is better)
 */
NIMCP_EXPORT float portia_accelerator_calculate_score(const accelerator_info_t* info,
                                                       const portia_accelerator_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PORTIA_ACCELERATOR_H */
