/**
 * @file nimcp_kg_gpu.h
 * @brief GPU Acceleration for Brain Knowledge Graph Operations
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: GPU-accelerated graph analytics, similarity search, and quantum simulation
 * WHY:  Large-scale KG operations (centrality, community detection, similarity)
 *       benefit from massive parallelism available on GPUs (30-60x speedup)
 * HOW:  Abstract GPU backend (CUDA/ROCm/Metal/Vulkan/OpenCL), memory management,
 *       and kernel dispatch with async execution and result collection
 *
 * ARCHITECTURE:
 * ```
 * +-----------------------------------------------------------------------------+
 * |                    KG GPU ACCELERATION ARCHITECTURE                          |
 * +-----------------------------------------------------------------------------+
 * |                                                                              |
 * |  CPU SIDE                           GPU SIDE                                 |
 * |  --------                           --------                                 |
 * |  +------------------+               +----------------------------------+     |
 * |  | KG Operations    |               | GPU Memory Pool                   |    |
 * |  | - Query          |  ---------->  | +------------------------------+ |    |
 * |  | - Analytics      |  Transfer     | | Graph Adjacency Matrix       | |    |
 * |  | - Similarity     |  Buffers      | | (CSR/CSC format)             | |    |
 * |  +------------------+               | +------------------------------+ |    |
 * |                                      | +------------------------------+ |    |
 * |  +------------------+               | | Node Embeddings              | |    |
 * |  | Async Dispatch   |               | | (float16 for Tensor Cores)   | |    |
 * |  | +--------------+ |               | +------------------------------+ |    |
 * |  | | Stream 0     | |  ---------->  | +------------------------------+ |    |
 * |  | | Stream 1     | |  Multi-       | | Weight Tensors               | |    |
 * |  | | Stream 2     | |  Stream       | | (SNN/LNN/CNN snapshots)      | |    |
 * |  | | Stream 3     | |               | +------------------------------+ |    |
 * |  | +--------------+ |               +----------------------------------+     |
 * |  +------------------+                                                        |
 * |                                      +----------------------------------+    |
 * |  +------------------+               | GPU Kernels                       |    |
 * |  | Result Callback  |  <----------  | +------------+ +------------+   |    |
 * |  | (async)          |  Results      | | PageRank   | | Louvain    |   |    |
 * |  +------------------+               | | Kernel     | | Kernel     |   |    |
 * |                                      | +------------+ +------------+   |    |
 * |                                      | +------------+ +------------+   |    |
 * |                                      | | KNN Search | | Quantum    |   |    |
 * |                                      | | Kernel     | | Walk Kernel|   |    |
 * |                                      | +------------+ +------------+   |    |
 * |                                      +----------------------------------+    |
 * |                                                                              |
 * |  SPEEDUP TARGETS:                                                            |
 * |  -----------------                                                           |
 * |  | Operation            | CPU Time | GPU Time | Speedup |                   |
 * |  |----------------------|----------|----------|---------|                   |
 * |  | Centrality (1M nodes)| 45s      | 0.8s     | 56x     |                   |
 * |  | Community Detection  | 120s     | 3.2s     | 37x     |                   |
 * |  | KNN Search (k=100)   | 2.1s     | 0.04s    | 52x     |                   |
 * |  | Quantum Walk (1000)  | 8.5s     | 0.15s    | 57x     |                   |
 * |  | Weight Stats (10M)   | 1.2s     | 0.02s    | 60x     |                   |
 * |  | AES Encryption (1GB) | 3.8s     | 0.12s    | 32x     |                   |
 * |                                                                              |
 * +-----------------------------------------------------------------------------+
 * ```
 *
 * THREAD SAFETY: GPU context operations are thread-safe via internal mutex
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_GPU_H
#define NIMCP_KG_GPU_H

#include "core/brain/nimcp_brain_kg.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Types from other KG headers (centrality, similarity, quantum) */
typedef struct kg_centrality_metrics kg_centrality_metrics_t;
typedef struct kg_community kg_community_t;
typedef struct kg_similarity_result kg_similarity_result_t;
typedef struct kg_quantum_walk_result kg_quantum_walk_result_t;
typedef struct kg_weight_stats kg_weight_stats_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum GPU device name length */
#define KG_GPU_MAX_DEVICE_NAME      128

/** Maximum number of GPU devices supported */
#define KG_GPU_MAX_DEVICES          16

/** Default number of CUDA/compute streams */
#define KG_GPU_DEFAULT_STREAMS      4

/** Default threads per block for GPU kernels */
#define KG_GPU_DEFAULT_BLOCK_SIZE   256

/** Default transfer buffers for async operations */
#define KG_GPU_DEFAULT_TRANSFER_BUFFERS 4

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief GPU compute backend
 *
 * WHAT: Available GPU compute APIs/frameworks
 * WHY:  Support multiple GPU vendors and platforms
 * HOW:  Runtime backend selection based on available hardware
 */
typedef enum {
    KG_GPU_NONE = 0,                     /**< CPU only (no GPU acceleration) */
    KG_GPU_CUDA,                         /**< NVIDIA CUDA */
    KG_GPU_ROCM,                         /**< AMD ROCm/HIP */
    KG_GPU_METAL,                        /**< Apple Metal */
    KG_GPU_VULKAN,                       /**< Vulkan compute shaders */
    KG_GPU_OPENCL                        /**< OpenCL (cross-platform) */
} kg_gpu_backend_t;

/**
 * @brief GPU acceleration targets (bitmask)
 *
 * WHAT: Categories of operations that can be GPU-accelerated
 * WHY:  Allow selective enabling/disabling of GPU for specific workloads
 * HOW:  Bitmask flags combined with OR operator
 */
typedef enum {
    KG_GPU_TARGET_GRAPH_ANALYTICS = 1 << 0,    /**< Centrality, community detection */
    KG_GPU_TARGET_SIMILARITY      = 1 << 1,    /**< KD-tree, embedding search */
    KG_GPU_TARGET_QUANTUM_SIM     = 1 << 2,    /**< Quantum walk simulation */
    KG_GPU_TARGET_WEIGHT_COMPUTE  = 1 << 3,    /**< Weight statistics */
    KG_GPU_TARGET_ENCRYPTION      = 1 << 4,    /**< AES encryption (if supported) */
    KG_GPU_TARGET_COMPRESSION     = 1 << 5,    /**< Data compression */
    KG_GPU_TARGET_ALL             = 0xFFFFFFFF /**< All targets enabled */
} kg_gpu_target_t;

/* ============================================================================
 * Data Structures - Device Information
 * ============================================================================ */

/**
 * @brief GPU device information
 *
 * WHAT: Hardware capabilities and status of a GPU device
 * WHY:  Device selection and workload optimization
 * HOW:  Queried from GPU driver at enumeration time
 */
typedef struct {
    uint32_t device_id;                  /**< Device index (0-based) */
    char name[KG_GPU_MAX_DEVICE_NAME];   /**< Device name (e.g., "NVIDIA RTX 4090") */
    kg_gpu_backend_t backend;            /**< Backend API for this device */

    /* Memory */
    uint64_t memory_total;               /**< Total VRAM in bytes */
    uint64_t memory_free;                /**< Free VRAM in bytes */

    /* Compute capability */
    uint32_t compute_units;              /**< SMs (CUDA) or CUs (ROCm/OpenCL) */
    uint32_t max_threads_per_block;      /**< Maximum threads per block/workgroup */
    float compute_capability;            /**< CUDA compute capability (e.g., 8.9) */

    /* Feature support */
    bool supports_fp16;                  /**< Half-precision (FP16) support */
    bool supports_tensor_cores;          /**< Tensor core / matrix unit support */
} kg_gpu_device_t;

/* ============================================================================
 * Data Structures - Configuration
 * ============================================================================ */

/**
 * @brief GPU memory pool configuration
 *
 * WHAT: Settings for GPU memory allocation strategy
 * WHY:  Control memory usage, enable async transfers
 * HOW:  Applied during GPU context initialization
 */
typedef struct {
    uint64_t pool_size_bytes;            /**< GPU memory pool size (0 = auto) */
    uint64_t max_allocation_bytes;       /**< Max single allocation size */
    bool enable_unified_memory;          /**< Use unified memory if available */
    bool enable_async_transfers;         /**< Enable async CPU<->GPU transfers */
    uint32_t transfer_buffer_count;      /**< Number of transfer staging buffers */
} kg_gpu_memory_config_t;

/**
 * @brief GPU kernel execution configuration
 *
 * WHAT: Settings for GPU kernel launch parameters
 * WHY:  Tune performance for different workloads and hardware
 * HOW:  Applied per-operation or globally via context
 */
typedef struct {
    uint32_t block_size;                 /**< Threads per block (0 = auto) */
    uint32_t grid_size;                  /**< Blocks per grid (0 = auto) */
    bool enable_cooperative_groups;      /**< Use cooperative groups (CUDA) */
    uint32_t shared_memory_bytes;        /**< Shared memory per block (0 = auto) */
    uint32_t stream_count;               /**< Number of concurrent streams */
} kg_gpu_kernel_config_t;

/* ============================================================================
 * Data Structures - Results and Statistics
 * ============================================================================ */

/**
 * @brief GPU operation result with timing and performance metrics
 *
 * WHAT: Detailed performance information for a GPU operation
 * WHY:  Monitor GPU efficiency, compare with CPU baseline
 * HOW:  Populated by GPU operations, includes timing breakdown
 */
typedef struct {
    bool success;                        /**< Operation completed successfully */
    uint64_t gpu_time_ns;                /**< GPU execution time (nanoseconds) */
    uint64_t transfer_time_ns;           /**< CPU<->GPU transfer time (ns) */
    uint64_t total_time_ns;              /**< Total operation time (ns) */
    float speedup;                       /**< Speedup vs CPU implementation */
    uint64_t memory_used;                /**< GPU memory consumed (bytes) */
} kg_gpu_result_t;

/**
 * @brief Cumulative GPU statistics
 *
 * WHAT: Aggregate statistics across all GPU operations
 * WHY:  Performance monitoring and optimization guidance
 * HOW:  Accumulated by GPU context over lifetime
 */
typedef struct {
    uint64_t total_operations;           /**< Total GPU operations executed */
    uint64_t total_gpu_time_ns;          /**< Total GPU execution time */
    uint64_t total_transfer_time_ns;     /**< Total transfer time */
    float avg_speedup;                   /**< Average speedup vs CPU */
    uint64_t peak_memory_used;           /**< Peak GPU memory usage */
    uint64_t current_memory_used;        /**< Current GPU memory usage */

    /* Per-target statistics */
    uint64_t analytics_operations;       /**< Graph analytics operations */
    uint64_t similarity_operations;      /**< Similarity search operations */
    uint64_t quantum_operations;         /**< Quantum simulation operations */
    uint64_t encryption_operations;      /**< Encryption operations */
} kg_gpu_stats_t;

/* ============================================================================
 * Data Structures - GPU Context
 * ============================================================================ */

/**
 * @brief GPU acceleration context
 *
 * WHAT: Main handle for GPU acceleration functionality
 * WHY:  Encapsulate GPU state, device selection, and configuration
 * HOW:  Created once, reused for all GPU operations
 */
typedef struct {
    kg_gpu_backend_t backend;            /**< Active GPU backend */
    kg_gpu_device_t* devices;            /**< Array of available devices */
    uint32_t device_count;               /**< Number of available devices */
    uint32_t active_device;              /**< Currently selected device index */
    kg_gpu_memory_config_t memory_config; /**< Memory configuration */
    kg_gpu_kernel_config_t kernel_config; /**< Kernel configuration */
    uint32_t enabled_targets;            /**< Bitmask of enabled acceleration targets */

    /* Statistics */
    uint64_t total_operations;           /**< Total operations executed */
    uint64_t total_gpu_time_ns;          /**< Total GPU time */
    float avg_speedup;                   /**< Rolling average speedup */

    /* Internal state (opaque) */
    void* internal;                      /**< Backend-specific internal state */
} kg_gpu_context_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create GPU acceleration context
 *
 * WHAT: Initialize GPU backend and enumerate available devices
 * WHY:  Prepare GPU for accelerated KG operations
 * HOW:  Probe for available backends, select best match, initialize
 *
 * @param preferred_backend Preferred GPU backend (KG_GPU_NONE = auto-select)
 * @return GPU context handle or NULL on failure
 *
 * @note Falls back to CPU-only (KG_GPU_NONE) if no GPU available
 */
kg_gpu_context_t* kg_gpu_create(kg_gpu_backend_t preferred_backend);

/**
 * @brief Destroy GPU acceleration context
 *
 * WHAT: Release all GPU resources and shutdown backend
 * WHY:  Clean resource deallocation
 * HOW:  Free GPU memory, destroy streams, cleanup backend
 *
 * @param gpu GPU context to destroy (NULL safe)
 */
void kg_gpu_destroy(kg_gpu_context_t* gpu);

/* ============================================================================
 * Device Management API
 * ============================================================================ */

/**
 * @brief Enumerate available GPU devices
 *
 * WHAT: List all GPU devices available for acceleration
 * WHY:  Device discovery for multi-GPU systems
 * HOW:  Query backend for device list and capabilities
 *
 * @param devices Output array for device info (caller allocated)
 * @param max Maximum devices to return
 * @param count Output: actual device count
 * @return 0 on success, -1 on error
 */
int kg_gpu_enumerate_devices(
    kg_gpu_device_t* devices,
    uint32_t max,
    uint32_t* count
);

/**
 * @brief Select active GPU device
 *
 * WHAT: Set the GPU device for subsequent operations
 * WHY:  Multi-GPU support, device migration
 * HOW:  Switch backend context to specified device
 *
 * @param gpu GPU context
 * @param device_id Device index to select
 * @return 0 on success, -1 if device invalid
 */
int kg_gpu_select_device(kg_gpu_context_t* gpu, uint32_t device_id);

/**
 * @brief Get current device information
 *
 * WHAT: Query active device capabilities and status
 * WHY:  Runtime capability checking, memory monitoring
 * HOW:  Copy device info from context, refresh memory stats
 *
 * @param gpu GPU context
 * @param info Output device info
 * @return 0 on success, -1 on error
 */
int kg_gpu_get_device_info(const kg_gpu_context_t* gpu, kg_gpu_device_t* info);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Configure GPU memory settings
 *
 * WHAT: Set memory pool and transfer configuration
 * WHY:  Optimize for workload (large graphs vs many small ops)
 * HOW:  Resize memory pool, configure transfer buffers
 *
 * @param gpu GPU context
 * @param config Memory configuration
 * @return 0 on success, -1 on error
 */
int kg_gpu_set_memory_config(
    kg_gpu_context_t* gpu,
    const kg_gpu_memory_config_t* config
);

/**
 * @brief Configure GPU kernel execution settings
 *
 * WHAT: Set kernel launch parameters
 * WHY:  Tune for specific hardware or workload
 * HOW:  Apply to context, affects subsequent operations
 *
 * @param gpu GPU context
 * @param config Kernel configuration
 * @return 0 on success, -1 on error
 */
int kg_gpu_set_kernel_config(
    kg_gpu_context_t* gpu,
    const kg_gpu_kernel_config_t* config
);

/**
 * @brief Enable GPU acceleration targets
 *
 * WHAT: Enable GPU for specific operation categories
 * WHY:  Selective GPU usage (e.g., GPU for analytics, CPU for small ops)
 * HOW:  OR bitmask into enabled_targets
 *
 * @param gpu GPU context
 * @param targets Bitmask of targets to enable (kg_gpu_target_t)
 * @return 0 on success
 */
int kg_gpu_enable_targets(kg_gpu_context_t* gpu, uint32_t targets);

/**
 * @brief Disable GPU acceleration targets
 *
 * WHAT: Disable GPU for specific operation categories
 * WHY:  Force CPU path for debugging or when GPU overhead not worth it
 * HOW:  AND NOT bitmask from enabled_targets
 *
 * @param gpu GPU context
 * @param targets Bitmask of targets to disable (kg_gpu_target_t)
 * @return 0 on success
 */
int kg_gpu_disable_targets(kg_gpu_context_t* gpu, uint32_t targets);

/* ============================================================================
 * GPU-Accelerated Graph Analytics
 * ============================================================================ */

/**
 * @brief Compute graph centrality metrics on GPU
 *
 * WHAT: Calculate node centrality (degree, betweenness, closeness, PageRank)
 * WHY:  Identify important nodes in large graphs (>50x speedup)
 * HOW:  GPU-parallel BFS/shortest paths, matrix operations
 *
 * @param gpu GPU context
 * @param kg Knowledge graph to analyze
 * @param metrics Output array for centrality metrics (caller allocated)
 * @param count Input: max metrics, Output: actual count
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_compute_centrality(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    kg_centrality_metrics_t* metrics,
    uint32_t* count,
    kg_gpu_result_t* result
);

/**
 * @brief Detect communities using GPU-accelerated Louvain algorithm
 *
 * WHAT: Find community structure via modularity optimization
 * WHY:  Identify module clusters, functional groups (>35x speedup)
 * HOW:  GPU-parallel Louvain with hierarchical coarsening
 *
 * @param gpu GPU context
 * @param kg Knowledge graph to analyze
 * @param communities Output array for detected communities
 * @param count Input: max communities, Output: actual count
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_detect_communities(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    kg_community_t* communities,
    uint32_t* count,
    kg_gpu_result_t* result
);

/**
 * @brief Compute PageRank on GPU
 *
 * WHAT: Calculate PageRank scores for all nodes
 * WHY:  Rank node importance based on connection structure
 * HOW:  GPU-parallel power iteration method
 *
 * @param gpu GPU context
 * @param kg Knowledge graph
 * @param ranks Output array for PageRank scores (one per node)
 * @param iterations Number of power iterations
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_compute_pagerank(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    float* ranks,
    uint32_t iterations,
    kg_gpu_result_t* result
);

/* ============================================================================
 * GPU-Accelerated Similarity Search
 * ============================================================================ */

/**
 * @brief Build similarity search index on GPU
 *
 * WHAT: Construct GPU-resident index for fast similarity search
 * WHY:  Pre-compute structure for repeated similarity queries
 * HOW:  Build KD-tree or IVF index on GPU memory
 *
 * @param gpu GPU context
 * @param kg Knowledge graph (uses node embeddings)
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_build_similarity_index(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    kg_gpu_result_t* result
);

/**
 * @brief Find k-nearest neighbors for a single node
 *
 * WHAT: Find k most similar nodes to a query node
 * WHY:  Similarity-based node lookup, recommendation
 * HOW:  GPU-parallel distance computation, top-k selection
 *
 * @param gpu GPU context
 * @param kg Knowledge graph
 * @param query Query node ID
 * @param k Number of neighbors to find
 * @param results Output array for similarity results
 * @param gpu_result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_find_similar(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    brain_kg_node_id_t query,
    uint32_t k,
    kg_similarity_result_t* results,
    kg_gpu_result_t* gpu_result
);

/**
 * @brief Batch k-nearest neighbors search
 *
 * WHAT: Find k-NN for multiple query nodes in parallel
 * WHY:  Efficient batch processing (>50x speedup vs sequential)
 * HOW:  GPU-parallel processing of all queries simultaneously
 *
 * @param gpu GPU context
 * @param kg Knowledge graph
 * @param queries Array of query node IDs
 * @param query_count Number of queries
 * @param k Neighbors per query
 * @param results Output: array of result arrays (caller allocated)
 * @param gpu_result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_batch_similarity(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    brain_kg_node_id_t* queries,
    uint32_t query_count,
    uint32_t k,
    kg_similarity_result_t** results,
    kg_gpu_result_t* gpu_result
);

/* ============================================================================
 * GPU-Accelerated Quantum Simulation
 * ============================================================================ */

/**
 * @brief Execute quantum walk on GPU
 *
 * WHAT: Simulate quantum walk for path finding
 * WHY:  Quadratic speedup over classical random walk
 * HOW:  GPU-parallel unitary evolution simulation
 *
 * @param gpu GPU context
 * @param kg Knowledge graph (defines walk topology)
 * @param start Start node for walk
 * @param target Target node (optional, BRAIN_KG_INVALID_NODE to explore)
 * @param steps Number of quantum walk steps
 * @param result Output quantum walk result (path, probabilities)
 * @param gpu_result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_quantum_walk(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    brain_kg_node_id_t start,
    brain_kg_node_id_t target,
    uint32_t steps,
    kg_quantum_walk_result_t* result,
    kg_gpu_result_t* gpu_result
);

/**
 * @brief GPU-accelerated quantum annealing optimization
 *
 * WHAT: Solve optimization problems via simulated quantum annealing
 * WHY:  Find near-optimal solutions for NP-hard graph problems
 * HOW:  GPU-parallel thermal/quantum Monte Carlo sampling
 *
 * @param gpu GPU context
 * @param kg Knowledge graph
 * @param optimization_problem Problem specification string
 * @param solution Output solution buffer
 * @param gpu_result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_quantum_annealing(
    kg_gpu_context_t* gpu,
    const brain_kg_t* kg,
    const char* optimization_problem,
    void* solution,
    kg_gpu_result_t* gpu_result
);

/* ============================================================================
 * GPU-Accelerated Weight Operations
 * ============================================================================ */

/**
 * @brief Compute weight statistics on GPU
 *
 * WHAT: Calculate statistical properties of weight tensor
 * WHY:  Fast analysis of large neural network weights (>60x speedup)
 * HOW:  GPU-parallel reduction operations
 *
 * @param gpu GPU context
 * @param weights Weight data buffer
 * @param weight_count Number of weights
 * @param stats Output weight statistics
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_compute_weight_stats(
    kg_gpu_context_t* gpu,
    const void* weights,
    size_t weight_count,
    kg_weight_stats_t* stats,
    kg_gpu_result_t* result
);

/**
 * @brief Compute weight tensor difference on GPU
 *
 * WHAT: Calculate difference metrics between two weight tensors
 * WHY:  Track weight changes, compare model snapshots
 * HOW:  GPU-parallel elementwise diff and reduction
 *
 * @param gpu GPU context
 * @param weights_a First weight tensor
 * @param weights_b Second weight tensor
 * @param count Number of weights in each tensor
 * @param diff Output: difference metric (L2 norm, etc.)
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error
 */
int kg_gpu_weight_diff(
    kg_gpu_context_t* gpu,
    const void* weights_a,
    const void* weights_b,
    size_t count,
    float* diff,
    kg_gpu_result_t* result
);

/* ============================================================================
 * GPU-Accelerated Encryption (Hardware-Dependent)
 * ============================================================================ */

/**
 * @brief Batch encryption on GPU
 *
 * WHAT: Encrypt multiple data buffers using GPU acceleration
 * WHY:  High-throughput encryption for large datasets (>30x speedup)
 * HOW:  GPU-parallel AES using hardware units (if available)
 *
 * @param gpu GPU context
 * @param plaintexts Array of plaintext buffer pointers
 * @param sizes Array of buffer sizes
 * @param count Number of buffers to encrypt
 * @param ciphertexts Output array of ciphertext buffer pointers
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error or if GPU encryption not supported
 */
int kg_gpu_encrypt_batch(
    kg_gpu_context_t* gpu,
    const void** plaintexts,
    size_t* sizes,
    uint32_t count,
    void** ciphertexts,
    kg_gpu_result_t* result
);

/**
 * @brief Batch decryption on GPU
 *
 * WHAT: Decrypt multiple data buffers using GPU acceleration
 * WHY:  High-throughput decryption for large datasets
 * HOW:  GPU-parallel AES decryption using hardware units
 *
 * @param gpu GPU context
 * @param ciphertexts Array of ciphertext buffer pointers
 * @param sizes Array of buffer sizes
 * @param count Number of buffers to decrypt
 * @param plaintexts Output array of plaintext buffer pointers
 * @param result Output GPU operation result
 * @return 0 on success, -1 on error or if GPU decryption not supported
 */
int kg_gpu_decrypt_batch(
    kg_gpu_context_t* gpu,
    const void** ciphertexts,
    size_t* sizes,
    uint32_t count,
    void** plaintexts,
    kg_gpu_result_t* result
);

/* ============================================================================
 * Memory Management API
 * ============================================================================ */

/**
 * @brief Allocate GPU memory
 *
 * WHAT: Allocate memory on the active GPU device
 * WHY:  Manual control of GPU memory for custom operations
 * HOW:  Backend-specific allocation (cudaMalloc, etc.)
 *
 * @param gpu GPU context
 * @param size Bytes to allocate
 * @param gpu_ptr Output: pointer to GPU memory
 * @return 0 on success, -1 on error (out of memory, etc.)
 */
int kg_gpu_allocate(kg_gpu_context_t* gpu, size_t size, void** gpu_ptr);

/**
 * @brief Free GPU memory
 *
 * WHAT: Release GPU memory allocation
 * WHY:  Clean resource deallocation
 * HOW:  Backend-specific free (cudaFree, etc.)
 *
 * @param gpu GPU context
 * @param gpu_ptr GPU memory pointer to free
 * @return 0 on success, -1 on error
 */
int kg_gpu_free(kg_gpu_context_t* gpu, void* gpu_ptr);

/**
 * @brief Copy data from CPU to GPU
 *
 * WHAT: Transfer data from host memory to GPU memory
 * WHY:  Prepare data for GPU processing
 * HOW:  Backend-specific memcpy (cudaMemcpy H2D, etc.)
 *
 * @param gpu GPU context
 * @param host_ptr Source CPU memory
 * @param gpu_ptr Destination GPU memory
 * @param size Bytes to copy
 * @return 0 on success, -1 on error
 */
int kg_gpu_copy_to_device(
    kg_gpu_context_t* gpu,
    const void* host_ptr,
    void* gpu_ptr,
    size_t size
);

/**
 * @brief Copy data from GPU to CPU
 *
 * WHAT: Transfer data from GPU memory to host memory
 * WHY:  Retrieve results from GPU processing
 * HOW:  Backend-specific memcpy (cudaMemcpy D2H, etc.)
 *
 * @param gpu GPU context
 * @param gpu_ptr Source GPU memory
 * @param host_ptr Destination CPU memory
 * @param size Bytes to copy
 * @return 0 on success, -1 on error
 */
int kg_gpu_copy_to_host(
    kg_gpu_context_t* gpu,
    const void* gpu_ptr,
    void* host_ptr,
    size_t size
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get cumulative GPU statistics
 *
 * WHAT: Retrieve aggregate performance statistics
 * WHY:  Performance monitoring, optimization guidance
 * HOW:  Copy accumulated stats from context
 *
 * @param gpu GPU context
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int kg_gpu_get_stats(const kg_gpu_context_t* gpu, kg_gpu_stats_t* stats);

/**
 * @brief Reset GPU statistics
 *
 * WHAT: Clear accumulated statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat fields in context
 *
 * @param gpu GPU context
 */
void kg_gpu_reset_stats(kg_gpu_context_t* gpu);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert GPU backend to string
 *
 * @param backend GPU backend enum value
 * @return Static string name (e.g., "CUDA", "ROCm")
 */
const char* kg_gpu_backend_to_string(kg_gpu_backend_t backend);

/**
 * @brief Convert GPU target to string
 *
 * @param target GPU target enum value
 * @return Static string name (e.g., "GRAPH_ANALYTICS")
 */
const char* kg_gpu_target_to_string(kg_gpu_target_t target);

/**
 * @brief Check if GPU acceleration is available
 *
 * @param gpu GPU context (can be NULL to check system-wide)
 * @return true if GPU acceleration available, false otherwise
 */
bool kg_gpu_is_available(const kg_gpu_context_t* gpu);

/**
 * @brief Check if specific target is GPU-accelerated
 *
 * @param gpu GPU context
 * @param target Target to check
 * @return true if target is GPU-enabled, false otherwise
 */
bool kg_gpu_target_enabled(const kg_gpu_context_t* gpu, kg_gpu_target_t target);

/**
 * @brief Get default memory configuration
 *
 * @param config Output configuration with defaults
 */
void kg_gpu_default_memory_config(kg_gpu_memory_config_t* config);

/**
 * @brief Get default kernel configuration
 *
 * @param config Output configuration with defaults
 */
void kg_gpu_default_kernel_config(kg_gpu_kernel_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_GPU_H */
