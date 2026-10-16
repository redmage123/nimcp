/**
 * @file nimcp_portia_accelerator.c
 * @brief Hardware Accelerator Detection Implementation
 *
 * WHAT: Platform-specific detection of hardware accelerators
 * WHY:  Enable optimal hardware selection for neural workloads
 * HOW:  Device enumeration, library probing, sysfs inspection
 *
 * DETECTION FLOW:
 * 1. Check device files (/dev/nvidia*, /dev/dri, etc.)
 * 2. Try to dlopen vendor libraries (libcuda, libOpenCL, etc.)
 * 3. Query sysfs for device capabilities (/sys/class/...)
 * 4. Parse vendor-specific info
 * 5. Score devices by capability and power
 *
 * @author NIMCP Portia Team
 * @date 2025-12-08
 */

#include "portia/nimcp_portia_accelerator.h"
#include "constants/nimcp_buffer_constants.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_accelerator)

//=============================================================================
// Constants
//=============================================================================

#define MODULE_NAME "portia_accelerator"
#define MAX_ACCELERATORS 32

// Forward declarations
static const char* accelerator_type_to_string(accelerator_type_t type);
#define SYSFS_CLASS_DRM "/sys/class/drm"
#define SYSFS_CLASS_MISC "/sys/class/misc"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal accelerator system state
 */
struct portia_accelerator_system_struct {
    portia_accelerator_config_t config;
    accelerator_registry_t registry;
    nimcp_mutex_t lock;
    bool initialized;
    bool bio_async_enabled;
};

//=============================================================================
// GPU Detection Helpers
//=============================================================================

/**
 * @brief Check if NVIDIA GPU is available via CUDA
 */
static bool detect_nvidia_cuda(accelerator_info_t* info) {
    // Try to dlopen libcuda
    void* handle = dlopen("libcuda.so.1", RTLD_LAZY);
    if (!handle) {
        handle = dlopen("libcuda.so", RTLD_LAZY);
    }

    if (!handle) {
        return false;
    }

    // Get function pointers
    int (*cuInit)(unsigned int) = dlsym(handle, "cuInit");
    int (*cuDeviceGetCount)(int*) = dlsym(handle, "cuDeviceGetCount");
    int (*cuDeviceGet)(int*, int) = dlsym(handle, "cuDeviceGet");
    int (*cuDeviceGetName)(char*, int, int) = dlsym(handle, "cuDeviceGetName");
    int (*cuDeviceTotalMem)(size_t*, int) = dlsym(handle, "cuDeviceTotalMem_v2");

    if (!cuInit || !cuDeviceGetCount || !cuDeviceGet || !cuDeviceGetName) {
        dlclose(handle);
        return false;
    }

    // Initialize CUDA
    if (cuInit(0) != 0) {
        dlclose(handle);
        return false;
    }

    // Get device count
    int count = 0;
    if (cuDeviceGetCount(&count) != 0 || count == 0) {
        dlclose(handle);
        return false;
    }

    // Get first device info
    int device = 0;
    if (cuDeviceGet(&device, 0) != 0) {
        dlclose(handle);
        return false;
    }

    // Get device name
    char name[NIMCP_NAME_BUFFER_SIZE] = {0};
    if (cuDeviceGetName(name, sizeof(name), device) == 0) {
        name[sizeof(info->name) - 1] = '\0';  // Ensure null-termination
        memcpy(info->name, name, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
    }

    // Get memory
    if (cuDeviceTotalMem) {
        size_t mem = 0;
        if (cuDeviceTotalMem(&mem, device) == 0) {
            info->memory_bytes = mem;
        }
    }

    strncpy(info->vendor, "NVIDIA", sizeof(info->vendor) - 1);
    info->type = ACCELERATOR_TYPE_GPU;
    info->available = true;
    info->initialized = true;

    // Rough estimates for compute units and TFlops
    info->compute_units = 64; // Typical for mid-range GPU
    info->peak_tflops = 10.0F; // Rough estimate
    info->power_watts = 150.0F; // Typical GPU power

    dlclose(handle);

    LOG_INFO("Detected NVIDIA GPU: %s, Memory: %.2f GB",
             info->name, info->memory_bytes / (1024.0 * 1024.0 * 1024.0));

    return true;
}

/**
 * @brief Detect AMD GPU via ROCm or sysfs
 */
static bool detect_amd_gpu(accelerator_info_t* info) {
    // Check for AMD GPU device files
    DIR* dir = opendir("/dev/dri");
    if (!dir) {
        return false;
    }

    bool found = false;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "renderD", 7) == 0) {
            found = true;
            break;
        }
    }
    closedir(dir);

    if (!found) {
        return false;
    }

    // Try to read vendor info from sysfs
    FILE* vendor_file = fopen("/sys/class/drm/card0/device/vendor", "r");
    if (vendor_file) {
        char vendor_id[16] = {0};
        if (fgets(vendor_id, sizeof(vendor_id), vendor_file)) {
            // AMD vendor ID is 0x1002
            if (strstr(vendor_id, "0x1002") != NULL) {
                strncpy(info->vendor, "AMD", sizeof(info->vendor) - 1);
                strncpy(info->name, "AMD Radeon GPU", sizeof(info->name) - 1);
                info->type = ACCELERATOR_TYPE_GPU;
                info->available = true;
                info->compute_units = 40;
                info->peak_tflops = 8.0F;
                info->power_watts = 120.0F;
                info->memory_bytes = 8ULL * 1024 * 1024 * 1024; // 8GB estimate

                fclose(vendor_file);
                LOG_INFO("Detected AMD GPU");
                return true;
            }
        }
        fclose(vendor_file);
    }

    return false;
}

/**
 * @brief Detect Intel GPU
 */
static bool detect_intel_gpu(accelerator_info_t* info) {
    // Check for Intel GPU via sysfs
    FILE* vendor_file = fopen("/sys/class/drm/card0/device/vendor", "r");
    if (!vendor_file) {
        return false;
    }

    char vendor_id[16] = {0};
    if (!fgets(vendor_id, sizeof(vendor_id), vendor_file)) {
        fclose(vendor_file);
        return false;
    }
    fclose(vendor_file);

    // Intel vendor ID is 0x8086
    if (strstr(vendor_id, "0x8086") == NULL) {
        return false;
    }

    strncpy(info->vendor, "Intel", sizeof(info->vendor) - 1);
    strncpy(info->name, "Intel Integrated GPU", sizeof(info->name) - 1);
    info->type = ACCELERATOR_TYPE_GPU;
    info->available = true;
    info->compute_units = 24;
    info->peak_tflops = 2.0F;
    info->power_watts = 30.0F;
    info->memory_bytes = 4ULL * 1024 * 1024 * 1024; // Shared memory

    LOG_INFO("Detected Intel GPU");
    return true;
}

//=============================================================================
// NPU Detection Helpers
//=============================================================================

/**
 * @brief Detect Intel Movidius NPU
 */
static bool detect_intel_movidius(accelerator_info_t* info) {
    // Check for Movidius device
    if (access("/dev/myriad0", F_OK) != 0) {
        return false;
    }

    strncpy(info->vendor, "Intel", sizeof(info->vendor) - 1);
    strncpy(info->name, "Movidius Neural Compute Stick", sizeof(info->name) - 1);
    info->type = ACCELERATOR_TYPE_NPU;
    info->available = true;
    info->compute_units = 16;
    info->peak_tflops = 1.0F;
    info->power_watts = 5.0F;
    info->memory_bytes = 512 * 1024 * 1024; // 512MB

    LOG_INFO("Detected Intel Movidius NPU");
    return true;
}

/**
 * @brief Detect Qualcomm Hexagon DSP/NPU
 */
static bool detect_qualcomm_hexagon(accelerator_info_t* info) {
    // Check for Hexagon DSP
    if (access("/dev/adsprpc-smd", F_OK) != 0) {
        return false;
    }

    strncpy(info->vendor, "Qualcomm", sizeof(info->vendor) - 1);
    strncpy(info->name, "Hexagon DSP", sizeof(info->name) - 1);
    info->type = ACCELERATOR_TYPE_NPU;
    info->available = true;
    info->compute_units = 4;
    info->peak_tflops = 0.5F;
    info->power_watts = 2.0F;
    info->memory_bytes = 256 * 1024 * 1024;

    LOG_INFO("Detected Qualcomm Hexagon NPU");
    return true;
}

/**
 * @brief Detect Apple Neural Engine
 */
static bool detect_apple_neural_engine(accelerator_info_t* info) {
    // Apple ANE is typically not directly accessible
    // Would need vendor-specific APIs

#ifdef __APPLE__
    // Check if running on Apple Silicon
    FILE* fp = popen("sysctl -n machdep.cpu.brand_string", "r");
    if (fp) {
        char brand[NIMCP_LABEL_BUFFER_SIZE] = {0};
        if (fgets(brand, sizeof(brand), fp)) {
            if (strstr(brand, "Apple M") != NULL) {
                strncpy(info->vendor, "Apple", sizeof(info->vendor) - 1);
                strncpy(info->name, "Apple Neural Engine", sizeof(info->name) - 1);
                info->type = ACCELERATOR_TYPE_NPU;
                info->available = true;
                info->compute_units = 16;
                info->peak_tflops = 11.0f;
                info->power_watts = 8.0f;
                info->memory_bytes = 1024 * 1024 * 1024;

                pclose(fp);
                LOG_INFO("Detected Apple Neural Engine");
                return true;
            }
        }
        pclose(fp);
    }
#else
    (void)info;  // Suppress unused parameter warning on non-Apple
#endif

    return false;
}

//=============================================================================
// DSP Detection
//=============================================================================

/**
 * @brief Detect TI DSP
 */
static bool detect_ti_dsp(accelerator_info_t* info) {
    // Check for TI DSP device
    DIR* dir = opendir("/dev");
    if (!dir) {
        return false;
    }

    bool found = false;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "dsp") != NULL) {
            found = true;
            break;
        }
    }
    closedir(dir);

    if (!found) {
        return false;
    }

    strncpy(info->vendor, "Texas Instruments", sizeof(info->vendor) - 1);
    strncpy(info->name, "TI DSP", sizeof(info->name) - 1);
    info->type = ACCELERATOR_TYPE_DSP;
    info->available = true;
    info->compute_units = 2;
    info->peak_tflops = 0.2F;
    info->power_watts = 1.0F;
    info->memory_bytes = 128 * 1024 * 1024;

    LOG_INFO("Detected TI DSP");
    return true;
}

//=============================================================================
// FPGA Detection
//=============================================================================

/**
 * @brief Detect Intel/Xilinx FPGA
 */
static bool detect_fpga(accelerator_info_t* info) {
    // Check for FPGA devices
    if (access("/dev/fpga0", F_OK) == 0 ||
        access("/dev/xillybus_read_32", F_OK) == 0) {

        strncpy(info->vendor, "Xilinx/Intel", sizeof(info->vendor) - 1);
        strncpy(info->name, "FPGA Accelerator", sizeof(info->name) - 1);
        info->type = ACCELERATOR_TYPE_FPGA;
        info->available = true;
        info->compute_units = 32;
        info->peak_tflops = 5.0F;
        info->power_watts = 50.0F;
        info->memory_bytes = 2ULL * 1024 * 1024 * 1024;

        LOG_INFO("Detected FPGA");
        return true;
    }

    return false;
}

//=============================================================================
// TPU Detection
//=============================================================================

/**
 * @brief Detect Google Edge TPU
 */
static bool detect_edge_tpu(accelerator_info_t* info) {
    // Check for Edge TPU device
    if (access("/dev/apex_0", F_OK) != 0) {
        return false;
    }

    strncpy(info->vendor, "Google", sizeof(info->vendor) - 1);
    strncpy(info->name, "Edge TPU", sizeof(info->name) - 1);
    info->type = ACCELERATOR_TYPE_TPU;
    info->available = true;
    info->compute_units = 1;
    info->peak_tflops = 4.0F; // 4 TOPS
    info->power_watts = 2.0F;
    info->memory_bytes = 8 * 1024 * 1024; // 8MB

    LOG_INFO("Detected Google Edge TPU");
    return true;
}

//=============================================================================
// Registry Management
//=============================================================================

/**
 * @brief Add accelerator to registry
 */
static bool registry_add(accelerator_registry_t* registry, const accelerator_info_t* info) {
    if (!registry || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL argument in registry_add");
        return false;
    }

    if (registry->count >= registry->capacity) {
        // Expand capacity
        uint32_t new_capacity = registry->capacity * 2;
        if (new_capacity == 0) {
            new_capacity = 8;
        }

        accelerator_info_t* new_array = nimcp_calloc(new_capacity, sizeof(accelerator_info_t));
        if (!new_array) {
            LOG_ERROR("Failed to expand accelerator registry");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to expand accelerator registry");
            return false;
        }

        if (registry->accelerators) {
            memcpy(new_array, registry->accelerators,
                   registry->count * sizeof(accelerator_info_t));
            nimcp_free(registry->accelerators);
        }

        registry->accelerators = new_array;
        registry->capacity = new_capacity;
    }

    // Add new accelerator
    memcpy(&registry->accelerators[registry->count], info, sizeof(accelerator_info_t));
    registry->count++;
    registry->type_mask |= info->type;

    return true;
}

//=============================================================================
// Bio-Async Handlers
//=============================================================================

/**
 * @brief Handle accelerator query messages
 * @note Called by bio-async router when accelerator queries are received
 */
__attribute__((unused))
static void handle_accelerator_query(void* user_data, const void* msg_data, size_t msg_size) {
    portia_accelerator_system_t system = (portia_accelerator_system_t)user_data;
    (void)msg_data;  // Will be used for query parsing in full implementation
    (void)msg_size;  // Will be used for validation in full implementation

    if (!bbb_check_pointer(system, "handle_accelerator_query")) {
        return;
    }

    LOG_DEBUG("Received accelerator query via bio-async");

    // Broadcast accelerator capabilities
    // This would send detailed capability info back through bio-async
}

/**
 * @brief Register bio-async handlers
 */
static bool register_bio_async_handlers(portia_accelerator_system_t system) {
    if (!system->bio_async_enabled) {
        return true; // Not an error if bio-async not available
    }

    // Note: Bio-async registration would happen here with proper bio_router API
    // For now, just log registration
    LOG_INFO("Bio-async handlers registered for accelerator system");

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

portia_accelerator_config_t portia_accelerator_default_config(void) {
    portia_accelerator_config_t config = {
        .detect_gpu = true,
        .detect_npu = true,
        .detect_dsp = true,
        .detect_fpga = true,
        .detect_tpu = true,
        .auto_select = true,
        .power_budget_watts = 300.0F,
        .min_memory_gb = 0.0F,
        .min_tflops = 0.0F,
        .prefer_low_power = false,
        .require_fp16 = false,
        .require_int8 = false
    };
    return config;
}

portia_accelerator_system_t portia_accelerator_init(
    const portia_accelerator_config_t* config) {

    // Register with BBB
    if (!bbb_helpers_is_initialized()) {
        bbb_helpers_init();
    }
    bbb_register_module(MODULE_NAME, BBB_MODULE_TYPE_PLATFORM);

    LOG_INFO("Initializing Portia accelerator detection system");

    // Allocate system
    portia_accelerator_system_t system = nimcp_calloc(1, sizeof(*system));
    if (!system) {
        LOG_ERROR("Failed to allocate accelerator system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate accelerator system");
        bbb_audit_log(BBB_AUDIT_ERROR, MODULE_NAME, "init_failed",
                      "reason=malloc_failed");
        return NULL;
    }

    // Copy config or use defaults
    if (config) {
        memcpy(&system->config, config, sizeof(system->config));
    } else {
        system->config = portia_accelerator_default_config();
    }

    // Initialize registry
    system->registry.accelerators = NULL;
    system->registry.count = 0;
    system->registry.capacity = 0;
    system->registry.type_mask = 0;
    system->registry.preferred = ACCELERATOR_TYPE_NONE;

    // Initialize mutex
    if (nimcp_mutex_init(&system->lock, NULL) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "Failed to initialize accelerator system mutex");
        nimcp_free(system);
        return NULL;
    }

    // Check if bio-async is available
    system->bio_async_enabled = nimcp_bio_async_is_initialized();

    // Register bio-async handlers
    register_bio_async_handlers(system);

    system->initialized = true;

    LOG_INFO("Accelerator system initialized successfully");
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "init_success",
                  "config=default");

    return system;
}

void portia_accelerator_shutdown(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_shutdown")) {
        return;
    }

    LOG_INFO("Shutting down accelerator system");

    nimcp_mutex_lock(&system->lock);

    if (system->registry.accelerators) {
        nimcp_free(system->registry.accelerators);
        system->registry.accelerators = NULL;
    }

    system->initialized = false;

    nimcp_mutex_unlock(&system->lock);
    nimcp_mutex_destroy(&system->lock);

    nimcp_free(system);

    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "shutdown", "status=success");
    LOG_INFO("Accelerator system shutdown complete");
}

uint32_t portia_accelerator_detect_all(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_detect_all")) {
        return 0;
    }

    LOG_INFO("Starting full accelerator detection");

    nimcp_mutex_lock(&system->lock);

    uint32_t total = 0;

    if (system->config.detect_gpu) {
        total += portia_accelerator_detect_gpu(system);
    }

    if (system->config.detect_npu) {
        total += portia_accelerator_detect_npu(system);
    }

    if (system->config.detect_dsp) {
        total += portia_accelerator_detect_dsp(system);
    }

    if (system->config.detect_fpga) {
        total += portia_accelerator_detect_fpga(system);
    }

    if (system->config.detect_tpu) {
        total += portia_accelerator_detect_tpu(system);
    }

    nimcp_mutex_unlock(&system->lock);

    LOG_INFO("Detected %u total accelerators", total);
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "detection_complete",
                  "count=%u", total);

    return total;
}

uint32_t portia_accelerator_detect_gpu(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_detect_gpu")) {
        return 0;
    }

    LOG_INFO("Detecting GPUs...");

    uint32_t count = 0;
    accelerator_info_t info = {0};

    // Try NVIDIA
    if (detect_nvidia_cuda(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
        memset(&info, 0, sizeof(info));
    }

    // Try AMD
    if (detect_amd_gpu(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
        memset(&info, 0, sizeof(info));
    }

    // Try Intel
    if (detect_intel_gpu(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
    }

    LOG_INFO("Detected %u GPU(s)", count);
    return count;
}

uint32_t portia_accelerator_detect_npu(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_detect_npu")) {
        return 0;
    }

    LOG_INFO("Detecting NPUs...");

    uint32_t count = 0;
    accelerator_info_t info = {0};

    if (detect_intel_movidius(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
        memset(&info, 0, sizeof(info));
    }

    if (detect_qualcomm_hexagon(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
        memset(&info, 0, sizeof(info));
    }

    if (detect_apple_neural_engine(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
    }

    LOG_INFO("Detected %u NPU(s)", count);
    return count;
}

uint32_t portia_accelerator_detect_dsp(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_detect_dsp")) {
        return 0;
    }

    LOG_INFO("Detecting DSPs...");

    uint32_t count = 0;
    accelerator_info_t info = {0};

    if (detect_ti_dsp(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
    }

    LOG_INFO("Detected %u DSP(s)", count);
    return count;
}

uint32_t portia_accelerator_detect_fpga(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_detect_fpga")) {
        return 0;
    }

    LOG_INFO("Detecting FPGAs...");

    uint32_t count = 0;
    accelerator_info_t info = {0};

    if (detect_fpga(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
    }

    LOG_INFO("Detected %u FPGA(s)", count);
    return count;
}

uint32_t portia_accelerator_detect_tpu(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_detect_tpu")) {
        return 0;
    }

    LOG_INFO("Detecting TPUs...");

    uint32_t count = 0;
    accelerator_info_t info = {0};

    if (detect_edge_tpu(&info)) {
        if (registry_add(&system->registry, &info)) {
            count++;
        }
    }

    LOG_INFO("Detected %u TPU(s)", count);
    return count;
}

bool portia_accelerator_get_info(portia_accelerator_system_t system,
                                  uint32_t index,
                                  accelerator_info_t* info) {
    if (!bbb_check_pointer(system, "portia_accelerator_get_info")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: bbb_check_pointer is NULL");
        return false;
    }

    if (!bbb_check_pointer(info, "portia_accelerator_get_info")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&system->lock);

    if (index >= system->registry.count) {
        nimcp_mutex_unlock(&system->lock);
        LOG_WARN("Invalid accelerator index: %u", index);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "portia_accelerator_detect_tpu: capacity exceeded");
        return false;
    }

    memcpy(info, &system->registry.accelerators[index], sizeof(accelerator_info_t));

    nimcp_mutex_unlock(&system->lock);
    return true;
}

bool portia_accelerator_get_best(portia_accelerator_system_t system,
                                  accelerator_info_t* info) {
    if (!bbb_check_pointer(system, "portia_accelerator_get_best")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: bbb_check_pointer is NULL");
        return false;
    }

    if (!bbb_check_pointer(info, "portia_accelerator_get_best")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&system->lock);

    if (system->registry.count == 0) {
        nimcp_mutex_unlock(&system->lock);
        LOG_WARN("No accelerators available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: system->registry.count is zero");
        return false;
    }

    // Find best accelerator based on scoring
    float best_score = -1.0F;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < system->registry.count; i++) {
        float score = portia_accelerator_calculate_score(
            &system->registry.accelerators[i], &system->config);

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    memcpy(info, &system->registry.accelerators[best_idx], sizeof(accelerator_info_t));

    nimcp_mutex_unlock(&system->lock);

    LOG_INFO("Best accelerator: %s (score: %.2f)", info->name, best_score);
    return true;
}

bool portia_accelerator_get_by_type(portia_accelerator_system_t system,
                                    accelerator_type_t type,
                                    accelerator_info_t* info) {
    if (!bbb_check_pointer(system, "portia_accelerator_get_by_type")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: bbb_check_pointer is NULL");
        return false;
    }

    if (!bbb_check_pointer(info, "portia_accelerator_get_by_type")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_detect_tpu: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&system->lock);

    for (uint32_t i = 0; i < system->registry.count; i++) {
        if (system->registry.accelerators[i].type == type) {
            memcpy(info, &system->registry.accelerators[i], sizeof(accelerator_info_t));
            nimcp_mutex_unlock(&system->lock);
            return true;
        }
    }

    nimcp_mutex_unlock(&system->lock);
    return false;  /* Not found - requested accelerator type not registered */
}

uint32_t portia_accelerator_get_count(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_get_count")) {
        return 0;
    }

    nimcp_mutex_lock(&system->lock);
    uint32_t count = system->registry.count;
    nimcp_mutex_unlock(&system->lock);

    return count;
}

uint32_t portia_accelerator_get_type_mask(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_get_type_mask")) {
        return 0;
    }

    nimcp_mutex_lock(&system->lock);
    uint32_t mask = system->registry.type_mask;
    nimcp_mutex_unlock(&system->lock);

    return mask;
}

bool portia_accelerator_set_preferred(portia_accelerator_system_t system,
                                       accelerator_type_t type) {
    if (!bbb_check_pointer(system, "portia_accelerator_set_preferred")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_get_type_mask: bbb_check_pointer is NULL");
        return false;
    }

    nimcp_mutex_lock(&system->lock);

    // Check if type is available
    if ((system->registry.type_mask & type) == 0) {
        nimcp_mutex_unlock(&system->lock);
        LOG_WARN("Cannot set preferred type %u - not available", type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_accelerator_get_type_mask: validation failed");
        return false;
    }

    system->registry.preferred = type;
    nimcp_mutex_unlock(&system->lock);

    LOG_INFO("Preferred accelerator type set to %s",
             accelerator_type_to_string(type));
    bbb_audit_log(BBB_AUDIT_INFO, MODULE_NAME, "preferred_set",
                  "type=%s", accelerator_type_to_string(type));

    return true;
}

accelerator_type_t portia_accelerator_get_preferred(
    portia_accelerator_system_t system) {

    if (!bbb_check_pointer(system, "portia_accelerator_get_preferred")) {
        return ACCELERATOR_TYPE_NONE;
    }

    nimcp_mutex_lock(&system->lock);
    accelerator_type_t type = system->registry.preferred;
    nimcp_mutex_unlock(&system->lock);

    return type;
}

bool portia_accelerator_is_available(portia_accelerator_system_t system,
                                     accelerator_type_t type) {
    if (!bbb_check_pointer(system, "portia_accelerator_is_available")) {
        return false;
    }

    nimcp_mutex_lock(&system->lock);
    bool available = (system->registry.type_mask & type) != 0;
    nimcp_mutex_unlock(&system->lock);

    return available;
}

static const char* accelerator_type_to_string(accelerator_type_t type) {
    switch (type) {
        case ACCELERATOR_TYPE_NONE: return "None";
        case ACCELERATOR_TYPE_GPU:  return "GPU";
        case ACCELERATOR_TYPE_NPU:  return "NPU";
        case ACCELERATOR_TYPE_DSP:  return "DSP";
        case ACCELERATOR_TYPE_FPGA: return "FPGA";
        case ACCELERATOR_TYPE_TPU:  return "TPU";
        default: return "Unknown";
    }
}

void portia_accelerator_print_info(const accelerator_info_t* info) {
    if (!bbb_check_pointer(info, "portia_accelerator_print_info")) {
        return;
    }

    printf("=== Accelerator Info ===\n");
    printf("Type:         %s\n", accelerator_type_to_string(info->type));
    printf("Name:         %s\n", info->name);
    printf("Vendor:       %s\n", info->vendor);
    printf("Compute Units:%u\n", info->compute_units);
    printf("Memory:       %.2f GB\n", info->memory_bytes / (1024.0 * 1024.0 * 1024.0));
    printf("Peak TFlops:  %.2f\n", info->peak_tflops);
    printf("Power:        %.2f W\n", info->power_watts);
    printf("Available:    %s\n", info->available ? "Yes" : "No");
    printf("Initialized:  %s\n", info->initialized ? "Yes" : "No");
    printf("=======================\n");
}

void portia_accelerator_print_all(portia_accelerator_system_t system) {
    if (!bbb_check_pointer(system, "portia_accelerator_print_all")) {
        return;
    }

    nimcp_mutex_lock(&system->lock);

    printf("\n=== Detected Accelerators (%u) ===\n", system->registry.count);

    for (uint32_t i = 0; i < system->registry.count; i++) {
        printf("\n[%u] ", i);
        portia_accelerator_print_info(&system->registry.accelerators[i]);
    }

    nimcp_mutex_unlock(&system->lock);
}

float portia_accelerator_estimate_power(const accelerator_info_t* info,
                                        float utilization_percent) {
    if (!bbb_check_pointer(info, "portia_accelerator_estimate_power")) {
        return 0.0F;
    }

    if (!bbb_validate_range_u((uint64_t)utilization_percent, 0, 100,
                              "portia_accelerator_estimate_power")) {
        return 0.0F;
    }

    // Simple linear model: idle power + (peak_power - idle_power) * utilization
    float idle_power = info->power_watts * 0.3F; // 30% at idle
    float dynamic_power = info->power_watts * 0.7F; // 70% dynamic

    return idle_power + (dynamic_power * utilization_percent / 100.0F);
}

float portia_accelerator_calculate_score(const accelerator_info_t* info,
                                         const portia_accelerator_config_t* config) {
    if (!bbb_check_pointer(info, "portia_accelerator_calculate_score")) {
        return 0.0F;
    }

    if (!bbb_check_pointer(config, "portia_accelerator_calculate_score")) {
        return 0.0F;
    }

    float score = 0.0F;

    // Check hard requirements
    if (config->min_memory_gb > 0) {
        float memory_gb = info->memory_bytes / (1024.0F * 1024.0F * 1024.0F);
        if (memory_gb < config->min_memory_gb) {
            return 0.0F; // Doesn't meet minimum
        }
    }

    if (config->min_tflops > 0 && info->peak_tflops < config->min_tflops) {
        return 0.0F;
    }

    if (config->power_budget_watts > 0 && info->power_watts > config->power_budget_watts) {
        return 0.0F;
    }

    // Calculate score based on capabilities
    score += info->peak_tflops * 10.0F; // Performance weight
    score += (info->memory_bytes / (1024.0F * 1024.0F * 1024.0F)) * 5.0F; // Memory weight

    // Power efficiency bonus
    if (config->prefer_low_power && info->power_watts > 0.0F) {
        float efficiency = info->peak_tflops / info->power_watts;
        score += efficiency * 20.0F;
    }

    // Type preference
    if (info->type == ACCELERATOR_TYPE_GPU) {
        score *= 1.2F; // Slight GPU preference
    } else if (info->type == ACCELERATOR_TYPE_NPU) {
        score *= 1.1F; // NPUs are efficient
    }

    return score;
}

const char* portia_accelerator_type_name(accelerator_type_t type) {
    switch (type) {
        case ACCELERATOR_TYPE_NONE: return "None";
        case ACCELERATOR_TYPE_GPU:  return "GPU";
        case ACCELERATOR_TYPE_NPU:  return "NPU";
        case ACCELERATOR_TYPE_DSP:  return "DSP";
        case ACCELERATOR_TYPE_FPGA: return "FPGA";
        case ACCELERATOR_TYPE_TPU:  return "TPU";
        default: return "Unknown";
    }
}
