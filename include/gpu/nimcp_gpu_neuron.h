/**
 * @file nimcp_gpu_neuron.h
 * @brief GPU P2P Neuron Implementation (CUDA/ROCm)
 *
 * WHAT: P2P neuron computation on GPU with biological message-passing
 * WHY:  Scale to millions of neurons using GPU parallelism
 * HOW:  Each CUDA thread = 1 neuron, spike events via shared memory queues
 *
 * ARCHITECTURE:
 *
 *   ┌─────────────────────────────────────────────┐
 *   │          GPU Global Memory                   │
 *   │  ┌──────────┐  ┌─────────┐  ┌─────────────┐│
 *   │  │ Neurons  │  │ Spikes  │  │  Synapses   ││
 *   │  │  Array   │  │  Queue  │  │  Adjacency  ││
 *   │  └──────────┘  └─────────┘  └─────────────┘│
 *   └─────────────────────────────────────────────┘
 *            │              │              │
 *   ┌────────┴──────────────┴──────────────┴──────┐
 *   │         GPU Kernel Execution                 │
 *   │  Thread 0     Thread 1    ...    Thread N-1  │
 *   │  [Neuron 0]   [Neuron 1]  ...    [Neuron N]  │
 *   │     │             │                   │       │
 *   │  Read Spikes  Read Spikes        Read Spikes │
 *   │  Compute V    Compute V          Compute V   │
 *   │  Write Spike  Write Spike        Write Spike │
 *   └──────────────────────────────────────────────┘
 *
 * BIOLOGICAL MAPPING:
 * - CUDA Thread → Individual Neuron (independent agent)
 * - Shared Memory Queue → Synaptic transmission
 * - Global Memory → Long-term connectivity structure
 * - Atomic Operations → Neurotransmitter release
 *
 * PERFORMANCE:
 * - NVIDIA RTX 4090: 16,384 CUDA cores → 16K neurons/kernel
 * - Each neuron processes independently (true parallelism)
 * - Coalesced memory access for spike queues
 * - Lock-free atomic operations for spike events
 *
 * CONDITIONAL COMPILATION:
 * - Compiles with or without CUDA installed
 * - GPU code only compiled when CUDA available
 * - CPU fallback always available
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.8.0 (Tensor-based GPU)
 */

#ifndef NIMCP_GPU_NEURON_H
#define NIMCP_GPU_NEURON_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "gpu/nimcp_spike_event.h"
#include "gpu/nimcp_execution_mode.h"

// Conditional CUDA support
#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#define NIMCP_DEVICE __device__
#define NIMCP_HOST __host__
#define NIMCP_GLOBAL __global__
#define NIMCP_SHARED __shared__
#else
#define NIMCP_DEVICE
#define NIMCP_HOST
#define NIMCP_GLOBAL
#define NIMCP_SHARED
#endif

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// GPU Neuron State (Compact for Cache Efficiency)
//=============================================================================

/**
 * @brief GPU neuron state - compact layout for coalesced access
 *
 * LAYOUT: 64 bytes total (fits in single cache line)
 * ALIGNMENT: 64-byte aligned for optimal GPU memory access
 *
 * DESIGN DECISIONS:
 * - float precision (vs double) - 2x faster on consumer GPUs
 * - Compact state - minimize global memory bandwidth
 * - No pointers - use indices for GPU compatibility
 */
typedef struct __attribute__((aligned(64))) {
    // Core state (16 bytes)
    float membrane_potential;    /**< Current V_m (mV) */
    float threshold;             /**< Spike threshold */
    float state;                 /**< Activation state */
    float bias;                  /**< Intrinsic bias */

    // Temporal dynamics (16 bytes)
    uint64_t last_spike;         /**< Last spike time (μs) */
    float calcium_concentration; /**< [Ca2+] concentration */
    float synaptic_trace;        /**< STDP trace */

    // Connectivity (16 bytes)
    uint32_t neuron_id;          /**< Global neuron ID */
    uint32_t synapse_offset;     /**< Index into synapse array */
    uint32_t num_incoming;       /**< Number of incoming synapses */
    uint32_t num_outgoing;       /**< Number of outgoing synapses */

    // Learning params (16 bytes)
    float learning_rate;         /**< STDP learning rate */
    float firing_rate;           /**< Instantaneous rate (Hz) */
    uint32_t spike_count;        /**< Total spikes emitted */
    uint32_t refractory_period;  /**< Refractory period (μs) */
} gpu_neuron_state_t;

/**
 * @brief GPU synapse structure - lightweight for GPU
 *
 * LAYOUT: 16 bytes (4 floats)
 * ALIGNMENT: 16-byte aligned
 */
typedef struct __attribute__((aligned(16))) {
    uint32_t source_id;          /**< Pre-synaptic neuron ID */
    uint32_t target_id;          /**< Post-synaptic neuron ID */
    float weight;                /**< Synaptic weight */
    float strength;              /**< Synaptic strength */
} gpu_synapse_t;

//=============================================================================
// GPU Neural Network Structure
//=============================================================================

/**
 * @brief GPU neural network (opaque handle)
 *
 * TENSOR-BASED ARCHITECTURE (v2.8.0):
 * - Neuron states: nimcp_gpu_tensor_t [N_neurons x 8] (float32)
 * - Neuron metadata: nimcp_gpu_tensor_t [N_neurons x 8] (uint32)
 * - Synapse data: nimcp_gpu_tensor_t [N_synapses x 4] (float32)
 * - Spike queue: nimcp_gpu_tensor_t [capacity x 4] (float32)
 *
 * CONTAINS:
 * - Neuron states (GPU tensor memory)
 * - Synapse connectivity (GPU tensor memory)
 * - Spike event queues (GPU tensor memory)
 * - GPU context for stream/device management
 * - Host-side copies for CPU access
 *
 * BENEFITS:
 * - Better memory coalescing on GPU
 * - Unified tensor API for memory management
 * - Enables batch operations via cuBLAS
 * - Consistent with nimcp_tensor_gpu.h API
 */
typedef struct gpu_neural_network_struct* gpu_neural_network_t;

/**
 * @brief GPU network configuration
 */
typedef struct {
    uint32_t num_neurons;        /**< Number of neurons */
    uint32_t num_synapses;       /**< Total synapses */

    // GPU kernel configuration
    uint32_t threads_per_block;  /**< CUDA threads per block */
    uint32_t max_blocks;         /**< Maximum number of blocks */

    // Memory configuration
    uint64_t spike_queue_capacity; /**< Spike queue size */
    bool use_unified_memory;     /**< Use CUDA unified memory */
    bool pin_host_memory;        /**< Pin CPU memory */

    // Execution mode
    execution_mode_t exec_mode;  /**< CPU/GPU/Hybrid */

    // Learning configuration
    bool enable_stdp;            /**< Enable STDP learning */
    bool enable_bcm;             /**< Enable BCM plasticity */
    float global_learning_rate;  /**< Global learning rate */
} gpu_network_config_t;

//=============================================================================
// GPU Network Lifecycle
//=============================================================================

/**
 * @brief Create GPU neural network
 *
 * @param config Network configuration
 * @return Network handle, or NULL on failure
 *
 * BEHAVIOR:
 * - Allocates GPU memory for neurons/synapses
 * - Creates spike event queues
 * - Initializes CUDA context
 * - Falls back to CPU if GPU unavailable
 *
 * THREAD SAFETY: Not thread-safe (call from single thread)
 */
NIMCP_EXPORT gpu_neural_network_t gpu_neural_network_create(
    const gpu_network_config_t* config
);

/**
 * @brief Destroy GPU neural network
 *
 * @param network Network handle (NULL-safe)
 *
 * CLEANUP:
 * - Synchronizes GPU
 * - Frees GPU memory
 * - Destroys CUDA context
 */
NIMCP_EXPORT void gpu_neural_network_destroy(gpu_neural_network_t network);

//=============================================================================
// Neuron Operations
//=============================================================================

/**
 * @brief Add neuron to GPU network
 *
 * @param network Network handle
 * @param initial_state Initial neuron state
 * @return Neuron ID, or UINT32_MAX on failure
 */
NIMCP_EXPORT uint32_t gpu_neural_network_add_neuron(
    gpu_neural_network_t network,
    const gpu_neuron_state_t* initial_state
);

/**
 * @brief Add synapse between neurons
 *
 * @param network Network handle
 * @param source_id Source neuron ID
 * @param target_id Target neuron ID
 * @param weight Initial synaptic weight
 * @param strength Initial synaptic strength
 * @return true on success
 */
NIMCP_EXPORT bool gpu_neural_network_add_synapse(
    gpu_neural_network_t network,
    uint32_t source_id,
    uint32_t target_id,
    float weight,
    float strength
);

//=============================================================================
// Simulation (GPU Kernel Launch)
//=============================================================================

/**
 * @brief Update all neurons for one timestep (GPU kernel)
 *
 * @param network Network handle
 * @param timestamp Current simulation time (μs)
 * @param delta_t Timestep duration (μs)
 * @return Number of neurons that spiked
 *
 * BEHAVIOR:
 * - Launches GPU kernel (1 thread per neuron)
 * - Each neuron reads incoming spikes
 * - Computes membrane potential
 * - Fires spike if threshold crossed
 * - Asynchronous (non-blocking) execution
 *
 * PERFORMANCE:
 * - GPU: ~100μs for 10K neurons
 * - CPU: ~10ms for 10K neurons
 * - Speedup: ~100x
 */
NIMCP_EXPORT uint32_t gpu_neural_network_update(
    gpu_neural_network_t network,
    uint64_t timestamp,
    uint64_t delta_t
);

/**
 * @brief Apply STDP learning (GPU kernel)
 *
 * @param network Network handle
 * @param timestamp Current time
 * @return Number of synapses modified
 *
 * ALGORITHM:
 * - Each thread processes one neuron's synapses
 * - Compares pre/post spike times from spike trains
 * - Updates weights based on Δt
 * - Normalizes weights
 */
NIMCP_EXPORT uint32_t gpu_neural_network_apply_stdp(
    gpu_neural_network_t network,
    uint64_t timestamp
);

/**
 * @brief Synchronize GPU execution (wait for completion)
 *
 * @param network Network handle
 * @return true on success
 *
 * BEHAVIOR: Calls cudaDeviceSynchronize() if GPU-enabled
 */
NIMCP_EXPORT bool gpu_neural_network_synchronize(gpu_neural_network_t network);

//=============================================================================
// Data Access (Host ↔ Device Transfers)
//=============================================================================

/**
 * @brief Get neuron state (copy from GPU to CPU)
 *
 * @param network Network handle
 * @param neuron_id Neuron ID
 * @param state Output neuron state
 * @return true on success
 */
NIMCP_EXPORT bool gpu_neural_network_get_neuron_state(
    gpu_neural_network_t network,
    uint32_t neuron_id,
    gpu_neuron_state_t* state
);

/**
 * @brief Set neuron state (copy from CPU to GPU)
 *
 * @param network Network handle
 * @param neuron_id Neuron ID
 * @param state Input neuron state
 * @return true on success
 */
NIMCP_EXPORT bool gpu_neural_network_set_neuron_state(
    gpu_neural_network_t network,
    uint32_t neuron_id,
    const gpu_neuron_state_t* state
);

/**
 * @brief Get all neuron states (batch transfer)
 *
 * @param network Network handle
 * @param states Output array (must be pre-allocated)
 * @param max_neurons Maximum neurons to copy
 * @return Number of neurons copied
 */
NIMCP_EXPORT uint32_t gpu_neural_network_get_all_states(
    gpu_neural_network_t network,
    gpu_neuron_state_t* states,
    uint32_t max_neurons
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get network statistics
 *
 * @param network Network handle
 * @param total_spikes Output: total spikes emitted
 * @param avg_firing_rate Output: average firing rate (Hz)
 * @param gpu_memory_used Output: GPU memory used (bytes)
 * @return true on success
 */
NIMCP_EXPORT bool gpu_neural_network_get_stats(
    gpu_neural_network_t network,
    uint64_t* total_spikes,
    float* avg_firing_rate,
    uint64_t* gpu_memory_used
);

//=============================================================================
// GPU Kernel Declarations (Compiled Only with CUDA)
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief GPU kernel: Update all neurons
 *
 * LAUNCH CONFIG: <<<blocks, threads_per_block>>>
 * - blocks: ceil(num_neurons / threads_per_block)
 * - threads_per_block: 256 (optimal for most GPUs)
 */
NIMCP_GLOBAL void kernel_update_neurons(
    gpu_neuron_state_t* neurons,
    gpu_synapse_t* synapses,
    spike_event_t* spike_queue,
    uint32_t num_neurons,
    uint64_t timestamp,
    uint64_t delta_t
);

/**
 * @brief GPU kernel: Apply STDP learning
 *
 * LAUNCH CONFIG: <<<blocks, threads_per_block>>>
 */
NIMCP_GLOBAL void kernel_apply_stdp(
    gpu_neuron_state_t* neurons,
    gpu_synapse_t* synapses,
    spike_train_t* spike_trains,
    uint32_t num_neurons,
    uint64_t timestamp,
    float learning_rate
);

/**
 * @brief GPU device function: Compute membrane potential
 *
 * DEVICE ONLY: Called from GPU kernels
 */
NIMCP_DEVICE float gpu_compute_membrane_potential(
    const gpu_neuron_state_t* neuron,
    const gpu_synapse_t* synapses,
    const gpu_neuron_state_t* all_neurons,
    uint32_t num_incoming
);

#endif // NIMCP_ENABLE_CUDA

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if GPU is available
 *
 * @return true if CUDA/ROCm GPU available
 */
NIMCP_EXPORT bool gpu_is_available(void);

/**
 * @brief Get GPU device count
 *
 * @return Number of GPUs, or 0 if none
 */
NIMCP_EXPORT uint32_t gpu_get_device_count(void);

/**
 * @brief Get GPU device name
 *
 * @param device_id GPU device ID
 * @param name Output buffer for device name
 * @param max_len Maximum buffer length
 * @return true on success
 */
NIMCP_EXPORT bool gpu_get_device_name(uint32_t device_id, char* name, size_t max_len);

/**
 * @brief Get optimal GPU configuration for network size
 *
 * @param num_neurons Number of neurons
 * @return Optimal GPU configuration
 */
NIMCP_EXPORT gpu_network_config_t gpu_get_optimal_config(uint32_t num_neurons);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GPU_NEURON_H
