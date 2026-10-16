#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_system_resources.c - System Resource Detection Implementation
//=============================================================================
/**
 * @file nimcp_system_resources.c
 * @brief Hardware-aware resource detection implementation
 *
 * PLATFORM SUPPORT:
 * - Linux: Full support (/proc, sysconf, CUDA/OpenCL)
 * - macOS: Basic support (sysctl)
 * - Windows: Basic support (GlobalMemoryStatusEx)
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include "utils/platform/nimcp_system_resources.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "api/nimcp_api_exception.h"

// Platform-specific headers
#ifdef __linux__
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#elif defined(_WIN32)
#include <windows.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(system_resources)

#endif

//=============================================================================
// Constants
//=============================================================================

#define KB_PER_NEURON_CPU  10   /**< Estimated KB per neuron (CPU mode) */
#define KB_PER_NEURON_GPU  5    /**< Estimated KB per neuron (GPU mode) */
#define SAFETY_MARGIN      0.8f /**< Use 80% of available RAM */

//=============================================================================
// Linux-Specific Implementation
//=============================================================================

#ifdef __linux__

/**
 * @brief Query RAM on Linux
 * WHY: /proc/meminfo provides accurate memory statistics
 */
static bool query_ram_linux(system_resources_t* res)
{
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_ram_linux: validation failed");
        return false;
    }

    res->total_ram_mb = (info.totalram * info.mem_unit) / (1024 * 1024);
    res->free_ram_mb = (info.freeram * info.mem_unit) / (1024 * 1024);

    // Available = free + buffers + cached (estimate)
    res->available_ram_mb = res->free_ram_mb + (info.bufferram * info.mem_unit) / (1024 * 1024);

    return true;
}

/**
 * @brief Query CPU on Linux
 * WHY: sysconf provides accurate CPU information
 */
static bool query_cpu_linux(system_resources_t* res)
{
    res->num_cpu_cores = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
    res->num_threads = res->num_cpu_cores;  // Approximation

    // Estimate cache size (rough)
    res->cpu_cache_kb = 256;  // Typical L3 cache per core

    return true;
}

/**
 * @brief Query disk space on Linux
 */
static bool query_disk_linux(system_resources_t* res)
{
    struct statvfs stat;
    if (statvfs(".", &stat) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_disk_linux: validation failed");
        return false;
    }

    res->available_disk_mb = (stat.f_bavail * stat.f_bsize) / (1024 * 1024);

    return true;
}

#endif // __linux__

//=============================================================================
// macOS-Specific Implementation
//=============================================================================

#ifdef __APPLE__

static bool query_ram_macos(system_resources_t* res)
{
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t memsize = 0;
    size_t length = sizeof(memsize);

    if (sysctl(mib, 2, &memsize, &length, NULL, 0) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_ram_macos: validation failed");
        return false;
    }

    res->total_ram_mb = memsize / (1024 * 1024);
    res->available_ram_mb = res->total_ram_mb / 2;  // Rough estimate
    res->free_ram_mb = res->available_ram_mb;

    return true;
}

static bool query_cpu_macos(system_resources_t* res)
{
    int mib[2] = {CTL_HW, HW_NCPU};
    int ncpu = 0;
    size_t length = sizeof(ncpu);

    if (sysctl(mib, 2, &ncpu, &length, NULL, 0) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_cpu_macos: validation failed");
        return false;
    }

    res->num_cpu_cores = (uint32_t)ncpu;
    res->num_threads = res->num_cpu_cores;
    res->cpu_cache_kb = 256;

    return true;
}

static bool query_disk_macos(system_resources_t* res)
{
    struct statvfs stat;
    if (statvfs(".", &stat) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_disk_macos: validation failed");
        return false;
    }

    res->available_disk_mb = (stat.f_bavail * stat.f_bsize) / (1024 * 1024);

    return true;
}

#endif // __APPLE__

//=============================================================================
// Windows-Specific Implementation
//=============================================================================

#ifdef _WIN32

static bool query_ram_windows(system_resources_t* res)
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (!GlobalMemoryStatusEx(&memInfo)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "query_ram_windows: GlobalMemoryStatusEx is NULL");
        return false;
    }

    res->total_ram_mb = memInfo.ullTotalPhys / (1024 * 1024);
    res->available_ram_mb = memInfo.ullAvailPhys / (1024 * 1024);
    res->free_ram_mb = res->available_ram_mb;

    return true;
}

static bool query_cpu_windows(system_resources_t* res)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    res->num_cpu_cores = sysInfo.dwNumberOfProcessors;
    res->num_threads = res->num_cpu_cores;
    res->cpu_cache_kb = 256;

    return true;
}

static bool query_disk_windows(system_resources_t* res)
{
    ULARGE_INTEGER freeBytesAvailable;
    if (!GetDiskFreeSpaceEx(NULL, &freeBytesAvailable, NULL, NULL)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "query_disk_windows: GetDiskFreeSpaceEx is NULL");
        return false;
    }

    res->available_disk_mb = freeBytesAvailable.QuadPart / (1024 * 1024);

    return true;
}

#endif // _WIN32

//=============================================================================
// GPU Detection (CUDA/OpenCL stub)
//=============================================================================

/**
 * @brief Query GPU capabilities
 * NOTE: This is a stub. Full CUDA/OpenCL detection requires linking
 * against those libraries. For now, return safe defaults.
 */
static bool query_gpu(gpu_basic_capabilities_t* gpu)
{
    // TODO: Implement actual CUDA/OpenCL detection
    // For now, assume no GPU
    memset(gpu, 0, sizeof(gpu_basic_capabilities_t));

    gpu->cuda_available = false;
    gpu->opencl_available = false;
    gpu->num_gpus = 0;
    gpu->total_vram_mb = 0;
    gpu->available_vram_mb = 0;

    return true;
}

//=============================================================================
// Neuromorphic Hardware Detection (stub)
//=============================================================================

/**
 * @brief Query neuromorphic hardware
 * NOTE: This is a stub. Actual detection would require vendor SDKs.
 */
static bool query_neuromorphic(neuromorphic_capabilities_t* neuro)
{
    memset(neuro, 0, sizeof(neuromorphic_capabilities_t));

    neuro->loihi_available = false;
    neuro->spinnaker_available = false;
    neuro->brainscales_available = false;
    neuro->num_cores = 0;
    neuro->neuron_capacity = 0;

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool system_resources_query(system_resources_t* resources)
{
    if (!resources) {
        LOG_ERROR("system_resources_query: resources pointer is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "System resources query failed: NULL pointer");
        return false;
    }

    memset(resources, 0, sizeof(system_resources_t));

    // Query platform-specific resources
#ifdef __linux__
    if (!query_ram_linux(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_ram_linux is NULL");
        return false;
    }
    if (!query_cpu_linux(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_cpu_linux is NULL");
        return false;
    }
    if (!query_disk_linux(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_disk_linux is NULL");
        return false;
    }
#elif defined(__APPLE__)
    if (!query_ram_macos(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_ram_macos is NULL");
        return false;
    }
    if (!query_cpu_macos(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_cpu_macos is NULL");
        return false;
    }
    if (!query_disk_macos(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_disk_macos is NULL");
        return false;
    }
#elif defined(_WIN32)
    if (!query_ram_windows(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_ram_windows is NULL");
        return false;
    }
    if (!query_cpu_windows(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_cpu_windows is NULL");
        return false;
    }
    if (!query_disk_windows(resources)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "system_resources_query: query_disk_windows is NULL");
        return false;
    }
#else
    // Unsupported platform - return safe defaults
    resources->total_ram_mb = 4096;       // 4GB default
    resources->available_ram_mb = 2048;   // 2GB available
    resources->free_ram_mb = 2048;
    resources->num_cpu_cores = 4;
    resources->num_threads = 4;
    resources->cpu_cache_kb = 256;
    resources->available_disk_mb = 10240; // 10GB
#endif

    // Query GPU and neuromorphic hardware (platform-independent)
    query_gpu(&resources->gpu);
    query_neuromorphic(&resources->neuro);

    return true;
}

uint32_t system_resources_estimate_max_neurons(const system_resources_t* resources, bool use_gpu)
{
    if (!resources) {
        LOG_WARN("system_resources_estimate_max_neurons: resources pointer is NULL, using safe default");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Estimate max neurons: NULL resources pointer");
        return 1000;  // Safe default
    }

    // Choose KB per neuron based on mode
    uint32_t kb_per_neuron = use_gpu ? KB_PER_NEURON_GPU : KB_PER_NEURON_CPU;

    // Calculate based on available RAM
    uint64_t available_kb = (uint64_t)(resources->available_ram_mb * 1024 * SAFETY_MARGIN);
    uint32_t max_neurons = (uint32_t)(available_kb / kb_per_neuron);

    // Cap by GPU VRAM if using GPU
    if (use_gpu && resources->gpu.cuda_available && resources->gpu.available_vram_mb > 0) {
        uint64_t vram_kb = resources->gpu.available_vram_mb * 1024;
        uint32_t gpu_max_neurons = (uint32_t)(vram_kb / kb_per_neuron);

        if (gpu_max_neurons < max_neurons) {
            max_neurons = gpu_max_neurons;
        }
    }

    // Cap by neuromorphic hardware if available
    if (resources->neuro.neuron_capacity > 0) {
        if (resources->neuro.neuron_capacity < max_neurons) {
            max_neurons = (uint32_t)resources->neuro.neuron_capacity;
        }
    }

    // Enforce reasonable bounds
    if (max_neurons < 100) {
        max_neurons = 100;  // Minimum viable brain
    }
    if (max_neurons > 100000000) {
        max_neurons = 100000000;  // 100M neuron cap
    }

    return max_neurons;
}

bool system_resources_can_resize(const system_resources_t* resources,
                                  uint32_t target_neurons, bool use_gpu)
{
    if (!resources) {
        LOG_WARN("system_resources_can_resize: resources pointer is NULL");
        return false;
    }

    uint32_t max_neurons = system_resources_estimate_max_neurons(resources, use_gpu);

    return target_neurons <= max_neurons;
}

uint32_t system_resources_recommend_size(const system_resources_t* resources,
                                          uint32_t current_neurons, bool use_gpu)
{
    if (!resources) {
        LOG_WARN("system_resources_recommend_size: resources pointer is NULL, using default growth factor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Recommend size: NULL resources pointer");
        return (uint32_t)(current_neurons * 1.5F);  // Default 1.5× growth
    }

    // Compute max safe size
    uint32_t max_neurons = system_resources_estimate_max_neurons(resources, use_gpu);

    // Desired growth: 1.5×
    uint32_t desired_size = (uint32_t)(current_neurons * 1.5F);

    // Cap at max safe size
    if (desired_size > max_neurons) {
        desired_size = max_neurons;
    }

    // Ensure we actually grow (at least +10%)
    uint32_t min_growth = (uint32_t)(current_neurons * 1.1F);
    if (desired_size < min_growth && min_growth <= max_neurons) {
        desired_size = min_growth;
    }

    return desired_size;
}
