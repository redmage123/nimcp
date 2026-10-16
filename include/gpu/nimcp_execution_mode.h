/**
 * @file nimcp_execution_mode.h
 * @brief Execution Mode Selection for Neural Networks (CPU/GPU/Distributed)
 *
 * WHAT: Provides runtime selection between CPU, GPU, and distributed execution
 * WHY:  Different platforms have different capabilities (laptop vs server vs cluster)
 * HOW:  Conditional compilation + runtime detection + fallback strategies
 *
 * ARCHITECTURE:
 *
 *   Application Code
 *         │
 *    ┌────▼────┐
 *    │  Mode   │
 *    │ Select  │
 *    └────┬────┘
 *         │
 *    ┌────┴─────────────┬──────────────┐
 *    │                  │              │
 * ┌──▼───┐         ┌───▼────┐     ┌──▼──────┐
 * │ CPU  │         │  GPU   │     │ Distrib │
 * │ Mode │         │  P2P   │     │  P2P    │
 * └──────┘         └────────┘     └─────────┘
 *
 * DESIGN DECISIONS:
 * 1. CPU Mode: Always available (fallback)
 * 2. GPU Mode: Conditional (requires CUDA/ROCm)
 * 3. Distributed: Conditional (requires network)
 * 4. Auto-detect: Query hardware capabilities at runtime
 * 5. Explicit override: User can force specific mode
 *
 * COMPILE-TIME FLAGS:
 * - NIMCP_ENABLE_CUDA: Enable NVIDIA CUDA support
 * - NIMCP_ENABLE_ROCM: Enable AMD ROCm support
 * - NIMCP_ENABLE_DISTRIBUTED: Enable network distribution
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6 (GPU P2P)
 */

#ifndef NIMCP_EXECUTION_MODE_H
#define NIMCP_EXECUTION_MODE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Execution Modes
//=============================================================================

/**
 * @brief Available execution modes
 */
typedef enum {
    EXEC_MODE_CPU_SEQUENTIAL,    /**< CPU single-threaded */
    EXEC_MODE_CPU_PARALLEL,      /**< CPU multi-threaded */
    EXEC_MODE_GPU_CUDA,          /**< NVIDIA GPU (CUDA) */
    EXEC_MODE_GPU_ROCM,          /**< AMD GPU (ROCm) */
    EXEC_MODE_GPU_OPENCL,        /**< OpenCL (cross-platform) */
    EXEC_MODE_DISTRIBUTED_CPU,   /**< Distributed across CPU nodes */
    EXEC_MODE_DISTRIBUTED_GPU,   /**< Distributed across GPU nodes */
    EXEC_MODE_HYBRID,            /**< CPU + GPU hybrid */
    EXEC_MODE_NEURON,            /**< AWS Inferentia NeuronCore */
    EXEC_MODE_AUTO               /**< Auto-detect best mode */
} execution_mode_t;

/**
 * @brief Hardware capabilities detected at runtime
 */
typedef struct {
    // CPU capabilities
    bool cpu_available;
    uint32_t cpu_cores;
    uint32_t cpu_threads;
    bool cpu_avx2;               /**< AVX2 SIMD support */
    bool cpu_avx512;             /**< AVX512 SIMD support */

    // GPU capabilities
    bool cuda_available;
    bool rocm_available;
    bool opencl_available;
    uint32_t gpu_count;
    uint32_t gpu_compute_units;  /**< CUDA cores or compute units */
    uint64_t gpu_memory_mb;      /**< GPU memory in MB */
    uint32_t gpu_compute_capability; /**< CUDA compute capability */

    // Network capabilities
    bool network_available;
    uint32_t network_nodes;
    uint32_t network_bandwidth_mbps;

    // Recommended mode
    execution_mode_t recommended_mode;
} hardware_capabilities_t;

/**
 * @brief Execution mode configuration
 */
typedef struct {
    execution_mode_t mode;

    // Thread/block configuration
    uint32_t cpu_threads;        /**< Number of CPU threads to use */
    uint32_t gpu_blocks;         /**< GPU blocks per grid */
    uint32_t gpu_threads_per_block; /**< Threads per block */

    // Memory management
    bool pin_cpu_memory;         /**< Pin CPU memory for GPU transfers */
    bool use_unified_memory;     /**< Use CUDA unified memory */
    uint64_t gpu_memory_limit;   /**< Max GPU memory to use (bytes) */

    // Performance tuning
    uint32_t batch_size;         /**< Batch size for processing */
    bool enable_profiling;       /**< Enable performance profiling */
    bool enable_validation;      /**< Enable correctness validation */

    // Fallback strategy
    execution_mode_t fallback_mode; /**< Mode to use if primary fails */
    bool auto_fallback;          /**< Automatically fallback on error */
} execution_config_t;

/**
 * @brief Execution context (opaque handle)
 */
typedef struct execution_context_struct* execution_context_t;

//=============================================================================
// Hardware Detection
//=============================================================================

/**
 * @brief Detect hardware capabilities
 *
 * WHAT: Query system for CPU, GPU, and network capabilities
 * WHY:  Need to know what execution modes are available
 * HOW:  Platform-specific queries (CUDA API, sysconf, etc.)
 *
 * @param caps Output capabilities structure
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe (read-only after init)
 * CACHING: Results cached after first call
 */
NIMCP_EXPORT bool execution_detect_capabilities(hardware_capabilities_t* caps);

/**
 * @brief Check if specific mode is supported
 *
 * @param mode Execution mode to check
 * @return true if supported on this system
 */
NIMCP_EXPORT bool execution_mode_is_supported(execution_mode_t mode);

/**
 * @brief Get recommended execution mode for workload
 *
 * @param num_neurons Number of neurons in network
 * @param num_synapses Average synapses per neuron
 * @return Recommended execution mode
 *
 * HEURISTICS:
 * - < 1K neurons: CPU sequential
 * - 1K-10K neurons: CPU parallel
 * - 10K-1M neurons: GPU CUDA (if available)
 * - > 1M neurons: Distributed GPU
 */
NIMCP_EXPORT execution_mode_t execution_get_recommended_mode(
    uint32_t num_neurons,
    uint32_t num_synapses
);

//=============================================================================
// Execution Context Management
//=============================================================================

/**
 * @brief Create execution context
 *
 * @param config Execution configuration
 * @return Context handle, or NULL on failure
 *
 * BEHAVIOR:
 * - Initializes CPU threads / GPU device / network
 * - Allocates memory pools
 * - Validates configuration
 * - Falls back if primary mode unavailable
 */
NIMCP_EXPORT execution_context_t execution_context_create(
    const execution_config_t* config
);

/**
 * @brief Destroy execution context
 *
 * @param ctx Context handle (NULL-safe)
 *
 * CLEANUP:
 * - Synchronizes GPU (if used)
 * - Frees memory pools
 * - Shuts down threads/devices
 */
NIMCP_EXPORT void execution_context_destroy(execution_context_t ctx);

/**
 * @brief Get active execution mode
 *
 * @param ctx Context handle
 * @return Current execution mode
 */
NIMCP_EXPORT execution_mode_t execution_context_get_mode(execution_context_t ctx);

/**
 * @brief Switch execution mode at runtime
 *
 * @param ctx Context handle
 * @param new_mode New execution mode
 * @return true on success, false if mode not supported
 *
 * WARNING: Expensive operation (requires synchronization)
 */
NIMCP_EXPORT bool execution_context_set_mode(
    execution_context_t ctx,
    execution_mode_t new_mode
);

//=============================================================================
// Memory Management (Cross-Platform)
//=============================================================================

/**
 * @brief Allocate memory in execution context
 *
 * @param ctx Context handle
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL
 *
 * BEHAVIOR:
 * - CPU mode: nimcp_malloc()
 * - GPU mode: cudaMalloc() or cudaMallocManaged()
 * - Distributed: allocate on appropriate node
 */
NIMCP_EXPORT void* execution_alloc(execution_context_t ctx, size_t size);

/**
 * @brief Free memory allocated in execution context
 *
 * @param ctx Context handle
 * @param ptr Pointer to free (NULL-safe)
 */
NIMCP_EXPORT void execution_free(execution_context_t ctx, void* ptr);

/**
 * @brief Copy memory between host and device
 *
 * @param ctx Context handle
 * @param dst Destination pointer
 * @param src Source pointer
 * @param size Number of bytes to copy
 * @param host_to_device true = CPU->GPU, false = GPU->CPU
 * @return true on success
 */
NIMCP_EXPORT bool execution_memcpy(
    execution_context_t ctx,
    void* dst,
    const void* src,
    size_t size,
    bool host_to_device
);

//=============================================================================
// Synchronization
//=============================================================================

/**
 * @brief Synchronize execution (wait for all operations to complete)
 *
 * @param ctx Context handle
 * @return true on success
 *
 * BEHAVIOR:
 * - CPU: No-op (already synchronous)
 * - GPU: cudaDeviceSynchronize()
 * - Distributed: MPI_Barrier()
 */
NIMCP_EXPORT bool execution_synchronize(execution_context_t ctx);

/**
 * @brief Get execution statistics
 *
 * @param ctx Context handle
 * @param total_ops Output: total operations executed
 * @param total_time_ms Output: total execution time (ms)
 * @return true on success
 */
NIMCP_EXPORT bool execution_get_stats(
    execution_context_t ctx,
    uint64_t* total_ops,
    double* total_time_ms
);

//=============================================================================
// Default Configurations
//=============================================================================

/**
 * @brief Get default execution config for mode
 *
 * @param mode Execution mode
 * @return Default configuration for that mode
 */
NIMCP_EXPORT execution_config_t execution_get_default_config(execution_mode_t mode);

/**
 * @brief Get auto-detected optimal configuration
 *
 * @param num_neurons Network size (neurons)
 * @return Optimal configuration for detected hardware
 */
NIMCP_EXPORT execution_config_t execution_get_optimal_config(uint32_t num_neurons);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EXECUTION_MODE_H
