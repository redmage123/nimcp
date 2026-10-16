/**
 * @file nimcp_execution_mode.c
 * @brief Implementation of execution mode detection and selection
 *
 * NIMCP Phase: Integrated bio-async communication and comprehensive logging
 * Date: 2025-11-28
 *
 * WHAT: Detects hardware capabilities and selects optimal execution mode
 * WHY:  Different platforms have different optimal execution strategies
 * HOW:  Query CPU, GPU, network at runtime; select best mode for workload
 *
 * BIO-ASYNC INTEGRATION:
 * - GPU execution events published to bio-async system
 * - Mode switches communicated via async channels
 * - Performance metrics shared with monitoring subsystems
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P)
 */

#define LOG_MODULE "GPU_EXEC_MODE"
#define LOG_MODULE_ID 0x0900
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_dimension_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(execution_mode)

#include "gpu/nimcp_execution_mode.h"
#include "gpu/execution/nimcp_gpu_detect.h"
#include "gpu/execution/nimcp_simd_detect.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Platform-specific includes
#ifdef __linux__
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

// CUDA includes (conditional)
#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Execution context internal structure
 */
struct execution_context_struct {
    execution_config_t config;
    execution_mode_t active_mode;

    // CPU resources
    uint32_t cpu_thread_count;
    nimcp_thread_pool_t* thread_pool;

    // GPU resources
    void* gpu_device_ptr;
    int gpu_device_id;

    // Statistics
    uint64_t total_operations;
    double total_time_ms;
    uint64_t start_time;

    // Bio-async integration
    bio_module_context_t bio_ctx;

    // Initialization flags
    bool thread_pool_initialized;
    bool gpu_initialized;
};

//=============================================================================
// CPU Detection (Cross-Platform)
//=============================================================================

/**
 * @brief Detect CPU capabilities with SIMD detection
 *
 * WHAT: Query CPU for cores, threads, and SIMD capabilities
 * WHY:  Need to know CPU resources for execution mode selection
 * HOW:  Platform APIs for core count + simd_detect module for SIMD
 */
static bool detect_cpu_capabilities(hardware_capabilities_t* caps)
{
    if (!caps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_cpu_capabilities: caps is NULL");
        return false;
    }

    caps->cpu_available = true;

    #ifdef __linux__
    // Linux: Use sysconf for CPU core/thread count
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    caps->cpu_cores = (nprocs > 0) ? (uint32_t)nprocs : 1;
    caps->cpu_threads = caps->cpu_cores;

    // Check for hyperthreading by comparing online vs conf
    long nprocs_conf = sysconf(_SC_NPROCESSORS_CONF);
    if (nprocs_conf > nprocs && nprocs_conf <= (long)caps->cpu_cores * 2) {
        caps->cpu_threads = (uint32_t)nprocs_conf;
    }

    #elif defined(_WIN32)
    // Windows: Use GetSystemInfo for processor count
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    caps->cpu_cores = sysinfo.dwNumberOfProcessors;
    caps->cpu_threads = caps->cpu_cores;

    #else
    // Fallback: Assume 4 cores
    caps->cpu_cores = 4;
    caps->cpu_threads = 4;
    #endif

    // Use simd_detect module for accurate SIMD capability detection
    simd_capabilities_t simd_caps;
    if (simd_detect_capabilities(&simd_caps)) {
        caps->cpu_avx2 = simd_caps.has_avx2;
        caps->cpu_avx512 = simd_caps.has_avx512f;
        LOG_DEBUG("CPU SIMD detection: AVX2=%s, AVX512=%s, vector_width=%u bits",
                  simd_caps.has_avx2 ? "yes" : "no",
                  simd_caps.has_avx512f ? "yes" : "no",
                  simd_caps.max_vector_width);
    } else {
        // Fallback to conservative assumptions if simd_detect fails
        caps->cpu_avx2 = false;
        caps->cpu_avx512 = false;
    }

    LOG_DEBUG("CPU detection: cores=%u, threads=%u", caps->cpu_cores, caps->cpu_threads);
    return true;
}

//=============================================================================
// GPU Detection (via runtime GPU detection module)
//=============================================================================

/**
 * @brief Detect GPU capabilities using runtime detection
 *
 * WHAT: Detects CUDA, OpenCL, and ROCm at runtime
 * WHY:  Works without compile-time dependencies
 * HOW:  Uses gpu_detect module for runtime library loading
 */
static bool detect_gpu_capabilities_runtime(hardware_capabilities_t* caps)
{
    if (!caps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_gpu_capabilities_runtime: caps is NULL");
        return false;
    }

    // Use runtime GPU detection
    gpu_detect_result_t gpu_caps;
    if (!gpu_detect_capabilities(&gpu_caps)) {
        LOG_DEBUG("Runtime GPU detection failed");
        caps->cuda_available = false;
        caps->rocm_available = false;
        caps->opencl_available = false;
        caps->gpu_count = 0;
        caps->gpu_compute_units = 0;
        caps->gpu_memory_mb = 0;
        caps->gpu_compute_capability = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_gpu_capabilities_runtime: gpu_detect_capabilities is NULL");
        return false;
    }

    // Map GPU detection results to hardware capabilities
    caps->cuda_available = gpu_caps.cuda_available;
    caps->rocm_available = gpu_caps.rocm_available;
    caps->opencl_available = gpu_caps.opencl_available;
    caps->gpu_count = gpu_caps.device_count;

    // Get details from best device if available
    if (gpu_caps.best_device_index >= 0) {
        gpu_device_info_t* best = &gpu_caps.devices[gpu_caps.best_device_index];
        caps->gpu_compute_units = best->compute_units;
        caps->gpu_memory_mb = (uint64_t)(best->total_memory_bytes / (1024 * 1024));
        caps->gpu_compute_capability = best->compute_capability_major * 10 + best->compute_capability_minor;
    } else if (gpu_caps.device_count > 0) {
        // Use first device as fallback
        caps->gpu_compute_units = gpu_caps.devices[0].compute_units;
        caps->gpu_memory_mb = (uint64_t)(gpu_caps.devices[0].total_memory_bytes / (1024 * 1024));
        caps->gpu_compute_capability = gpu_caps.devices[0].compute_capability_major * 10 +
                                       gpu_caps.devices[0].compute_capability_minor;
    } else {
        caps->gpu_compute_units = 0;
        caps->gpu_memory_mb = 0;
        caps->gpu_compute_capability = 0;
    }

    LOG_DEBUG("GPU detection: cuda=%d, rocm=%d, opencl=%d, devices=%u, memory=%lu MB",
              caps->cuda_available, caps->rocm_available, caps->opencl_available,
              caps->gpu_count, (unsigned long)caps->gpu_memory_mb);

    return (gpu_caps.device_count > 0);
}

//=============================================================================
// Network Detection
//=============================================================================

/**
 * @brief Detect network capabilities for distributed computing
 *
 * WHAT: Detect MPI and network availability for distributed execution
 * WHY:  Enable distributed GPU/CPU modes across cluster nodes
 * HOW:  Environment variable checks for MPI job detection
 */
static bool detect_network_capabilities(hardware_capabilities_t* caps)
{
    if (!caps) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_network_capabilities: caps is NULL");
        return false;
    }

    // Default: No distributed capabilities
    caps->network_available = false;
    caps->network_nodes = 1;
    caps->network_bandwidth_mbps = 0;

    // Check for MPI environment variables
    const char* env_world_size = NULL;
    uint32_t mpi_world_size = 0;

    // Check OpenMPI environment
    // P1-2 fix: Use strtol instead of atoi for safe conversion
    env_world_size = getenv("OMPI_COMM_WORLD_SIZE");
    if (env_world_size) {
        char* endptr;
        long size = strtol(env_world_size, &endptr, 10);
        if (endptr != env_world_size && size > 0 && size <= UINT32_MAX) {
            mpi_world_size = (uint32_t)size;
            LOG_DEBUG("Detected OpenMPI environment: world_size=%u", mpi_world_size);
        }
    }

    // Check MPICH/Intel MPI environment
    if (mpi_world_size == 0) {
        env_world_size = getenv("PMI_SIZE");
        if (env_world_size) {
            char* endptr;
            long size = strtol(env_world_size, &endptr, 10);
            if (endptr != env_world_size && size > 0 && size <= UINT32_MAX) {
                mpi_world_size = (uint32_t)size;
                LOG_DEBUG("Detected MPICH/PMI environment: world_size=%u", mpi_world_size);
            }
        }
    }

    // Check SLURM environment
    if (mpi_world_size == 0) {
        env_world_size = getenv("SLURM_NTASKS");
        if (env_world_size) {
            char* endptr;
            long size = strtol(env_world_size, &endptr, 10);
            if (endptr != env_world_size && size > 0 && size <= UINT32_MAX) {
                mpi_world_size = (uint32_t)size;
                LOG_DEBUG("Detected SLURM environment: ntasks=%u", mpi_world_size);
            }
        }
    }

    if (mpi_world_size > 1) {
        caps->network_available = true;
        caps->network_nodes = mpi_world_size;

        // Check for InfiniBand on Linux
        #ifdef __linux__
        if (access("/sys/class/infiniband", F_OK) == 0) {
            caps->network_bandwidth_mbps = 100000;  // Assume EDR InfiniBand
            LOG_DEBUG("InfiniBand detected, estimated bandwidth: 100 Gbps");
        } else {
            caps->network_bandwidth_mbps = 10000;  // Default to 10GbE
        }
        #else
        caps->network_bandwidth_mbps = 10000;
        #endif

        LOG_INFO("Network capabilities detected: nodes=%u, bandwidth=%u Mbps",
                 caps->network_nodes, caps->network_bandwidth_mbps);
    } else {
        LOG_DEBUG("No MPI environment detected, distributed mode unavailable");
    }

    return true;
}

//=============================================================================
// Public API: Hardware Detection
//=============================================================================

/**
 * @brief Detect all hardware capabilities
 */
bool execution_detect_capabilities(hardware_capabilities_t* caps)
{
    // Guard: Validate output
    if (!caps) {
        LOG_ERROR("NULL capabilities pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_detect_capabilities: caps is NULL");
        return false;
    }

    LOG_DEBUG("Detecting hardware capabilities");

    // Initialize all fields to zero/false
    memset(caps, 0, sizeof(hardware_capabilities_t));

    // Detect each capability
    detect_cpu_capabilities(caps);
    detect_gpu_capabilities_runtime(caps);
    detect_network_capabilities(caps);

    // Detect SIMD capabilities
    simd_capabilities_t simd_caps;
    if (simd_detect_capabilities(&simd_caps)) {
        caps->cpu_avx2 = simd_caps.has_avx2;
        caps->cpu_avx512 = simd_caps.has_avx512f;
        LOG_DEBUG("SIMD detected: AVX2=%d, AVX512=%d, width=%u",
                  simd_caps.has_avx2, simd_caps.has_avx512f, simd_caps.max_vector_width);
    }

    // Determine recommended mode based on available backends
    if (caps->cuda_available && caps->gpu_count > 0) {
        caps->recommended_mode = EXEC_MODE_GPU_CUDA;
        LOG_INFO("Detected CUDA-capable GPU (count=%u), recommending GPU mode", caps->gpu_count);
    } else if (caps->rocm_available && caps->gpu_count > 0) {
        caps->recommended_mode = EXEC_MODE_GPU_ROCM;
        LOG_INFO("Detected ROCm-capable GPU (count=%u), recommending ROCm mode", caps->gpu_count);
    } else if (caps->opencl_available && caps->gpu_count > 0) {
        caps->recommended_mode = EXEC_MODE_GPU_OPENCL;
        LOG_INFO("Detected OpenCL-capable GPU (count=%u), recommending OpenCL mode", caps->gpu_count);
    } else if (caps->cpu_threads >= 4) {
        caps->recommended_mode = EXEC_MODE_CPU_PARALLEL;
        LOG_INFO("Detected multi-core CPU (threads=%u), recommending parallel mode", caps->cpu_threads);
    } else {
        caps->recommended_mode = EXEC_MODE_CPU_SEQUENTIAL;
        LOG_INFO("Limited CPU resources, recommending sequential mode");
    }

    return true;
}

/**
 * @brief Check if mode is supported
 */
bool execution_mode_is_supported(execution_mode_t mode)
{
    hardware_capabilities_t caps;
    if (!execution_detect_capabilities(&caps)) {
        return false;
    }

    switch (mode) {
        case EXEC_MODE_CPU_SEQUENTIAL:
        case EXEC_MODE_CPU_PARALLEL:
            return caps.cpu_available;

        case EXEC_MODE_GPU_CUDA:
            return caps.cuda_available;

        case EXEC_MODE_GPU_ROCM:
            return caps.rocm_available;

        case EXEC_MODE_GPU_OPENCL:
            return caps.opencl_available;

        case EXEC_MODE_DISTRIBUTED_CPU:
        case EXEC_MODE_DISTRIBUTED_GPU:
            return caps.network_available;

        case EXEC_MODE_HYBRID:
            return caps.cpu_available && caps.cuda_available;

        case EXEC_MODE_AUTO:
            return true;  // Auto always supported

        default:
            return false;
    }
}

/**
 * @brief Get recommended mode for workload
 *
 * WHAT: Returns the recommended execution mode for a given network size
 * WHY:  Phase 1 GPU Integration - GPU is now always preferred when available
 * HOW:  GPU-first policy: always try GPU regardless of network size
 *
 * GPU-FIRST POLICY (Phase 1 GPU Integration):
 * - Always return GPU mode if ANY GPU is available
 * - Network size no longer affects GPU vs CPU decision
 * - Only fall back to CPU if no GPU is available
 *
 * PRIORITY ORDER:
 * 1. Distributed GPU (if multi-node + GPU available)
 * 2. CUDA (NVIDIA GPUs)
 * 3. ROCm (AMD GPUs)
 * 4. OpenCL (cross-platform)
 * 5. CPU Parallel (if multi-core)
 * 6. CPU Sequential (fallback)
 *
 * BACKWARD COMPATIBILITY:
 * - Network size parameters are still accepted but no longer
 *   determine GPU vs CPU choice
 * - Very large networks still prefer distributed mode when available
 */
execution_mode_t execution_get_recommended_mode(uint32_t num_neurons, uint32_t num_synapses)
{
    hardware_capabilities_t caps;
    if (!execution_detect_capabilities(&caps)) {
        LOG_WARN("Hardware detection failed, defaulting to CPU sequential");
        return EXEC_MODE_CPU_SEQUENTIAL;
    }

    (void)num_synapses;  // Not used yet

    // GPU-FIRST POLICY: Always prefer GPU when available
    // Check for distributed GPU mode for very large networks
    if (num_neurons >= 1000000 && caps.network_available) {
        if (caps.cuda_available) {
            LOG_DEBUG("Recommending DISTRIBUTED_GPU mode for %u neurons (multi-node + CUDA)", num_neurons);
            return EXEC_MODE_DISTRIBUTED_GPU;
        }
        // Could add ROCm distributed support here in future
    }

    // GPU-first: Always use GPU if available, regardless of network size
    if (caps.cuda_available) {
        LOG_DEBUG("Recommending GPU_CUDA mode for %u neurons (GPU-first policy)", num_neurons);
        return EXEC_MODE_GPU_CUDA;
    }

    if (caps.rocm_available) {
        LOG_DEBUG("Recommending GPU_ROCM mode for %u neurons (GPU-first policy)", num_neurons);
        return EXEC_MODE_GPU_ROCM;
    }

    if (caps.opencl_available) {
        LOG_DEBUG("Recommending GPU_OPENCL mode for %u neurons (GPU-first policy)", num_neurons);
        return EXEC_MODE_GPU_OPENCL;
    }

    // No GPU available - fall back to CPU
    LOG_DEBUG("No GPU available, falling back to CPU for %u neurons", num_neurons);

    // Use parallel CPU if multi-core
    if (caps.cpu_threads >= 4) {
        return EXEC_MODE_CPU_PARALLEL;
    }

    return EXEC_MODE_CPU_SEQUENTIAL;
}

//=============================================================================
// Execution Context Management
//=============================================================================

/**
 * @brief Create execution context
 */
execution_context_t execution_context_create(const execution_config_t* config)
{
    // Guard: Validate config
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "config is NULL in execution_context_create");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    // Allocate context
    execution_context_t ctx = (execution_context_t)nimcp_calloc(1, sizeof(struct execution_context_struct));
    if (!ctx) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate execution context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    // Copy configuration
    ctx->config = *config;

    // Determine actual mode (handle AUTO and fallback)
    execution_mode_t requested_mode = config->mode;

    if (requested_mode == EXEC_MODE_AUTO) {
        requested_mode = execution_get_recommended_mode(1000, 100);  // Default heuristic
    }

    // Check if requested mode is supported
    if (!execution_mode_is_supported(requested_mode)) {
        // Fallback to CPU parallel or sequential
        if (config->auto_fallback) {
            requested_mode = EXEC_MODE_CPU_PARALLEL;
            if (!execution_mode_is_supported(requested_mode)) {
                requested_mode = EXEC_MODE_CPU_SEQUENTIAL;
            }
        } else {
            nimcp_free(ctx);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_context_create: execution_mode_is_supported is NULL");
            return NULL;
        }
    }

    ctx->active_mode = requested_mode;
    ctx->thread_pool_initialized = false;
    ctx->gpu_initialized = false;

    // Initialize mode-specific resources
    switch (ctx->active_mode) {
        case EXEC_MODE_GPU_CUDA:
            #ifdef NIMCP_ENABLE_CUDA
            ctx->gpu_device_id = 0;
            if (cudaSetDevice(ctx->gpu_device_id) == cudaSuccess) {
                ctx->gpu_initialized = true;
                LOG_INFO("CUDA execution context initialized (device=%d)", ctx->gpu_device_id);
            } else {
                LOG_ERROR("CUDA device selection failed");
                if (config->auto_fallback) {
                    ctx->active_mode = EXEC_MODE_CPU_PARALLEL;
                    // Fall through to CPU_PARALLEL initialization
                } else {
                    nimcp_free(ctx);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_context_create: validation failed");
                    return NULL;
                }
            }
            if (ctx->active_mode != EXEC_MODE_CPU_PARALLEL) break;
            #else
            if (config->auto_fallback) {
                ctx->active_mode = EXEC_MODE_CPU_PARALLEL;
            } else {
                nimcp_free(ctx);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_context_create: validation failed");
                return NULL;
            }
            #endif
            // Fall through if fallback to CPU_PARALLEL

        case EXEC_MODE_CPU_PARALLEL:
            {
                // Initialize thread pool
                uint32_t num_threads = config->cpu_threads;
                if (num_threads == 0) {
                    hardware_capabilities_t hw_caps;
                    if (execution_detect_capabilities(&hw_caps)) {
                        num_threads = hw_caps.cpu_threads;
                    } else {
                        num_threads = 4;
                    }
                }
                if (num_threads > NIMCP_POOL_MAX_THREADS) {
                    num_threads = NIMCP_POOL_MAX_THREADS;
                }

                ctx->thread_pool = nimcp_pool_create(num_threads);
                if (!ctx->thread_pool) {
                    LOG_ERROR("Failed to create thread pool with %u threads", num_threads);
                    if (config->auto_fallback) {
                        ctx->active_mode = EXEC_MODE_CPU_SEQUENTIAL;
                        ctx->cpu_thread_count = 1;
                    } else {
                        nimcp_free(ctx);
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_context_create: validation failed");
                        return NULL;
                    }
                } else {
                    ctx->cpu_thread_count = num_threads;
                    ctx->thread_pool_initialized = true;
                    LOG_INFO("Thread pool initialized with %u threads", num_threads);
                }
            }
            break;

        case EXEC_MODE_HYBRID:
            #ifdef NIMCP_ENABLE_CUDA
            ctx->gpu_device_id = 0;
            if (cudaSetDevice(ctx->gpu_device_id) == cudaSuccess) {
                ctx->gpu_initialized = true;
            }
            #endif
            {
                uint32_t num_threads = config->cpu_threads > 0 ? config->cpu_threads : 4;
                ctx->thread_pool = nimcp_pool_create(num_threads);
                if (ctx->thread_pool) {
                    ctx->cpu_thread_count = num_threads;
                    ctx->thread_pool_initialized = true;
                }
            }
            LOG_INFO("Hybrid execution context initialized");
            break;

        default:
            ctx->cpu_thread_count = 1;
            break;
    }

    return ctx;
}

/**
 * @brief Destroy execution context and release all resources
 *
 * WHAT: Clean up execution context and all associated resources
 * WHY:  Prevent resource leaks (threads, GPU memory)
 * HOW:  Synchronize pending work, cleanup resources, free memory
 */
void execution_context_destroy(execution_context_t ctx)
{
    if (!ctx) {
        return;
    }

    LOG_DEBUG("Destroying execution context (mode=%d)", ctx->active_mode);

    // Synchronize before cleanup
    execution_synchronize(ctx);

    // Cleanup GPU resources if initialized
    #ifdef NIMCP_ENABLE_CUDA
    if (ctx->gpu_initialized) {
        cudaDeviceSynchronize();
        ctx->gpu_initialized = false;
    }
    #endif

    // Cleanup thread pool if initialized
    if (ctx->thread_pool_initialized && ctx->thread_pool) {
        LOG_DEBUG("Destroying thread pool (%u threads)", ctx->cpu_thread_count);
        nimcp_pool_destroy(ctx->thread_pool);
        ctx->thread_pool = NULL;
        ctx->thread_pool_initialized = false;
    }

    // Log final statistics
    if (ctx->total_operations > 0) {
        double avg_time = ctx->total_time_ms / (double)ctx->total_operations;
        LOG_INFO("Execution context stats: ops=%lu, total_time=%.2fms, avg=%.3fms/op",
                 (unsigned long)ctx->total_operations, ctx->total_time_ms, avg_time);
    }

    nimcp_free(ctx);
}

/**
 * @brief Get active mode
 */
execution_mode_t execution_context_get_mode(execution_context_t ctx)
{
    if (!ctx) {
        return EXEC_MODE_CPU_SEQUENTIAL;
    }

    return ctx->active_mode;
}

/**
 * @brief Cleanup resources for a specific execution mode
 *
 * WHAT: Release resources associated with an execution mode
 * WHY:  Prevent resource leaks when switching modes
 * HOW:  Mode-specific cleanup (GPU sync, thread pool shutdown)
 */
static void cleanup_mode_resources(execution_context_t ctx, execution_mode_t mode)
{
    if (!ctx) {
        return;
    }

    LOG_DEBUG("Cleaning up resources for mode %d", mode);

    switch (mode) {
        case EXEC_MODE_GPU_CUDA:
        case EXEC_MODE_GPU_ROCM:
        case EXEC_MODE_GPU_OPENCL:
            #ifdef NIMCP_ENABLE_CUDA
            if (ctx->gpu_initialized) {
                // Synchronize GPU before cleanup
                cudaDeviceSynchronize();
                ctx->gpu_initialized = false;
                LOG_DEBUG("GPU resources cleaned up");
            }
            #endif
            break;

        case EXEC_MODE_CPU_PARALLEL:
        case EXEC_MODE_HYBRID:
            if (ctx->thread_pool_initialized && ctx->thread_pool) {
                LOG_DEBUG("Destroying thread pool (%u threads)", ctx->cpu_thread_count);
                nimcp_pool_destroy(ctx->thread_pool);
                ctx->thread_pool = NULL;
                ctx->thread_pool_initialized = false;
                ctx->cpu_thread_count = 0;
            }
            // For HYBRID mode, also cleanup GPU
            if (mode == EXEC_MODE_HYBRID) {
                #ifdef NIMCP_ENABLE_CUDA
                if (ctx->gpu_initialized) {
                    cudaDeviceSynchronize();
                    ctx->gpu_initialized = false;
                }
                #endif
            }
            break;

        case EXEC_MODE_CPU_SEQUENTIAL:
        case EXEC_MODE_AUTO:
        case EXEC_MODE_DISTRIBUTED_CPU:
        case EXEC_MODE_DISTRIBUTED_GPU:
        default:
            // No specific resources to clean up
            break;
    }
}

/**
 * @brief Initialize resources for a specific execution mode
 *
 * WHAT: Allocate and initialize resources for an execution mode
 * WHY:  Each mode requires specific resources (GPU context, thread pool)
 * HOW:  Mode-specific initialization with fallback on failure
 *
 * @return true on success, false if initialization failed
 */
static bool initialize_mode_resources(execution_context_t ctx, execution_mode_t mode)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_mode_resources: ctx is NULL");
        return false;
    }

    LOG_DEBUG("Initializing resources for mode %d", mode);

    switch (mode) {
        case EXEC_MODE_GPU_CUDA:
            #ifdef NIMCP_ENABLE_CUDA
            {
                ctx->gpu_device_id = 0;  // Default to first device
                cudaError_t err = cudaSetDevice(ctx->gpu_device_id);
                if (err == cudaSuccess) {
                    ctx->gpu_initialized = true;
                    LOG_INFO("CUDA device %d initialized for mode switch", ctx->gpu_device_id);
                    return true;
                } else {
                    LOG_ERROR("Failed to set CUDA device: %s", cudaGetErrorString(err));
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_mode_resources: validation failed");
                    return false;
                }
            }
            #else
            LOG_ERROR("CUDA not available at compile time");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_mode_resources: operation failed");
            return false;
            #endif

        case EXEC_MODE_CPU_PARALLEL:
            {
                uint32_t num_threads = ctx->config.cpu_threads;
                if (num_threads == 0) {
                    // Auto-detect thread count
                    hardware_capabilities_t hw_caps;
                    if (execution_detect_capabilities(&hw_caps)) {
                        num_threads = hw_caps.cpu_threads;
                    } else {
                        num_threads = 4;  // Reasonable default
                    }
                }
                if (num_threads > NIMCP_POOL_MAX_THREADS) {
                    num_threads = NIMCP_POOL_MAX_THREADS;
                }

                ctx->thread_pool = nimcp_pool_create(num_threads);
                if (!ctx->thread_pool) {
                    LOG_ERROR("Failed to create thread pool with %u threads", num_threads);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_mode_resources: ctx->thread_pool is NULL");
                    return false;
                }
                ctx->cpu_thread_count = num_threads;
                ctx->thread_pool_initialized = true;
                LOG_INFO("Thread pool created with %u threads for mode switch", num_threads);
                return true;
            }

        case EXEC_MODE_HYBRID:
            {
                // Initialize both GPU and thread pool
                bool gpu_ok = false;
                bool pool_ok = false;

                #ifdef NIMCP_ENABLE_CUDA
                ctx->gpu_device_id = 0;
                if (cudaSetDevice(ctx->gpu_device_id) == cudaSuccess) {
                    ctx->gpu_initialized = true;
                    gpu_ok = true;
                    LOG_DEBUG("CUDA device initialized for hybrid mode");
                }
                #endif

                uint32_t num_threads = ctx->config.cpu_threads > 0 ? ctx->config.cpu_threads : 4;
                if (num_threads > NIMCP_POOL_MAX_THREADS) {
                    num_threads = NIMCP_POOL_MAX_THREADS;
                }
                ctx->thread_pool = nimcp_pool_create(num_threads);
                if (ctx->thread_pool) {
                    ctx->cpu_thread_count = num_threads;
                    ctx->thread_pool_initialized = true;
                    pool_ok = true;
                    LOG_DEBUG("Thread pool created for hybrid mode");
                }

                // Hybrid mode requires at least one to succeed
                if (gpu_ok || pool_ok) {
                    LOG_INFO("Hybrid mode initialized (GPU=%s, CPU pool=%s)",
                             gpu_ok ? "yes" : "no", pool_ok ? "yes" : "no");
                    return true;
                }
                LOG_ERROR("Failed to initialize hybrid mode resources");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "initialize_mode_resources: validation failed");
                return false;
            }

        case EXEC_MODE_CPU_SEQUENTIAL:
            ctx->cpu_thread_count = 1;
            LOG_DEBUG("Sequential CPU mode initialized");
            return true;

        case EXEC_MODE_GPU_ROCM:
        case EXEC_MODE_GPU_OPENCL:
            // ROCm and OpenCL not fully implemented yet
            LOG_WARN("ROCm/OpenCL mode not fully implemented, using basic init");
            return true;

        case EXEC_MODE_DISTRIBUTED_CPU:
        case EXEC_MODE_DISTRIBUTED_GPU:
            // Distributed modes require MPI/network - basic init only
            LOG_WARN("Distributed mode initialization is basic only");
            return true;

        case EXEC_MODE_AUTO:
        default:
            // AUTO should have been resolved before calling this
            LOG_DEBUG("Default/AUTO mode initialization");
            return true;
    }
}

/**
 * @brief Set execution mode
 *
 * WHAT: Switch execution context to a different mode
 * WHY:  Allow dynamic mode switching based on workload or conditions
 * HOW:  Synchronize, cleanup old resources, initialize new resources
 */
bool execution_context_set_mode(execution_context_t ctx, execution_mode_t new_mode)
{
    // Guard: Validate context
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_context_set_mode: ctx is NULL");
        return false;
    }

    // No change needed if already in requested mode
    if (ctx->active_mode == new_mode) {
        LOG_DEBUG("Already in mode %d, no switch needed", new_mode);
        return true;
    }

    // Check if mode is supported
    if (!execution_mode_is_supported(new_mode)) {
        LOG_WARN("Requested mode %d is not supported on this system", new_mode);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "execution_context_set_mode: execution_mode_is_supported is NULL");
        return false;
    }

    LOG_INFO("Switching execution mode from %d to %d", ctx->active_mode, new_mode);

    // Synchronize current mode before switching
    execution_synchronize(ctx);

    // Save old mode for potential rollback
    execution_mode_t old_mode = ctx->active_mode;

    // Cleanup old mode resources
    cleanup_mode_resources(ctx, old_mode);

    // Initialize new mode resources
    if (!initialize_mode_resources(ctx, new_mode)) {
        LOG_ERROR("Failed to initialize resources for mode %d", new_mode);

        // Attempt to rollback to old mode
        if (ctx->config.auto_fallback) {
            LOG_WARN("Attempting rollback to previous mode %d", old_mode);
            if (initialize_mode_resources(ctx, old_mode)) {
                ctx->active_mode = old_mode;
                LOG_INFO("Rollback successful, remaining in mode %d", old_mode);
            } else {
                // Last resort: fall back to CPU sequential
                LOG_WARN("Rollback failed, falling back to CPU sequential");
                ctx->active_mode = EXEC_MODE_CPU_SEQUENTIAL;
                ctx->cpu_thread_count = 1;
            }
        }
        return false;
    }

    // Update active mode
    ctx->active_mode = new_mode;
    LOG_INFO("Successfully switched to execution mode %d", new_mode);

    return true;
}

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Allocate memory
 */
void* execution_alloc(execution_context_t ctx, size_t size)
{
    if (!ctx || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "execution_alloc: ctx is NULL");
        return NULL;
    }

    switch (ctx->active_mode) {
        case EXEC_MODE_GPU_CUDA:
            #ifdef NIMCP_ENABLE_CUDA
            if (ctx->config.use_unified_memory) {
                void* ptr = NULL;
                cudaMallocManaged(&ptr, size, cudaMemAttachGlobal);
                return ptr;
            } else {
                void* ptr = NULL;
                cudaMalloc(&ptr, size);
                return ptr;
            }
            #else
            // Fallback to CPU
            return nimcp_malloc(size);
            #endif

        default:
            // CPU allocation
            return nimcp_malloc(size);
    }
}

/**
 * @brief Free memory
 */
void execution_free(execution_context_t ctx, void* ptr)
{
    if (!ctx || !ptr) {
        return;
    }

    switch (ctx->active_mode) {
        case EXEC_MODE_GPU_CUDA:
            #ifdef NIMCP_ENABLE_CUDA
            cudaFree(ptr);
            #else
            nimcp_free(ptr);
            #endif
            break;

        default:
            nimcp_free(ptr);
            break;
    }
}

/**
 * @brief Copy memory
 */
bool execution_memcpy(execution_context_t ctx, void* dst, const void* src,
                     size_t size, bool host_to_device)
{
    if (!ctx || !dst || !src || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_memcpy: required parameter is NULL (ctx, dst, src)");
        return false;
    }

    switch (ctx->active_mode) {
        case EXEC_MODE_GPU_CUDA:
            #ifdef NIMCP_ENABLE_CUDA
            {
                enum cudaMemcpyKind kind = host_to_device ?
                    cudaMemcpyHostToDevice : cudaMemcpyDeviceToHost;
                cudaError_t err = cudaMemcpy(dst, src, size, kind);
                return (err == cudaSuccess);
            }
            #else
            // Fallback to CPU
            memcpy(dst, src, size);
            return true;
            #endif

        default:
            // CPU memcpy
            memcpy(dst, src, size);
            return true;
    }
}

//=============================================================================
// Synchronization
//=============================================================================

/**
 * @brief Synchronize execution
 */
bool execution_synchronize(execution_context_t ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_synchronize: ctx is NULL");
        return false;
    }

    switch (ctx->active_mode) {
        case EXEC_MODE_GPU_CUDA:
            #ifdef NIMCP_ENABLE_CUDA
            cudaError_t err = cudaDeviceSynchronize();
            return (err == cudaSuccess);
            #else
            return true;
            #endif

        default:
            // CPU - already synchronous
            return true;
    }
}

/**
 * @brief Get statistics
 */
bool execution_get_stats(execution_context_t ctx, uint64_t* total_ops,
                        double* total_time_ms)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "execution_get_stats: ctx is NULL");
        return false;
    }

    if (total_ops) {
        *total_ops = ctx->total_operations;
    }

    if (total_time_ms) {
        *total_time_ms = ctx->total_time_ms;
    }

    return true;
}

//=============================================================================
// Default Configurations
//=============================================================================

/**
 * @brief Get default config for mode
 */
execution_config_t execution_get_default_config(execution_mode_t mode)
{
    execution_config_t config = {0};

    config.mode = mode;
    config.auto_fallback = true;
    config.fallback_mode = EXEC_MODE_CPU_PARALLEL;

    switch (mode) {
        case EXEC_MODE_CPU_SEQUENTIAL:
            config.cpu_threads = 1;
            config.batch_size = 1;
            break;

        case EXEC_MODE_CPU_PARALLEL:
            {
                hardware_capabilities_t caps;
                execution_detect_capabilities(&caps);
                config.cpu_threads = caps.cpu_threads;
                config.batch_size = NIMCP_DEFAULT_BATCH_SIZE;
            }
            break;

        case EXEC_MODE_GPU_CUDA:
            config.gpu_blocks = 256;
            config.gpu_threads_per_block = 256;
            config.use_unified_memory = false;
            config.pin_cpu_memory = true;
            config.batch_size = 1024;
            break;

        default:
            config.cpu_threads = 4;
            config.batch_size = 16;
            break;
    }

    return config;
}

/**
 * @brief Get optimal config
 */
execution_config_t execution_get_optimal_config(uint32_t num_neurons)
{
    execution_mode_t mode = execution_get_recommended_mode(num_neurons, 100);
    return execution_get_default_config(mode);
}
