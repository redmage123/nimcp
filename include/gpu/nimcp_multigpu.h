/**
 * @file nimcp_multigpu.h
 * @brief Multi-GPU Support for Distributed Neural Network Computation
 *
 * WHAT: Provides transparent multi-GPU execution with automatic work distribution
 * WHY:  Scale to larger networks using multiple GPUs in single machine
 * HOW:  Device management, work partitioning, memory synchronization, load balancing
 *
 * ARCHITECTURE:
 *
 *   Neural Network
 *         │
 *    ┌────▼─────┐
 *    │ MultiGPU │
 *    │ Manager  │
 *    └────┬─────┘
 *         │
 *    ┌────┴─────────────┬──────────────┬───────────────┐
 *    │                  │              │               │
 * ┌──▼───┐         ┌───▼────┐     ┌──▼──────┐    ┌──▼──────┐
 * │GPU 0 │         │ GPU 1  │     │  GPU 2  │    │  GPU 3  │
 * │Layer │         │ Layer  │     │  Layer  │    │  Layer  │
 * │0-25% │         │ 25-50% │     │  50-75% │    │  75-100%│
 * └──────┘         └────────┘     └─────────┘    └─────────┘
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Different partitioning strategies (layer, neuron, hybrid)
 * - Factory Pattern: Device initialization and work queue creation
 * - Observer Pattern: Load balancing monitors GPU utilization
 * - Command Pattern: Async GPU operations via command queues
 *
 * WORK DISTRIBUTION STRATEGIES:
 * 1. LAYER_PARTITION: Split network by layers (good for deep networks)
 * 2. NEURON_PARTITION: Split neurons across GPUs (good for wide networks)
 * 3. HYBRID_PARTITION: Mix of layer + neuron splitting (best for most)
 * 4. DYNAMIC_PARTITION: Adaptive based on runtime performance
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Ideal speedup: N×GPUs (with perfect load balance)
 * - Communication overhead: 5-15% depending on partition strategy
 * - Memory per GPU: total_memory / num_gpus + communication buffers
 * - Synchronization: Barrier after each network layer
 *
 * COMPLEXITY:
 * - Device enumeration: O(N) where N = num_GPUs
 * - Work distribution: O(L) where L = num_layers
 * - Memory sync: O(M) where M = data_to_sync
 * - Load balance check: O(N) per iteration
 *
 * DESIGN PRINCIPLES (NIMCP Standards):
 * - No nested ifs: Guard clauses only
 * - Functions < 50 lines: Helper functions for complex logic
 * - Single responsibility: Each function does exactly one thing
 * - Clear documentation: WHAT/WHY/HOW for all public functions
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7 (Multi-GPU)
 */

#ifndef NIMCP_MULTIGPU_H
#define NIMCP_MULTIGPU_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "gpu/nimcp_execution_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Multi-GPU Configuration
//=============================================================================

/**
 * @brief Work partitioning strategies
 */
typedef enum {
    MULTIGPU_PARTITION_LAYER,    /**< Split by network layers (deep networks) */
    MULTIGPU_PARTITION_NEURON,   /**< Split by neurons (wide networks) */
    MULTIGPU_PARTITION_HYBRID,   /**< Mix of layer + neuron (adaptive) */
    MULTIGPU_PARTITION_DYNAMIC,  /**< Runtime-adaptive based on performance */
    MULTIGPU_PARTITION_AUTO      /**< Auto-select based on network topology */
} multigpu_partition_strategy_t;

/**
 * @brief Load balancing strategies
 */
typedef enum {
    MULTIGPU_LOADBALANCE_STATIC,    /**< Fixed allocation (fastest) */
    MULTIGPU_LOADBALANCE_DYNAMIC,   /**< Redistribute based on utilization */
    MULTIGPU_LOADBALANCE_ADAPTIVE   /**< Learn optimal distribution */
} multigpu_loadbalance_strategy_t;

/**
 * @brief GPU device information
 */
typedef struct {
    int device_id;                   /**< CUDA device ID */
    char name[256];                  /**< Device name (e.g., "NVIDIA RTX 4090") */
    uint64_t total_memory_bytes;     /**< Total device memory */
    uint64_t free_memory_bytes;      /**< Available memory */
    uint32_t compute_capability;     /**< CUDA compute capability */
    uint32_t multiprocessor_count;   /**< Number of SMs */
    uint32_t max_threads_per_block;  /**< Max threads per block */
    bool peer_access_available;      /**< P2P memory access supported */
    float compute_utilization;       /**< Current utilization [0, 1] */
    float memory_utilization;        /**< Memory usage [0, 1] */
} multigpu_device_info_t;

/**
 * @brief Multi-GPU configuration
 */
typedef struct {
    // Device selection
    uint32_t num_devices;            /**< Number of GPUs to use (0 = all) */
    int* device_ids;                 /**< Specific device IDs (NULL = auto) */
    bool enable_peer_access;         /**< Enable GPU-to-GPU P2P transfers */

    // Partitioning strategy
    multigpu_partition_strategy_t partition_strategy;
    multigpu_loadbalance_strategy_t loadbalance_strategy;

    // Memory management
    uint64_t max_memory_per_gpu;     /**< Max memory per GPU (0 = auto) */
    bool enable_unified_memory;      /**< Use CUDA unified memory */
    bool pin_host_memory;            /**< Pin CPU memory for fast transfers */
    uint64_t sync_buffer_size;       /**< Size of inter-GPU sync buffers */

    // Performance tuning
    uint32_t streams_per_device;     /**< CUDA streams per GPU (default: 4) */
    bool enable_concurrent_kernels;  /**< Allow concurrent kernel execution */
    bool enable_async_transfers;     /**< Async memory transfers */
    uint32_t pipeline_depth;         /**< Depth of operation pipeline */

    // Load balancing
    uint32_t loadbalance_interval;   /**< Check balance every N iterations */
    float imbalance_threshold;       /**< Rebalance if diff > X (default: 0.15) */
    bool enable_work_stealing;       /**< Allow idle GPUs to steal work */

    // Monitoring
    bool enable_profiling;           /**< Enable performance profiling */
    bool enable_validation;          /**< Validate results across GPUs */
    bool verbose_logging;            /**< Log detailed multi-GPU operations */
} multigpu_config_t;

/**
 * @brief Multi-GPU context (opaque handle)
 */
typedef struct multigpu_context_struct* multigpu_context_t;

//=============================================================================
// Device Management
//=============================================================================

/**
 * @brief Enumerate available GPU devices
 *
 * WHAT: Query system for all CUDA-capable GPU devices
 * WHY:  Need to know what GPUs are available before allocation
 * HOW:  Call cudaGetDeviceCount() and query each device
 *
 * COMPLEXITY: O(N) where N = number of GPUs
 *
 * @param devices Output array of device info (caller allocates)
 * @param max_devices Size of devices array
 * @param count Output: actual number of devices found
 * @return true on success, false on failure
 *
 * THREAD SAFETY: Thread-safe (CUDA API is thread-safe)
 * EXAMPLE:
 * ```c
 * multigpu_device_info_t devices[8];
 * uint32_t count;
 * multigpu_enumerate_devices(devices, 8, &count);
 * printf("Found %u GPUs\n", count);
 * ```
 */
NIMCP_EXPORT bool multigpu_enumerate_devices(
    multigpu_device_info_t* devices,
    uint32_t max_devices,
    uint32_t* count
);

/**
 * @brief Check if GPU pair supports P2P access
 *
 * WHAT: Determine if two GPUs can directly access each other's memory
 * WHY:  P2P avoids slow CPU-mediated transfers (3-5x faster)
 * HOW:  Call cudaDeviceCanAccessPeer()
 *
 * COMPLEXITY: O(1)
 *
 * @param device_id1 First GPU device ID
 * @param device_id2 Second GPU device ID
 * @return true if P2P access supported
 *
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT bool multigpu_check_peer_access(int device_id1, int device_id2);

/**
 * @brief Get recommended number of GPUs for workload
 *
 * WHAT: Analyze network size and recommend GPU count
 * WHY:  Too few GPUs = underutilized, too many = overhead dominates
 * HOW:  Heuristics based on neurons, synapses, memory requirements
 *
 * COMPLEXITY: O(1)
 *
 * HEURISTICS:
 * - < 100K neurons: 1 GPU (single GPU faster than multi-GPU overhead)
 * - 100K-1M neurons: 2 GPUs (good speedup, low overhead)
 * - 1M-10M neurons: 4 GPUs (sweet spot for most workloads)
 * - > 10M neurons: 8+ GPUs (large models benefit from more parallelism)
 *
 * @param num_neurons Number of neurons in network
 * @param num_synapses Average synapses per neuron
 * @param available_gpus Number of GPUs detected
 * @return Recommended number of GPUs to use
 */
NIMCP_EXPORT uint32_t multigpu_get_recommended_count(
    uint32_t num_neurons,
    uint32_t num_synapses,
    uint32_t available_gpus
);

//=============================================================================
// Context Management
//=============================================================================

/**
 * @brief Create multi-GPU execution context
 *
 * WHAT: Initialize multiple GPUs for distributed computation
 * WHY:  Need unified interface to manage N GPUs as single compute resource
 * HOW:  Initialize devices, allocate memory pools, setup P2P, create streams
 *
 * INITIALIZATION SEQUENCE:
 * 1. Enumerate and validate devices
 * 2. Setup P2P access between GPU pairs
 * 3. Allocate memory pools on each GPU
 * 4. Create CUDA streams for async ops
 * 5. Initialize synchronization primitives
 * 6. Setup work partitioning
 *
 * COMPLEXITY: O(N²) for P2P setup where N = num_GPUs
 *
 * @param config Multi-GPU configuration
 * @return Context handle, or NULL on failure
 *
 * FAILURE MODES:
 * - Insufficient GPUs available
 * - Memory allocation failure
 * - P2P access setup failure
 * - CUDA initialization failure
 *
 * THREAD SAFETY: NOT thread-safe (call from single thread)
 *
 * EXAMPLE:
 * ```c
 * multigpu_config_t config = multigpu_default_config();
 * config.num_devices = 4;
 * config.partition_strategy = MULTIGPU_PARTITION_HYBRID;
 * multigpu_context_t ctx = multigpu_context_create(&config);
 * if (!ctx) {
 *     fprintf(stderr, "Failed to initialize multi-GPU\n");
 * }
 * ```
 */
NIMCP_EXPORT multigpu_context_t multigpu_context_create(
    const multigpu_config_t* config
);

/**
 * @brief Destroy multi-GPU context
 *
 * WHAT: Cleanup all GPU resources and synchronize
 * WHY:  Free memory, destroy streams, disable P2P
 * HOW:  Synchronize all GPUs, free memory, destroy CUDA objects
 *
 * CLEANUP SEQUENCE:
 * 1. Synchronize all GPU streams
 * 2. Free device memory on each GPU
 * 3. Destroy CUDA streams
 * 4. Disable P2P access
 * 5. Free host structures
 *
 * COMPLEXITY: O(N) where N = num_GPUs
 *
 * @param ctx Context handle (NULL-safe)
 *
 * THREAD SAFETY: NOT thread-safe (call from single thread)
 */
NIMCP_EXPORT void multigpu_context_destroy(multigpu_context_t ctx);

/**
 * @brief Get number of active GPUs
 *
 * WHAT: Return count of GPUs in this context
 * WHY:  Caller needs to know resource availability
 * HOW:  Return config field
 *
 * COMPLEXITY: O(1)
 *
 * @param ctx Context handle
 * @return Number of GPUs, or 0 on error
 */
NIMCP_EXPORT uint32_t multigpu_get_device_count(multigpu_context_t ctx);

/**
 * @brief Get device info for specific GPU in context
 *
 * WHAT: Query device properties and current utilization
 * WHY:  Monitor per-GPU performance and memory usage
 * HOW:  Return cached device info + query current stats
 *
 * COMPLEXITY: O(1)
 *
 * @param ctx Context handle
 * @param device_index Index in context (0 to num_devices-1)
 * @param info Output device info structure
 * @return true on success, false if index invalid
 */
NIMCP_EXPORT bool multigpu_get_device_info(
    multigpu_context_t ctx,
    uint32_t device_index,
    multigpu_device_info_t* info
);

//=============================================================================
// Work Distribution
//=============================================================================

/**
 * @brief Partition neural network across GPUs
 *
 * WHAT: Divide network computation among available GPUs
 * WHY:  Distribute work to maximize throughput
 * HOW:  Apply partitioning strategy to split layers/neurons
 *
 * PARTITIONING STRATEGIES:
 * - LAYER: GPU 0 = layers [0, N/4), GPU 1 = [N/4, N/2), etc.
 * - NEURON: GPU 0 = neurons [0, M/4), GPU 1 = [M/4, M/2), etc.
 * - HYBRID: Split large layers by neurons, small layers by layer
 * - DYNAMIC: Start with HYBRID, adjust based on runtime metrics
 *
 * COMPLEXITY: O(L) where L = num_layers
 *
 * @param ctx Context handle
 * @param num_layers Number of network layers
 * @param neurons_per_layer Array of neuron counts per layer
 * @return true on success, false on failure
 *
 * SIDE EFFECTS:
 * - Updates internal work distribution map
 * - May allocate device memory
 * - Caches partition for reuse
 */
NIMCP_EXPORT bool multigpu_partition_network(
    multigpu_context_t ctx,
    uint32_t num_layers,
    const uint32_t* neurons_per_layer
);

/**
 * @brief Get GPU assignment for specific layer
 *
 * WHAT: Query which GPU is assigned to process a layer
 * WHY:  Need to know where to send data for that layer
 * HOW:  Lookup in cached partition map
 *
 * COMPLEXITY: O(1)
 *
 * @param ctx Context handle
 * @param layer_index Layer index (0 to num_layers-1)
 * @return GPU device index, or -1 if not partitioned
 */
NIMCP_EXPORT int multigpu_get_layer_assignment(
    multigpu_context_t ctx,
    uint32_t layer_index
);

/**
 * @brief Rebalance work distribution
 *
 * WHAT: Redistribute work based on current GPU utilization
 * WHY:  Adapt to runtime performance imbalances
 * HOW:  Monitor utilization, shift work from busy to idle GPUs
 *
 * REBALANCING ALGORITHM:
 * 1. Query current utilization per GPU
 * 2. Identify imbalanced pairs (diff > threshold)
 * 3. Calculate optimal redistribution
 * 4. Migrate data and update partition map
 * 5. Wait for GPUs to synchronize
 *
 * COMPLEXITY: O(N + M) where N = GPUs, M = data_to_migrate
 *
 * @param ctx Context handle
 * @return true if rebalancing occurred, false if already balanced
 *
 * THREAD SAFETY: NOT thread-safe (call from single thread)
 * COST: Expensive (involves data migration), use sparingly
 */
NIMCP_EXPORT bool multigpu_rebalance_work(multigpu_context_t ctx);

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Allocate distributed memory across GPUs
 *
 * WHAT: Allocate memory split across all GPUs
 * WHY:  Large buffers may not fit on single GPU
 * HOW:  Divide allocation across devices based on partition
 *
 * COMPLEXITY: O(N) where N = num_GPUs
 *
 * @param ctx Context handle
 * @param total_size Total size in bytes
 * @return Array of N device pointers, or NULL on failure
 *
 * MEMORY LAYOUT:
 * - Each GPU gets total_size / N bytes
 * - Caller must free with multigpu_free()
 */
NIMCP_EXPORT void** multigpu_alloc(multigpu_context_t ctx, size_t total_size);

/**
 * @brief Free distributed memory
 *
 * WHAT: Free memory allocated by multigpu_alloc()
 * WHY:  Release GPU memory resources
 * HOW:  Free on each device, then free array
 *
 * COMPLEXITY: O(N) where N = num_GPUs
 *
 * @param ctx Context handle
 * @param device_ptrs Array of device pointers from multigpu_alloc()
 */
NIMCP_EXPORT void multigpu_free(multigpu_context_t ctx, void** device_ptrs);

/**
 * @brief Broadcast data from host to all GPUs
 *
 * WHAT: Copy same data to all GPU devices
 * WHY:  Synchronize shared parameters (weights, config)
 * HOW:  Async copy to each GPU via separate streams
 *
 * COMPLEXITY: O(N × M) where N = GPUs, M = data size
 *
 * @param ctx Context handle
 * @param host_data Source data on CPU
 * @param device_ptrs Destination pointers on GPUs
 * @param size Size in bytes
 * @return true on success
 *
 * OPTIMIZATION: Uses async transfers, returns before completion
 * SYNCHRONIZATION: Call multigpu_synchronize() to wait
 */
NIMCP_EXPORT bool multigpu_broadcast(
    multigpu_context_t ctx,
    const void* host_data,
    void** device_ptrs,
    size_t size
);

/**
 * @brief Gather data from all GPUs to host
 *
 * WHAT: Collect partial results from each GPU
 * WHY:  Aggregate results after distributed computation
 * HOW:  Async copy from each GPU, concatenate on host
 *
 * COMPLEXITY: O(N × M) where N = GPUs, M = data per GPU
 *
 * @param ctx Context handle
 * @param device_ptrs Source pointers on GPUs
 * @param host_data Destination buffer on CPU (caller allocates)
 * @param size_per_gpu Size in bytes per GPU
 * @return true on success
 *
 * OUTPUT FORMAT: [GPU0_data][GPU1_data][GPU2_data]...
 */
NIMCP_EXPORT bool multigpu_gather(
    multigpu_context_t ctx,
    void** device_ptrs,
    void* host_data,
    size_t size_per_gpu
);

/**
 * @brief Synchronize data between GPU pairs
 *
 * WHAT: Exchange boundary data between adjacent GPUs
 * WHY:  GPUs need each other's results for next layer
 * HOW:  Use P2P if available, else copy via host
 *
 * COMPLEXITY: O(M) where M = boundary_data_size
 *
 * @param ctx Context handle
 * @param src_device Source GPU index
 * @param dst_device Destination GPU index
 * @param src_data Pointer to source data on src_device
 * @param dst_data Pointer to destination buffer on dst_device
 * @param size Size in bytes
 * @return true on success
 *
 * OPTIMIZATION: P2P transfer is 3-5x faster than via-host
 */
NIMCP_EXPORT bool multigpu_sync_devices(
    multigpu_context_t ctx,
    uint32_t src_device,
    uint32_t dst_device,
    const void* src_data,
    void* dst_data,
    size_t size
);

//=============================================================================
// Synchronization
//=============================================================================

/**
 * @brief Synchronize all GPUs
 *
 * WHAT: Wait for all GPU operations to complete
 * WHY:  Ensure consistency before accessing results
 * HOW:  Call cudaDeviceSynchronize() on each GPU
 *
 * COMPLEXITY: O(N) where N = num_GPUs
 *
 * @param ctx Context handle
 * @return true on success
 *
 * THREAD SAFETY: Thread-safe
 * COST: Expensive (stalls pipeline), use sparingly
 */
NIMCP_EXPORT bool multigpu_synchronize(multigpu_context_t ctx);

/**
 * @brief Check if all GPUs are idle
 *
 * WHAT: Query if any GPU has pending operations
 * WHY:  Non-blocking check for completion
 * HOW:  Call cudaStreamQuery() on all streams
 *
 * COMPLEXITY: O(N × S) where N = GPUs, S = streams per GPU
 *
 * @param ctx Context handle
 * @return true if all idle, false if any busy
 */
NIMCP_EXPORT bool multigpu_is_idle(multigpu_context_t ctx);

//=============================================================================
// Performance Monitoring
//=============================================================================

/**
 * @brief Get multi-GPU performance statistics
 *
 * WHAT: Aggregate stats from all GPUs
 * WHY:  Monitor efficiency and detect bottlenecks
 * HOW:  Query each GPU, compute aggregate metrics
 *
 * COMPLEXITY: O(N) where N = num_GPUs
 *
 * @param ctx Context handle
 * @param total_ops Output: total operations across all GPUs
 * @param total_time_ms Output: wall-clock time (ms)
 * @param avg_utilization Output: average GPU utilization [0, 1]
 * @param load_imbalance Output: max(util) - min(util)
 * @return true on success
 *
 * METRICS:
 * - total_ops: Sum of operations across GPUs
 * - total_time_ms: Wall-clock time (not sum of GPU times)
 * - avg_utilization: Mean compute utilization
 * - load_imbalance: 0 = perfect balance, 1 = worst case
 */
NIMCP_EXPORT bool multigpu_get_performance_stats(
    multigpu_context_t ctx,
    uint64_t* total_ops,
    double* total_time_ms,
    float* avg_utilization,
    float* load_imbalance
);

/**
 * @brief Get per-GPU statistics
 *
 * WHAT: Detailed stats for specific GPU
 * WHY:  Identify which GPU is bottleneck
 * HOW:  Query device-specific metrics
 *
 * COMPLEXITY: O(1)
 *
 * @param ctx Context handle
 * @param device_index GPU index (0 to num_devices-1)
 * @param ops Output: operations on this GPU
 * @param time_ms Output: time spent (ms)
 * @param utilization Output: compute utilization [0, 1]
 * @return true on success
 */
NIMCP_EXPORT bool multigpu_get_device_stats(
    multigpu_context_t ctx,
    uint32_t device_index,
    uint64_t* ops,
    double* time_ms,
    float* utilization
);

//=============================================================================
// Default Configurations
//=============================================================================

/**
 * @brief Get default multi-GPU configuration
 *
 * WHAT: Sensible defaults for multi-GPU setup
 * WHY:  Most users don't need to tune parameters
 * HOW:  Return struct with recommended values
 *
 * COMPLEXITY: O(1)
 *
 * DEFAULTS:
 * - num_devices: 0 (use all available)
 * - partition_strategy: HYBRID
 * - loadbalance_strategy: DYNAMIC
 * - enable_peer_access: true
 * - streams_per_device: 4
 * - enable_concurrent_kernels: true
 * - enable_async_transfers: true
 * - loadbalance_interval: 100 iterations
 * - imbalance_threshold: 0.15 (15%)
 *
 * @return Default configuration
 */
NIMCP_EXPORT multigpu_config_t multigpu_default_config(void);

/**
 * @brief Get optimal multi-GPU configuration for workload
 *
 * WHAT: Auto-tune config based on network size
 * WHY:  Different networks need different strategies
 * HOW:  Heuristics based on neurons, layers, available GPUs
 *
 * COMPLEXITY: O(1)
 *
 * @param num_neurons Number of neurons
 * @param num_layers Number of layers
 * @param available_gpus Number of GPUs detected
 * @return Optimized configuration
 */
NIMCP_EXPORT multigpu_config_t multigpu_get_optimal_config(
    uint32_t num_neurons,
    uint32_t num_layers,
    uint32_t available_gpus
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MULTIGPU_H
