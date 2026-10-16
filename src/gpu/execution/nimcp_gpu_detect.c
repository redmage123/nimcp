/**
 * @file nimcp_gpu_detect.c
 * @brief Runtime GPU Capability Detection Implementation
 *
 * WHAT: Detects GPU hardware and driver capabilities at runtime
 * WHY:  Enables optimal GPU backend selection without compile-time dependencies
 * HOW:  Runtime library loading and API probing for each GPU backend
 *
 * PLATFORM SUPPORT:
 * - Linux: dlopen for CUDA, ROCm, OpenCL libraries
 * - Windows: LoadLibrary for nvcuda.dll, OpenCL.dll
 * - macOS: dlopen for OpenCL framework
 *
 * RUNTIME DETECTION:
 * - CUDA: Load libcuda.so/nvcuda.dll, call cuInit, cuDeviceGetCount
 * - OpenCL: Load libOpenCL.so/OpenCL.dll, call clGetPlatformIDs
 * - ROCm: Load libamdhip64.so, call hipInit, hipGetDeviceCount
 *
 * FALLBACK:
 * - If library load fails: backend marked unavailable
 * - If API call fails: backend marked unavailable
 * - Graceful degradation to CPU mode
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#define LOG_MODULE "GPU_DETECT"
#define LOG_MODULE_ID 0x0903
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_detect)

#include "gpu/execution/nimcp_gpu_detect.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Platform-specific dynamic loading
#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE lib_handle_t;
    #define LIB_OPEN(name) LoadLibraryA(name)
    #define LIB_CLOSE(handle) FreeLibrary(handle)
    #define LIB_SYM(handle, name) GetProcAddress(handle, name)
#else
    #include <dlfcn.h>
    typedef void* lib_handle_t;
    #define LIB_OPEN(name) dlopen(name, RTLD_LAZY)
    #define LIB_CLOSE(handle) dlclose(handle)
    #define LIB_SYM(handle, name) dlsym(handle, name)
#endif

// Thread-safe initialization
#include <pthread.h>
#include "utils/thread/nimcp_thread.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// CUDA Types (for runtime loading without CUDA headers)
//=============================================================================

typedef int CUresult;
typedef int CUdevice;

#define CUDA_SUCCESS 0

// CUDA function pointer types
typedef CUresult (*cuInit_t)(unsigned int);
typedef CUresult (*cuDeviceGetCount_t)(int*);
typedef CUresult (*cuDeviceGet_t)(CUdevice*, int);
typedef CUresult (*cuDeviceGetName_t)(char*, int, CUdevice);
typedef CUresult (*cuDeviceTotalMem_t)(size_t*, CUdevice);
typedef CUresult (*cuDeviceGetAttribute_t)(int*, int, CUdevice);
typedef CUresult (*cuDriverGetVersion_t)(int*);

// CUDA device attributes
#define CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT          16
#define CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK         1
#define CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X               2
#define CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y               3
#define CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z               4
#define CU_DEVICE_ATTRIBUTE_WARP_SIZE                     10
#define CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR      75
#define CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR      76
#define CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE             36
#define CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH       37
#define CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING            41
#define CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS            31
#define CU_DEVICE_ATTRIBUTE_CLOCK_RATE                    13

//=============================================================================
// OpenCL Types (for runtime loading without OpenCL headers)
//=============================================================================

typedef int32_t cl_int;
typedef uint32_t cl_uint;
typedef uint64_t cl_ulong;
typedef void* cl_platform_id;
typedef void* cl_device_id;

#define CL_SUCCESS 0
#define CL_DEVICE_TYPE_GPU (1 << 2)

// OpenCL info queries
#define CL_PLATFORM_NAME       0x0902
#define CL_DEVICE_NAME         0x102B
#define CL_DEVICE_VENDOR       0x102C
#define CL_DEVICE_GLOBAL_MEM_SIZE 0x101F
#define CL_DEVICE_MAX_COMPUTE_UNITS 0x1002
#define CL_DEVICE_MAX_WORK_GROUP_SIZE 0x1004
#define CL_DEVICE_MAX_CLOCK_FREQUENCY 0x100C

// OpenCL function pointer types
typedef cl_int (*clGetPlatformIDs_t)(cl_uint, cl_platform_id*, cl_uint*);
typedef cl_int (*clGetPlatformInfo_t)(cl_platform_id, cl_uint, size_t, void*, size_t*);
typedef cl_int (*clGetDeviceIDs_t)(cl_platform_id, cl_ulong, cl_uint, cl_device_id*, cl_uint*);
typedef cl_int (*clGetDeviceInfo_t)(cl_device_id, cl_uint, size_t, void*, size_t*);

//=============================================================================
// ROCm/HIP Types (for runtime loading without HIP headers)
//=============================================================================

typedef int hipError_t;

#define hipSuccess 0

// HIP device properties structure (simplified)
typedef struct {
    char name[NIMCP_NAME_BUFFER_SIZE];
    size_t totalGlobalMem;
    int multiProcessorCount;
    int maxThreadsPerBlock;
    int maxThreadsDim[3];
    int warpSize;
    int clockRate;
    int memoryBusWidth;
    int memoryClockRate;
    int major;
    int minor;
    int concurrentKernels;
} hipDeviceProp_simplified_t;

// HIP function pointer types
typedef hipError_t (*hipInit_t)(unsigned int);
typedef hipError_t (*hipGetDeviceCount_t)(int*);
typedef hipError_t (*hipGetDeviceProperties_t)(hipDeviceProp_simplified_t*, int);
typedef hipError_t (*hipDriverGetVersion_t)(int*);

//=============================================================================
// Static State
//=============================================================================

static gpu_detect_result_t s_cached_caps = {0};
static pthread_once_t s_cache_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t s_refresh_mutex = NIMCP_MUTEX_INITIALIZER;

// Library handles (kept open for potential future use)
static lib_handle_t s_cuda_lib = NULL;
static lib_handle_t s_opencl_lib = NULL;
static lib_handle_t s_rocm_lib = NULL;
static lib_handle_t s_neuron_lib = NULL;

// Forward declaration for pthread_once callback
static void gpu_detect_init_impl(void);

//=============================================================================
// CUDA Detection
//=============================================================================

/**
 * @brief Detect CUDA devices via runtime library loading
 *
 * WHAT: Loads CUDA driver API and queries device info
 * WHY:  Runtime detection without compile-time CUDA dependency
 * HOW:  dlopen libcuda.so, call cuInit and device queries
 */
static void detect_cuda_devices(gpu_detect_result_t* caps)
{
    // Guard: Already have CUDA lib from previous detection
    if (s_cuda_lib != NULL) {
        return;
    }

    LOG_DEBUG("Attempting to detect CUDA devices");

    // Try to load CUDA driver library
#ifdef _WIN32
    s_cuda_lib = LIB_OPEN("nvcuda.dll");
#elif defined(__APPLE__)
    s_cuda_lib = LIB_OPEN("/usr/local/cuda/lib/libcuda.dylib");
    if (!s_cuda_lib) {
        s_cuda_lib = LIB_OPEN("libcuda.dylib");
    }
#else
    s_cuda_lib = LIB_OPEN("libcuda.so.1");
    if (!s_cuda_lib) {
        s_cuda_lib = LIB_OPEN("libcuda.so");
    }
#endif

    // Guard: Library not found
    if (!s_cuda_lib) {
        LOG_DEBUG("CUDA driver library not found");
        return;
    }

    // Load function pointers
    cuInit_t cuInit = (cuInit_t)LIB_SYM(s_cuda_lib, "cuInit");
    cuDeviceGetCount_t cuDeviceGetCount = (cuDeviceGetCount_t)LIB_SYM(s_cuda_lib, "cuDeviceGetCount");
    cuDeviceGet_t cuDeviceGet = (cuDeviceGet_t)LIB_SYM(s_cuda_lib, "cuDeviceGet");
    cuDeviceGetName_t cuDeviceGetName = (cuDeviceGetName_t)LIB_SYM(s_cuda_lib, "cuDeviceGetName");
    cuDeviceTotalMem_t cuDeviceTotalMem = (cuDeviceTotalMem_t)LIB_SYM(s_cuda_lib, "cuDeviceTotalMem_v2");
    cuDeviceGetAttribute_t cuDeviceGetAttribute = (cuDeviceGetAttribute_t)LIB_SYM(s_cuda_lib, "cuDeviceGetAttribute");
    cuDriverGetVersion_t cuDriverGetVersion = (cuDriverGetVersion_t)LIB_SYM(s_cuda_lib, "cuDriverGetVersion");

    // Guard: Required functions not found
    if (!cuInit || !cuDeviceGetCount || !cuDeviceGet || !cuDeviceGetName || !cuDeviceTotalMem || !cuDeviceGetAttribute) {
        LOG_DEBUG("Required CUDA functions not found");
        LIB_CLOSE(s_cuda_lib);
        s_cuda_lib = NULL;
        return;
    }

    // Initialize CUDA
    CUresult result = cuInit(0);
    if (result != CUDA_SUCCESS) {
        LOG_DEBUG("cuInit failed with error %d", result);
        LIB_CLOSE(s_cuda_lib);
        s_cuda_lib = NULL;
        return;
    }

    // Get driver version
    if (cuDriverGetVersion) {
        int version = 0;
        if (cuDriverGetVersion(&version) == CUDA_SUCCESS) {
            caps->cuda_driver_version = version;
        }
    }

    // Get device count
    int device_count = 0;
    result = cuDeviceGetCount(&device_count);
    if (result != CUDA_SUCCESS || device_count == 0) {
        LOG_DEBUG("No CUDA devices found");
        return;
    }

    caps->cuda_available = true;
    caps->cuda_device_count = (uint32_t)device_count;
    caps->available_backends |= GPU_BACKEND_CUDA;

    LOG_INFO("CUDA detected: driver=%d, devices=%d", caps->cuda_driver_version, device_count);

    // Query each device
    for (int i = 0; i < device_count && caps->device_count < 16; i++) {
        CUdevice device;
        if (cuDeviceGet(&device, i) != CUDA_SUCCESS) {
            continue;
        }

        gpu_device_info_t* info = &caps->devices[caps->device_count];
        info->device_index = i;
        info->vendor = GPU_VENDOR_NVIDIA;
        info->backend = GPU_BACKEND_CUDA;

        // Get device name
        cuDeviceGetName(info->name, sizeof(info->name), device);

        // Get total memory
        size_t total_mem = 0;
        if (cuDeviceTotalMem(&total_mem, device) == CUDA_SUCCESS) {
            info->total_memory_bytes = total_mem;
            // Estimate free memory as 90% of total (conservative)
            info->free_memory_bytes = (uint64_t)(total_mem * 0.9);
        }

        // Get compute capability
        int major = 0, minor = 0;
        cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, device);
        cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, device);
        info->compute_capability_major = (uint32_t)major;
        info->compute_capability_minor = (uint32_t)minor;

        // Get compute units (SMs)
        int sm_count = 0;
        cuDeviceGetAttribute(&sm_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, device);
        info->compute_units = (uint32_t)sm_count;

        // Get thread/block limits
        int max_threads = 0;
        cuDeviceGetAttribute(&max_threads, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, device);
        info->max_threads_per_block = (uint32_t)max_threads;

        int max_dim_x = 0, max_dim_y = 0, max_dim_z = 0;
        cuDeviceGetAttribute(&max_dim_x, CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X, device);
        cuDeviceGetAttribute(&max_dim_y, CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y, device);
        cuDeviceGetAttribute(&max_dim_z, CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z, device);
        info->max_block_dim_x = (uint32_t)max_dim_x;
        info->max_block_dim_y = (uint32_t)max_dim_y;
        info->max_block_dim_z = (uint32_t)max_dim_z;

        // Get warp size
        int warp_size = 0;
        cuDeviceGetAttribute(&warp_size, CU_DEVICE_ATTRIBUTE_WARP_SIZE, device);
        info->warp_size = (uint32_t)warp_size;

        // Get clock speeds
        int clock_rate = 0;
        cuDeviceGetAttribute(&clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, device);
        info->core_clock_mhz = (uint32_t)(clock_rate / 1000);
        info->boost_clock_mhz = info->core_clock_mhz;  // Boost not available via driver API

        // Get memory info
        int mem_clock = 0, mem_bus_width = 0;
        cuDeviceGetAttribute(&mem_clock, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE, device);
        cuDeviceGetAttribute(&mem_bus_width, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH, device);
        info->memory_clock_mhz = (uint32_t)(mem_clock / 1000);
        info->memory_bus_width = (uint32_t)mem_bus_width;

        // Calculate memory bandwidth: (bus_width / 8) * mem_clock * 2 (DDR) / 1000 (GB/s)
        if (mem_bus_width > 0 && mem_clock > 0) {
            info->memory_bandwidth_gbps = (float)(mem_bus_width / 8.0 * mem_clock * 2.0 / 1000000.0);
        }

        // Get feature support
        int unified = 0, concurrent = 0;
        cuDeviceGetAttribute(&unified, CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING, device);
        cuDeviceGetAttribute(&concurrent, CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS, device);
        info->supports_unified_memory = (unified != 0);
        info->supports_concurrent_kernels = (concurrent != 0);

        // Features based on compute capability
        info->supports_fp16 = (major >= 5 || (major == 5 && minor >= 3));
        info->supports_fp64 = true;  // All CUDA GPUs support FP64
        info->supports_tensor_cores = (major >= 7);  // Volta and later
        info->supports_p2p = true;  // Assume P2P support

        // Update aggregate stats
        caps->total_gpu_memory_bytes += info->total_memory_bytes;
        caps->total_compute_units += info->compute_units;

        // Estimate TFLOPS: SMs * cores_per_sm * 2 (FMA) * clock_mhz / 1000000
        // Cores per SM varies by architecture, use 128 as approximation for modern GPUs
        int cores_per_sm = (major >= 8) ? 128 : (major >= 7) ? 64 : 128;
        float tflops = (float)sm_count * cores_per_sm * 2.0f * (clock_rate / 1000.0f) / 1000000.0f;
        caps->estimated_tflops += tflops;

        caps->device_count++;

        LOG_DEBUG("CUDA device %d: %s, SM=%d, Mem=%lu MB, CC=%d.%d",
                  i, info->name, sm_count, (unsigned long)(total_mem / (1024*1024)), major, minor);
    }
}

//=============================================================================
// OpenCL Detection
//=============================================================================

/**
 * @brief Detect OpenCL devices via runtime library loading
 *
 * WHAT: Loads OpenCL library and queries platforms/devices
 * WHY:  Cross-platform GPU detection for AMD, Intel, and NVIDIA
 * HOW:  dlopen libOpenCL.so, call clGetPlatformIDs, clGetDeviceIDs
 */
static void detect_opencl_devices(gpu_detect_result_t* caps)
{
    // Guard: Already have OpenCL lib from previous detection
    if (s_opencl_lib != NULL) {
        return;
    }

    LOG_DEBUG("Attempting to detect OpenCL devices");

    // Try to load OpenCL library
#ifdef _WIN32
    s_opencl_lib = LIB_OPEN("OpenCL.dll");
#elif defined(__APPLE__)
    s_opencl_lib = LIB_OPEN("/System/Library/Frameworks/OpenCL.framework/OpenCL");
#else
    s_opencl_lib = LIB_OPEN("libOpenCL.so.1");
    if (!s_opencl_lib) {
        s_opencl_lib = LIB_OPEN("libOpenCL.so");
    }
#endif

    // Guard: Library not found
    if (!s_opencl_lib) {
        LOG_DEBUG("OpenCL library not found");
        return;
    }

    // Load function pointers
    clGetPlatformIDs_t clGetPlatformIDs = (clGetPlatformIDs_t)LIB_SYM(s_opencl_lib, "clGetPlatformIDs");
    clGetPlatformInfo_t clGetPlatformInfo = (clGetPlatformInfo_t)LIB_SYM(s_opencl_lib, "clGetPlatformInfo");
    clGetDeviceIDs_t clGetDeviceIDs = (clGetDeviceIDs_t)LIB_SYM(s_opencl_lib, "clGetDeviceIDs");
    clGetDeviceInfo_t clGetDeviceInfo = (clGetDeviceInfo_t)LIB_SYM(s_opencl_lib, "clGetDeviceInfo");

    // Guard: Required functions not found
    if (!clGetPlatformIDs || !clGetPlatformInfo || !clGetDeviceIDs || !clGetDeviceInfo) {
        LOG_DEBUG("Required OpenCL functions not found");
        LIB_CLOSE(s_opencl_lib);
        s_opencl_lib = NULL;
        return;
    }

    // Get platform count
    cl_uint platform_count = 0;
    cl_int result = clGetPlatformIDs(0, NULL, &platform_count);
    if (result != CL_SUCCESS || platform_count == 0) {
        LOG_DEBUG("No OpenCL platforms found");
        return;
    }

    caps->opencl_available = true;
    caps->opencl_platform_count = platform_count;
    caps->available_backends |= GPU_BACKEND_OPENCL;

    // Get platform IDs
    cl_platform_id platforms[8];
    cl_uint actual_platforms = (platform_count > 8) ? 8 : platform_count;
    clGetPlatformIDs(actual_platforms, platforms, NULL);

    LOG_INFO("OpenCL detected: platforms=%u", platform_count);

    // Query each platform
    for (cl_uint p = 0; p < actual_platforms; p++) {
        // Get platform name
        char platform_name[NIMCP_LABEL_BUFFER_SIZE] = {0};
        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);

        if (p < 4) {
            strncpy(caps->opencl_platform_names[p], platform_name, sizeof(caps->opencl_platform_names[p]) - 1);
        }

        LOG_DEBUG("OpenCL platform %u: %s", p, platform_name);

        // Get GPU devices on this platform
        cl_uint device_count = 0;
        result = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, 0, NULL, &device_count);
        if (result != CL_SUCCESS || device_count == 0) {
            continue;
        }

        caps->opencl_device_count += device_count;

        // Get device IDs
        cl_device_id devices[8];
        cl_uint actual_devices = (device_count > 8) ? 8 : device_count;
        clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, actual_devices, devices, NULL);

        // Query each device
        for (cl_uint d = 0; d < actual_devices && caps->device_count < 16; d++) {
            // Check if this device was already detected via CUDA
            char dev_name[NIMCP_NAME_BUFFER_SIZE] = {0};
            clGetDeviceInfo(devices[d], CL_DEVICE_NAME, sizeof(dev_name), dev_name, NULL);

            // Skip if already detected via CUDA (avoid duplicates)
            bool duplicate = false;
            for (uint32_t i = 0; i < caps->device_count; i++) {
                if (strstr(caps->devices[i].name, dev_name) != NULL ||
                    strstr(dev_name, caps->devices[i].name) != NULL) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                LOG_DEBUG("Skipping duplicate OpenCL device: %s", dev_name);
                continue;
            }

            gpu_device_info_t* info = &caps->devices[caps->device_count];
            info->device_index = (int)d;
            info->backend = GPU_BACKEND_OPENCL;
            strncpy(info->name, dev_name, sizeof(info->name) - 1);

            // Determine vendor from name or vendor string
            char vendor_str[NIMCP_LABEL_BUFFER_SIZE] = {0};
            clGetDeviceInfo(devices[d], CL_DEVICE_VENDOR, sizeof(vendor_str), vendor_str, NULL);

            if (strstr(vendor_str, "NVIDIA") || strstr(vendor_str, "nvidia")) {
                info->vendor = GPU_VENDOR_NVIDIA;
            } else if (strstr(vendor_str, "AMD") || strstr(vendor_str, "Advanced Micro")) {
                info->vendor = GPU_VENDOR_AMD;
            } else if (strstr(vendor_str, "Intel")) {
                info->vendor = GPU_VENDOR_INTEL;
            } else if (strstr(vendor_str, "Apple")) {
                info->vendor = GPU_VENDOR_APPLE;
            } else {
                info->vendor = GPU_VENDOR_OTHER;
            }

            // Get memory
            cl_ulong global_mem = 0;
            clGetDeviceInfo(devices[d], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(global_mem), &global_mem, NULL);
            info->total_memory_bytes = global_mem;
            info->free_memory_bytes = (uint64_t)(global_mem * 0.9);

            // Get compute units
            cl_uint compute_units = 0;
            clGetDeviceInfo(devices[d], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, NULL);
            info->compute_units = compute_units;

            // Get max work group size
            size_t max_workgroup = 0;
            clGetDeviceInfo(devices[d], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_workgroup), &max_workgroup, NULL);
            info->max_threads_per_block = (uint32_t)max_workgroup;

            // Get clock frequency
            cl_uint clock_freq = 0;
            clGetDeviceInfo(devices[d], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(clock_freq), &clock_freq, NULL);
            info->core_clock_mhz = clock_freq;
            info->boost_clock_mhz = clock_freq;

            // Set common features
            info->supports_fp64 = true;  // Most GPUs support FP64
            info->supports_fp16 = true;  // Most modern GPUs support FP16
            info->warp_size = (info->vendor == GPU_VENDOR_AMD) ? 64 : 32;

            // Update aggregate stats
            caps->total_gpu_memory_bytes += info->total_memory_bytes;
            caps->total_compute_units += info->compute_units;

            // Rough TFLOPS estimate
            float tflops = (float)compute_units * 64.0f * 2.0f * (clock_freq / 1000.0f) / 1000.0f;
            caps->estimated_tflops += tflops;

            caps->device_count++;

            LOG_DEBUG("OpenCL device: %s, vendor=%s, CUs=%u, Mem=%lu MB",
                      dev_name, vendor_str, compute_units, (unsigned long)(global_mem / (1024*1024)));
        }
    }
}

//=============================================================================
// ROCm/HIP Detection
//=============================================================================

/**
 * @brief Detect ROCm/HIP devices via runtime library loading
 *
 * WHAT: Loads HIP library and queries AMD GPU devices
 * WHY:  Runtime detection for AMD GPUs without compile-time ROCm dependency
 * HOW:  dlopen libamdhip64.so, call hipInit, hipGetDeviceCount
 */
static void detect_rocm_devices(gpu_detect_result_t* caps)
{
    // Guard: Already have ROCm lib from previous detection
    if (s_rocm_lib != NULL) {
        return;
    }

    LOG_DEBUG("Attempting to detect ROCm/HIP devices");

    // ROCm is primarily Linux
#ifdef __linux__
    s_rocm_lib = LIB_OPEN("libamdhip64.so");
    if (!s_rocm_lib) {
        s_rocm_lib = LIB_OPEN("/opt/rocm/lib/libamdhip64.so");
    }
#elif defined(_WIN32)
    s_rocm_lib = LIB_OPEN("amdhip64.dll");
#else
    // ROCm not supported on other platforms
    LOG_DEBUG("ROCm not supported on this platform");
    return;
#endif

    // Guard: Library not found
    if (!s_rocm_lib) {
        LOG_DEBUG("ROCm/HIP library not found");
        return;
    }

    // Load function pointers
    hipInit_t hipInit = (hipInit_t)LIB_SYM(s_rocm_lib, "hipInit");
    hipGetDeviceCount_t hipGetDeviceCount = (hipGetDeviceCount_t)LIB_SYM(s_rocm_lib, "hipGetDeviceCount");
    hipGetDeviceProperties_t hipGetDeviceProperties = (hipGetDeviceProperties_t)LIB_SYM(s_rocm_lib, "hipGetDeviceProperties");
    hipDriverGetVersion_t hipDriverGetVersion = (hipDriverGetVersion_t)LIB_SYM(s_rocm_lib, "hipDriverGetVersion");

    // Guard: Required functions not found
    if (!hipInit || !hipGetDeviceCount || !hipGetDeviceProperties) {
        LOG_DEBUG("Required HIP functions not found");
        LIB_CLOSE(s_rocm_lib);
        s_rocm_lib = NULL;
        return;
    }

    // Initialize HIP
    hipError_t result = hipInit(0);
    if (result != hipSuccess) {
        LOG_DEBUG("hipInit failed with error %d", result);
        LIB_CLOSE(s_rocm_lib);
        s_rocm_lib = NULL;
        return;
    }

    // Get driver version
    if (hipDriverGetVersion) {
        int version = 0;
        if (hipDriverGetVersion(&version) == hipSuccess) {
            caps->rocm_version = version;
        }
    }

    // Get device count
    int device_count = 0;
    result = hipGetDeviceCount(&device_count);
    if (result != hipSuccess || device_count == 0) {
        LOG_DEBUG("No ROCm/HIP devices found");
        return;
    }

    caps->rocm_available = true;
    caps->rocm_device_count = (uint32_t)device_count;
    caps->available_backends |= GPU_BACKEND_ROCM;

    LOG_INFO("ROCm detected: version=%d, devices=%d", caps->rocm_version, device_count);

    // Query each device
    for (int i = 0; i < device_count && caps->device_count < 16; i++) {
        hipDeviceProp_simplified_t props;
        memset(&props, 0, sizeof(props));

        if (hipGetDeviceProperties(&props, i) != hipSuccess) {
            continue;
        }

        // Check if this device was already detected via OpenCL
        bool duplicate = false;
        for (uint32_t j = 0; j < caps->device_count; j++) {
            if (strstr(caps->devices[j].name, props.name) != NULL ||
                strstr(props.name, caps->devices[j].name) != NULL) {
                // Update existing entry to prefer ROCm backend for AMD devices
                if (caps->devices[j].vendor == GPU_VENDOR_AMD) {
                    caps->devices[j].backend = GPU_BACKEND_ROCM;
                }
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            LOG_DEBUG("Updating backend for device: %s", props.name);
            continue;
        }

        gpu_device_info_t* info = &caps->devices[caps->device_count];
        info->device_index = i;
        info->vendor = GPU_VENDOR_AMD;
        info->backend = GPU_BACKEND_ROCM;
        strncpy(info->name, props.name, sizeof(info->name) - 1);

        info->total_memory_bytes = props.totalGlobalMem;
        info->free_memory_bytes = (uint64_t)(props.totalGlobalMem * 0.9);
        info->compute_units = (uint32_t)props.multiProcessorCount;
        info->max_threads_per_block = (uint32_t)props.maxThreadsPerBlock;
        info->max_block_dim_x = (uint32_t)props.maxThreadsDim[0];
        info->max_block_dim_y = (uint32_t)props.maxThreadsDim[1];
        info->max_block_dim_z = (uint32_t)props.maxThreadsDim[2];
        info->warp_size = (uint32_t)props.warpSize;
        info->core_clock_mhz = (uint32_t)(props.clockRate / 1000);
        info->boost_clock_mhz = info->core_clock_mhz;
        info->memory_clock_mhz = (uint32_t)(props.memoryClockRate / 1000);
        info->memory_bus_width = (uint32_t)props.memoryBusWidth;
        info->compute_capability_major = (uint32_t)props.major;
        info->compute_capability_minor = (uint32_t)props.minor;
        info->supports_concurrent_kernels = (props.concurrentKernels != 0);

        // Calculate memory bandwidth
        if (props.memoryBusWidth > 0 && props.memoryClockRate > 0) {
            info->memory_bandwidth_gbps = (float)(props.memoryBusWidth / 8.0 * props.memoryClockRate * 2.0 / 1000000.0);
        }

        // AMD-specific features
        info->supports_fp16 = true;
        info->supports_fp64 = true;
        info->supports_unified_memory = false;  // AMD uses different approach
        info->supports_p2p = true;

        // Update aggregate stats
        caps->total_gpu_memory_bytes += info->total_memory_bytes;
        caps->total_compute_units += info->compute_units;

        // Estimate TFLOPS for AMD: CUs * 64 (wavefront) * 2 (FMA) * clock / 1000000
        float tflops = (float)props.multiProcessorCount * 64.0f * 2.0f * (props.clockRate / 1000.0f) / 1000000.0f;
        caps->estimated_tflops += tflops;

        caps->device_count++;

        LOG_DEBUG("ROCm device %d: %s, CUs=%d, Mem=%lu MB",
                  i, props.name, props.multiProcessorCount, (unsigned long)(props.totalGlobalMem / (1024*1024)));
    }
}

//=============================================================================
// AWS Neuron/Inferentia Detection
//=============================================================================

/**
 * @brief NRT function pointer types for runtime loading
 */
typedef int (*nrt_init_fn)(void);
typedef void (*nrt_close_fn)(void);
typedef int (*nrt_get_total_nc_count_fn)(uint32_t*);

/**
 * @brief Detect AWS Inferentia NeuronCore devices via NRT library
 *
 * WHAT: Loads Neuron Runtime library and queries NeuronCore count
 * WHY:  Runtime detection for Inferentia hardware without compile-time dependency
 * HOW:  dlopen libnrt.so, call nrt_init, nrt_get_total_nc_count
 */
static void detect_neuron_devices(gpu_detect_result_t* caps)
{
    // Guard: Already have NRT lib from previous detection
    if (s_neuron_lib != NULL) {
        return;
    }

    LOG_DEBUG("Attempting to detect AWS Neuron/Inferentia devices");

    // NRT is Linux-only (Inferentia runs on Amazon Linux / Ubuntu)
#ifdef __linux__
    s_neuron_lib = LIB_OPEN("libnrt.so");
    if (!s_neuron_lib) {
        s_neuron_lib = LIB_OPEN("libnrt.so.1");
    }
    if (!s_neuron_lib) {
        s_neuron_lib = LIB_OPEN("/opt/aws/neuron/lib/libnrt.so");
    }
    if (!s_neuron_lib) {
        s_neuron_lib = LIB_OPEN("/opt/aws/neuron/lib/libnrt.so.1");
    }
#else
    LOG_DEBUG("Neuron/Inferentia not supported on this platform");
    return;
#endif

    // Guard: Library not found
    if (!s_neuron_lib) {
        LOG_DEBUG("Neuron Runtime (NRT) library not found");
        return;
    }

    // Load function pointers
    nrt_init_fn nrt_init = (nrt_init_fn)LIB_SYM(s_neuron_lib, "nrt_init");
    nrt_close_fn nrt_close = (nrt_close_fn)LIB_SYM(s_neuron_lib, "nrt_close");
    nrt_get_total_nc_count_fn nrt_get_total_nc_count =
        (nrt_get_total_nc_count_fn)LIB_SYM(s_neuron_lib, "nrt_get_total_nc_count");

    // Guard: Required functions not found
    if (!nrt_init || !nrt_get_total_nc_count) {
        LOG_DEBUG("Required NRT functions not found");
        LIB_CLOSE(s_neuron_lib);
        s_neuron_lib = NULL;
        return;
    }

    // Initialize NRT
    int result = nrt_init();
    if (result != 0) {
        LOG_DEBUG("nrt_init failed with error %d", result);
        LIB_CLOSE(s_neuron_lib);
        s_neuron_lib = NULL;
        return;
    }

    // Get NeuronCore count
    uint32_t nc_count = 0;
    result = nrt_get_total_nc_count(&nc_count);

    // Clean up NRT (we only needed it for detection)
    if (nrt_close) {
        nrt_close();
    }

    if (result != 0 || nc_count == 0) {
        LOG_DEBUG("No NeuronCores found");
        return;
    }

    caps->neuron_available = true;
    caps->available_backends |= GPU_BACKEND_NEURON;

    // inf2.xlarge = 2 NeuronCores per device, inf2.8xlarge = 2 devices × 2 NeuronCores
    // Estimate device count from core count (2 NeuronCores per Inferentia2 device)
    caps->neuron_cores_per_device = 2;
    caps->neuron_device_count = (nc_count + 1) / caps->neuron_cores_per_device;
    if (caps->neuron_device_count == 0) caps->neuron_device_count = 1;

    LOG_INFO("Neuron detected: %u NeuronCores across %u device(s)",
             nc_count, caps->neuron_device_count);
}

//=============================================================================
// Best Device Selection
//=============================================================================

/**
 * @brief Determine best device based on capabilities
 *
 * WHAT: Ranks devices and selects the most capable one
 * WHY:  Simplifies device selection for workloads
 * HOW:  Score based on compute units, memory, and features
 */
static void determine_best_device(gpu_detect_result_t* caps)
{
    caps->best_device_index = -1;
    caps->best_backend = GPU_BACKEND_NONE;
    caps->recommended_backend = GPU_BACKEND_NONE;

    // Guard: No devices
    if (caps->device_count == 0) {
        return;
    }

    float best_score = 0.0f;

    for (uint32_t i = 0; i < caps->device_count; i++) {
        gpu_device_info_t* dev = &caps->devices[i];

        // Calculate device score
        // Weight: memory (40%), compute units (30%), features (20%), backend preference (10%)
        float mem_score = (float)dev->total_memory_bytes / (16.0f * 1024 * 1024 * 1024);  // Normalize to 16GB
        if (mem_score > 1.0f) mem_score = 1.0f;

        float cu_score = (float)dev->compute_units / 100.0f;  // Normalize to 100 CUs
        if (cu_score > 1.0f) cu_score = 1.0f;

        float feature_score = 0.0f;
        if (dev->supports_tensor_cores) feature_score += 0.3f;
        if (dev->supports_fp16) feature_score += 0.2f;
        if (dev->supports_unified_memory) feature_score += 0.2f;
        if (dev->supports_concurrent_kernels) feature_score += 0.15f;
        if (dev->supports_p2p) feature_score += 0.15f;

        // Backend preference: CUDA > ROCm > OpenCL (for stability/performance)
        float backend_score = 0.0f;
        switch (dev->backend) {
            case GPU_BACKEND_CUDA:   backend_score = 1.0f; break;
            case GPU_BACKEND_ROCM:   backend_score = 0.9f; break;
            case GPU_BACKEND_OPENCL: backend_score = 0.7f; break;
            default: break;
        }

        float total_score = mem_score * 0.4f + cu_score * 0.3f + feature_score * 0.2f + backend_score * 0.1f;

        if (total_score > best_score) {
            best_score = total_score;
            caps->best_device_index = (int)i;
            caps->best_backend = dev->backend;
        }
    }

    // Set recommended backend based on best device
    caps->recommended_backend = caps->best_backend;

    LOG_INFO("Best GPU: index=%d, backend=%s, score=%.3f",
             caps->best_device_index, gpu_backend_name(caps->best_backend), best_score);
}

//=============================================================================
// Internal Initialization (called via pthread_once)
//=============================================================================

/**
 * @brief Thread-safe one-time initialization of GPU detection cache
 *
 * WHY:  pthread_once guarantees this runs exactly once, even with concurrent calls
 * HOW:  Called by pthread_once in gpu_detect_capabilities
 */
static void gpu_detect_init_impl(void)
{
    // Initialize structure
    memset(&s_cached_caps, 0, sizeof(gpu_detect_result_t));
    s_cached_caps.best_device_index = -1;

    LOG_INFO("Starting GPU capability detection");

    // Detect each backend
    // Order: CUDA first (most common for ML), then ROCm (AMD), then OpenCL (fallback), then Neuron
    detect_cuda_devices(&s_cached_caps);
    detect_rocm_devices(&s_cached_caps);
    detect_opencl_devices(&s_cached_caps);
    detect_neuron_devices(&s_cached_caps);

    // Determine best device
    determine_best_device(&s_cached_caps);

    LOG_INFO("GPU detection complete: backends=0x%x, devices=%u, total_mem=%lu GB, est_tflops=%.1f",
             s_cached_caps.available_backends,
             s_cached_caps.device_count,
             (unsigned long)(s_cached_caps.total_gpu_memory_bytes / (1024*1024*1024)),
             s_cached_caps.estimated_tflops);
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool gpu_detect_capabilities(gpu_detect_result_t* caps)
{
    if (!caps) {
        LOG_ERROR("NULL capabilities pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gpu_detect_capabilities: caps is NULL");
        return false;
    }

    // Thread-safe one-time initialization using pthread_once
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);

    // Copy cached result to caller's buffer
    memcpy(caps, &s_cached_caps, sizeof(gpu_detect_result_t));
    return true;
}

bool gpu_is_available(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);
    return s_cached_caps.device_count > 0;
}

bool gpu_backend_is_available(gpu_backend_t backend)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);
    return (s_cached_caps.available_backends & backend) != 0;
}

uint32_t gpu_get_device_count(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);
    return s_cached_caps.device_count;
}

bool gpu_get_device_info(uint32_t device_index, gpu_device_info_t* info)
{
    if (!info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "gpu_get_device_info: info is NULL");

            return false;
    }

    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);

    if (device_index >= s_cached_caps.device_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gpu_get_device_info: capacity exceeded");
        return false;
    }

    memcpy(info, &s_cached_caps.devices[device_index], sizeof(gpu_device_info_t));
    return true;
}

int gpu_get_best_device(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);
    return s_cached_caps.best_device_index;
}

gpu_backend_t gpu_get_recommended_backend(void)
{
    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);
    return s_cached_caps.recommended_backend;
}

const char* gpu_backend_name(gpu_backend_t backend)
{
    switch (backend) {
        case GPU_BACKEND_NONE:    return "None";
        case GPU_BACKEND_CUDA:    return "CUDA";
        case GPU_BACKEND_OPENCL:  return "OpenCL";
        case GPU_BACKEND_ROCM:    return "ROCm";
        case GPU_BACKEND_METAL:   return "Metal";
        case GPU_BACKEND_VULKAN:  return "Vulkan";
        case GPU_BACKEND_NEURON:  return "Neuron";
        default:                  return "Unknown";
    }
}

const char* gpu_vendor_name(gpu_vendor_t vendor)
{
    switch (vendor) {
        case GPU_VENDOR_UNKNOWN:  return "Unknown";
        case GPU_VENDOR_NVIDIA:   return "NVIDIA";
        case GPU_VENDOR_AMD:      return "AMD";
        case GPU_VENDOR_INTEL:    return "Intel";
        case GPU_VENDOR_APPLE:    return "Apple";
        case GPU_VENDOR_AWS:      return "AWS";
        case GPU_VENDOR_OTHER:    return "Other";
        default:                  return "Unknown";
    }
}

size_t gpu_capabilities_string(char* buffer, size_t size)
{
    if (!buffer || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "gpu_capabilities_string: invalid parameters");

            return 0;
    }

    // Ensure cache is initialized (thread-safe)
    pthread_once(&s_cache_init_once, gpu_detect_init_impl);

    if (s_cached_caps.device_count == 0) {
        return (size_t)snprintf(buffer, size, "No GPU detected");
    }

    size_t offset = 0;

    // List backends
    if (s_cached_caps.cuda_available) {
        int written = snprintf(buffer + offset, size - offset, "CUDA: %u device(s)", s_cached_caps.cuda_device_count);
        if (written > 0) offset += (size_t)written;
    }

    if (s_cached_caps.rocm_available) {
        int written = snprintf(buffer + offset, size - offset, "%sROCm: %u device(s)",
                               offset > 0 ? ", " : "", s_cached_caps.rocm_device_count);
        if (written > 0) offset += (size_t)written;
    }

    if (s_cached_caps.opencl_available && s_cached_caps.opencl_device_count > 0) {
        int written = snprintf(buffer + offset, size - offset, "%sOpenCL: %u device(s)",
                               offset > 0 ? ", " : "", s_cached_caps.opencl_device_count);
        if (written > 0) offset += (size_t)written;
    }

    if (s_cached_caps.neuron_available) {
        int written = snprintf(buffer + offset, size - offset, "%sNeuron: %u device(s), %u cores/device",
                               offset > 0 ? ", " : "",
                               s_cached_caps.neuron_device_count,
                               s_cached_caps.neuron_cores_per_device);
        if (written > 0) offset += (size_t)written;
    }

    // Add summary
    int written = snprintf(buffer + offset, size - offset, " | Total: %lu GB, %.1f TFLOPS",
                           (unsigned long)(s_cached_caps.total_gpu_memory_bytes / (1024*1024*1024)),
                           s_cached_caps.estimated_tflops);
    if (written > 0) offset += (size_t)written;

    return offset;
}

bool gpu_refresh_capabilities(void)
{
    nimcp_mutex_lock(&s_refresh_mutex);

    // Close existing library handles
    if (s_cuda_lib) {
        LIB_CLOSE(s_cuda_lib);
        s_cuda_lib = NULL;
    }
    if (s_opencl_lib) {
        LIB_CLOSE(s_opencl_lib);
        s_opencl_lib = NULL;
    }
    if (s_rocm_lib) {
        LIB_CLOSE(s_rocm_lib);
        s_rocm_lib = NULL;
    }
    if (s_neuron_lib) {
        LIB_CLOSE(s_neuron_lib);
        s_neuron_lib = NULL;
    }

    // Re-run detection
    memset(&s_cached_caps, 0, sizeof(gpu_detect_result_t));
    s_cached_caps.best_device_index = -1;

    detect_cuda_devices(&s_cached_caps);
    detect_rocm_devices(&s_cached_caps);
    detect_opencl_devices(&s_cached_caps);
    detect_neuron_devices(&s_cached_caps);
    determine_best_device(&s_cached_caps);

    nimcp_mutex_unlock(&s_refresh_mutex);

    LOG_INFO("GPU capabilities refreshed: devices=%u", s_cached_caps.device_count);
    return true;
}
