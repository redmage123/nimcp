/**
 * @file nimcp_axon_gpu.h
 * @brief GPU-Accelerated Axon Module API
 *
 * WHAT: C API for GPU-accelerated axon signal propagation
 * WHY:  GPU acceleration for large-scale axon network simulation
 * HOW:  Wraps CUDA kernels for parallel signal propagation, velocity, and myelination
 *
 * BIOLOGICAL BASIS:
 * =================
 * Axons transmit action potentials from neuron soma to synaptic terminals:
 * - Signal propagation: Parallel processing of many simultaneous spikes
 * - Conduction velocity: Depends on diameter and myelination level
 * - Myelination effects: Saltatory conduction speeds up transmission
 * - Refractory periods: Temporal dynamics of spike generation
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Large neural networks contain millions of axons with:
 * - Simultaneous spike propagation across thousands of axons
 * - Parallel velocity calculations based on myelination
 * - Batch updates for refractory states and activity tracking
 * - Tensor-based data layout for coalesced memory access
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * axon_gpu_context_t* axon_gpu = axon_gpu_create(ctx, &config);
 *
 * // Initialize axon tensors
 * axon_gpu_init_tensors(axon_gpu, num_axons, num_segments);
 *
 * // Batch signal propagation
 * axon_gpu_propagate(axon_gpu, dt);
 *
 * // Update velocities based on myelination
 * axon_gpu_update_velocities(axon_gpu);
 *
 * axon_gpu_destroy(axon_gpu);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_AXON_GPU_H
#define NIMCP_AXON_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

/** @brief Opaque GPU Axon context */
typedef struct axon_gpu_context axon_gpu_context_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief GPU Axon configuration
 */
typedef struct {
    uint32_t max_axons;              /**< Maximum number of axons */
    uint32_t max_segments;           /**< Maximum segments per axon */
    uint32_t max_batch_size;         /**< Maximum batch size for operations */
    bool enable_async_transfer;      /**< Enable async CPU-GPU transfers */
    bool enable_biophysics;          /**< Enable enhanced biophysics model */
    float refractory_period_ms;      /**< Default refractory period (ms) */
    float base_velocity_ms;          /**< Base conduction velocity (m/s) */
    float myelin_multiplier;         /**< Velocity multiplier for myelinated axons */
} axon_gpu_config_t;

/**
 * @brief Get default GPU Axon configuration
 */
NIMCP_EXPORT axon_gpu_config_t axon_gpu_default_config(void);

//=============================================================================
// GPU Axon Context Structure (Tensor-based)
//=============================================================================

/**
 * @brief Internal GPU axon context with tensor storage
 *
 * WHAT: All axon data stored in GPU tensors for parallel processing
 * WHY:  Enable efficient batched operations on GPU
 * HOW:  Tensors organized for coalesced memory access patterns
 */
struct axon_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;    /**< GPU context */
    axon_gpu_config_t config;        /**< Configuration */

    //--- Core Tensor Storage ---
    nimcp_gpu_tensor_t* positions;   /**< [N_axons, N_segments] - position along axon */
    nimcp_gpu_tensor_t* velocities;  /**< [N_axons] - conduction velocity per axon */
    nimcp_gpu_tensor_t* myelination; /**< [N_axons, N_segments] - myelin thickness */
    nimcp_gpu_tensor_t* signals;     /**< [N_axons, N_segments] - signal amplitude */
    nimcp_gpu_tensor_t* delays;      /**< [N_axons] - propagation delay */
    nimcp_gpu_tensor_t* refractory;  /**< [N_axons] - refractory state (time remaining) */

    //--- Auxiliary Tensors ---
    nimcp_gpu_tensor_t* diameters;   /**< [N_axons] - axon diameter (um) */
    nimcp_gpu_tensor_t* lengths;     /**< [N_axons] - axon length (um) */
    nimcp_gpu_tensor_t* active;      /**< [N_axons] - active spike flag (bool as uint8) */
    nimcp_gpu_tensor_t* spike_times; /**< [N_axons] - last spike initiation time */
    nimcp_gpu_tensor_t* atp_levels;  /**< [N_axons] - metabolic ATP level (0-1) */

    //--- Segment-level Tensors ---
    nimcp_gpu_tensor_t* seg_lengths;     /**< [N_axons, N_segments] - segment length */
    nimcp_gpu_tensor_t* seg_velocities;  /**< [N_axons, N_segments] - local velocity */
    nimcp_gpu_tensor_t* seg_delays;      /**< [N_axons, N_segments] - cumulative delay */

    //--- Connectivity ---
    nimcp_gpu_tensor_t* source_neurons;  /**< [N_axons] - source neuron IDs */
    nimcp_gpu_tensor_t* target_synapses; /**< [N_axons] - target synapse IDs */

    //--- Dimensions ---
    uint32_t num_axons;              /**< Current number of axons */
    uint32_t num_segments;           /**< Segments per axon (uniform) */

    //--- CUDA Stream ---
    void* stream;                    /**< CUDA stream (cudaStream_t) */

    //--- Statistics ---
    uint64_t total_spikes;           /**< Total spikes propagated */
    uint64_t total_updates;          /**< Total update calls */
    float avg_propagation_time_us;   /**< Average propagation kernel time */
    float avg_velocity_time_us;      /**< Average velocity update time */
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU Axon context
 *
 * WHAT: Initialize GPU resources for axon acceleration
 * WHY:  Allocate GPU memory for tensor storage
 * HOW:  Create CUDA streams, prepare for tensor allocation
 *
 * @param gpu_ctx GPU context (required)
 * @param config Configuration (NULL for defaults)
 * @return New GPU Axon context, or NULL on failure
 */
NIMCP_EXPORT axon_gpu_context_t* axon_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const axon_gpu_config_t* config
);

/**
 * @brief Destroy GPU Axon context
 *
 * @param ctx GPU Axon context to destroy
 */
NIMCP_EXPORT void axon_gpu_destroy(axon_gpu_context_t* ctx);

/**
 * @brief Synchronize GPU operations
 *
 * @param ctx GPU Axon context
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_synchronize(axon_gpu_context_t* ctx);

//=============================================================================
// Tensor Initialization
//=============================================================================

/**
 * @brief Initialize GPU tensors for axon network
 *
 * WHAT: Allocate and initialize all required tensors
 * WHY:  Prepare GPU memory for axon simulation
 * HOW:  Creates tensors with appropriate dimensions and precision
 *
 * @param ctx GPU Axon context
 * @param num_axons Number of axons in network
 * @param num_segments Number of segments per axon
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_init_tensors(
    axon_gpu_context_t* ctx,
    uint32_t num_axons,
    uint32_t num_segments
);

/**
 * @brief Upload axon properties from CPU
 *
 * WHAT: Transfer axon data from CPU to GPU tensors
 * WHY:  Initialize GPU state from CPU-side axon network
 * HOW:  Batch upload of diameters, lengths, myelination levels
 *
 * @param ctx GPU Axon context
 * @param diameters Array of axon diameters [num_axons]
 * @param lengths Array of axon lengths [num_axons]
 * @param myelination Array of myelination levels [num_axons * num_segments]
 * @param num_axons Number of axons
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_upload_properties(
    axon_gpu_context_t* ctx,
    const float* diameters,
    const float* lengths,
    const float* myelination,
    uint32_t num_axons
);

/**
 * @brief Upload connectivity information
 *
 * @param ctx GPU Axon context
 * @param source_neurons Array of source neuron IDs [num_axons]
 * @param target_synapses Array of target synapse IDs [num_axons]
 * @param num_axons Number of axons
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_upload_connectivity(
    axon_gpu_context_t* ctx,
    const uint32_t* source_neurons,
    const uint32_t* target_synapses,
    uint32_t num_axons
);

//=============================================================================
// Signal Propagation (GPU-Accelerated)
//=============================================================================

/**
 * @brief Propagate signals along all axons (GPU kernel)
 *
 * WHAT: Advance signal positions based on local velocities
 * WHY:  Core simulation step for action potential propagation
 * HOW:  CUDA kernel updates all axon signals in parallel
 *
 * CUDA Kernel: kernel_axon_propagate
 * - Each thread processes one axon
 * - Updates signal positions based on segment velocities
 * - Handles segment boundaries and cumulative delays
 *
 * @param ctx GPU Axon context
 * @param dt Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_propagate(
    axon_gpu_context_t* ctx,
    float dt
);

/**
 * @brief Initiate spikes on specified axons (GPU batch)
 *
 * WHAT: Start action potentials on multiple axons simultaneously
 * WHY:  Batch spike initiation for efficiency
 * HOW:  GPU kernel sets initial signal state and refractory period
 *
 * @param ctx GPU Axon context
 * @param axon_indices Array of axon indices to initiate
 * @param amplitudes Array of spike amplitudes (NULL for default 1.0)
 * @param count Number of axons to initiate
 * @param current_time Current simulation time (microseconds)
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_initiate_spikes(
    axon_gpu_context_t* ctx,
    const uint32_t* axon_indices,
    const float* amplitudes,
    uint32_t count,
    uint64_t current_time
);

/**
 * @brief Check for spike arrivals (GPU kernel)
 *
 * WHAT: Identify axons where spikes have reached terminals
 * WHY:  Trigger synaptic transmission when spikes arrive
 * HOW:  GPU kernel checks cumulative delay vs elapsed time
 *
 * @param ctx GPU Axon context
 * @param current_time Current simulation time (microseconds)
 * @param arrived_indices Output array for arrived axon indices
 * @param max_arrivals Maximum arrivals to return
 * @param arrival_count Output: actual number of arrivals
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_check_arrivals(
    axon_gpu_context_t* ctx,
    uint64_t current_time,
    uint32_t* arrived_indices,
    uint32_t max_arrivals,
    uint32_t* arrival_count
);

//=============================================================================
// Velocity and Myelination (GPU-Accelerated)
//=============================================================================

/**
 * @brief Update conduction velocities (GPU kernel)
 *
 * WHAT: Recalculate velocities based on myelination and diameter
 * WHY:  Velocities change when myelination levels change
 * HOW:  CUDA kernel applies Hursh's law and myelin multiplier
 *
 * CUDA Kernel: kernel_axon_velocity_update
 * - Batch velocity calculations for all axons
 * - Unmyelinated: v = k * sqrt(diameter)
 * - Myelinated: v = k * diameter * myelin_level * multiplier
 *
 * @param ctx GPU Axon context
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_update_velocities(
    axon_gpu_context_t* ctx
);

/**
 * @brief Apply myelination effects (GPU kernel)
 *
 * WHAT: Compute velocity speedup from myelin thickness
 * WHY:  Myelination increases conduction velocity significantly
 * HOW:  GPU kernel multiplies base velocity by myelin factor
 *
 * CUDA Kernel: kernel_axon_myelination_effect
 * - Element-wise multiplication of velocity by myelin factor
 * - Clamps to maximum biological velocity
 *
 * @param ctx GPU Axon context
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_apply_myelination(
    axon_gpu_context_t* ctx
);

/**
 * @brief Update myelination levels (GPU batch)
 *
 * WHAT: Set new myelination levels for specified axons/segments
 * WHY:  Activity-dependent myelination changes over time
 * HOW:  GPU kernel updates myelination tensor
 *
 * @param ctx GPU Axon context
 * @param axon_indices Axon indices to update
 * @param segment_indices Segment indices (NULL for all segments)
 * @param new_myelination New myelination values
 * @param count Number of updates
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_update_myelination(
    axon_gpu_context_t* ctx,
    const uint32_t* axon_indices,
    const uint32_t* segment_indices,
    const float* new_myelination,
    uint32_t count
);

//=============================================================================
// Refractory Period Management (GPU)
//=============================================================================

/**
 * @brief Update refractory states (GPU kernel)
 *
 * WHAT: Decay refractory timers for all axons
 * WHY:  Axons become available to fire again after refractory period
 * HOW:  GPU kernel subtracts dt from refractory timers
 *
 * @param ctx GPU Axon context
 * @param dt Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_update_refractory(
    axon_gpu_context_t* ctx,
    float dt
);

/**
 * @brief Check which axons are available to fire
 *
 * WHAT: Get bitmap of non-refractory axons
 * WHY:  Only non-refractory axons can initiate new spikes
 * HOW:  GPU kernel generates availability mask
 *
 * @param ctx GPU Axon context
 * @param available Output array (1 = available, 0 = refractory)
 * @param num_axons Number of axons
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_get_available(
    axon_gpu_context_t* ctx,
    uint8_t* available,
    uint32_t num_axons
);

//=============================================================================
// Activity and Metabolic State (GPU)
//=============================================================================

/**
 * @brief Update activity tracking (GPU kernel)
 *
 * WHAT: Update firing rate estimates and activity EMA
 * WHY:  Track activity for plasticity and myelination signals
 * HOW:  GPU kernel computes exponential moving average
 *
 * @param ctx GPU Axon context
 * @param dt Time step in milliseconds
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_update_activity(
    axon_gpu_context_t* ctx,
    float dt
);

/**
 * @brief Update ATP levels (GPU kernel)
 *
 * WHAT: Regenerate ATP and consume during spikes
 * WHY:  Metabolic state affects spike reliability
 * HOW:  GPU kernel applies regeneration and consumption
 *
 * @param ctx GPU Axon context
 * @param dt Time step in milliseconds
 * @param regeneration_rate ATP regeneration rate per ms
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_update_atp(
    axon_gpu_context_t* ctx,
    float dt,
    float regeneration_rate
);

//=============================================================================
// Batch Simulation Step
//=============================================================================

/**
 * @brief Perform complete simulation step (GPU)
 *
 * WHAT: Execute all per-timestep operations in one call
 * WHY:  Minimize CPU-GPU synchronization overhead
 * HOW:  Fused kernel execution for propagation, refractory, activity
 *
 * Equivalent to:
 *   axon_gpu_propagate(ctx, dt);
 *   axon_gpu_update_refractory(ctx, dt);
 *   axon_gpu_update_activity(ctx, dt);
 *   axon_gpu_update_atp(ctx, dt, regen_rate);
 *
 * @param ctx GPU Axon context
 * @param dt Time step in milliseconds
 * @param current_time Current simulation time (microseconds)
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_step(
    axon_gpu_context_t* ctx,
    float dt,
    uint64_t current_time
);

//=============================================================================
// Data Retrieval (GPU to CPU)
//=============================================================================

/**
 * @brief Download velocities to CPU
 *
 * @param ctx GPU Axon context
 * @param velocities Output array [num_axons]
 * @param num_axons Number of velocities to download
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_get_velocities(
    axon_gpu_context_t* ctx,
    float* velocities,
    uint32_t num_axons
);

/**
 * @brief Download delays to CPU
 *
 * @param ctx GPU Axon context
 * @param delays Output array [num_axons]
 * @param num_axons Number of delays to download
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_get_delays(
    axon_gpu_context_t* ctx,
    float* delays,
    uint32_t num_axons
);

/**
 * @brief Download myelination levels to CPU
 *
 * @param ctx GPU Axon context
 * @param myelination Output array [num_axons * num_segments]
 * @param num_axons Number of axons
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_get_myelination(
    axon_gpu_context_t* ctx,
    float* myelination,
    uint32_t num_axons
);

/**
 * @brief Download signal amplitudes to CPU
 *
 * @param ctx GPU Axon context
 * @param signals Output array [num_axons * num_segments]
 * @param num_axons Number of axons
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_get_signals(
    axon_gpu_context_t* ctx,
    float* signals,
    uint32_t num_axons
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief GPU Axon statistics
 */
typedef struct {
    uint64_t total_spikes;           /**< Total spikes propagated */
    uint64_t total_arrivals;         /**< Total spike arrivals */
    uint64_t total_updates;          /**< Total update calls */
    float mean_velocity;             /**< Mean conduction velocity (m/s) */
    float mean_delay;                /**< Mean propagation delay (ms) */
    float mean_myelination;          /**< Mean myelination level (0-1) */
    float avg_propagate_time_us;     /**< Average propagation kernel time */
    float avg_velocity_time_us;      /**< Average velocity update time */
    float avg_step_time_us;          /**< Average step time */
    size_t gpu_memory_used;          /**< GPU memory in use */
} axon_gpu_stats_t;

/**
 * @brief Get GPU Axon statistics
 *
 * @param ctx GPU Axon context
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool axon_gpu_get_stats(
    const axon_gpu_context_t* ctx,
    axon_gpu_stats_t* stats
);

/**
 * @brief Reset GPU Axon statistics
 *
 * @param ctx GPU Axon context
 */
NIMCP_EXPORT void axon_gpu_reset_stats(axon_gpu_context_t* ctx);

//=============================================================================
// CPU Reference Implementations (for testing)
//=============================================================================

/**
 * @brief CPU reference implementation of signal propagation
 *
 * WHAT: CPU equivalent for GPU/CPU equivalence testing
 * WHY:  Verify GPU results match CPU
 */
NIMCP_EXPORT bool axon_cpu_propagate(
    float* signals,
    const float* seg_velocities,
    uint32_t num_axons,
    uint32_t num_segments,
    float dt
);

/**
 * @brief CPU reference implementation of velocity calculation
 */
NIMCP_EXPORT bool axon_cpu_calculate_velocities(
    float* velocities,
    const float* diameters,
    const float* myelination,
    uint32_t num_axons,
    float base_velocity,
    float myelin_multiplier
);

/**
 * @brief CPU reference implementation of myelination effect
 */
NIMCP_EXPORT bool axon_cpu_apply_myelination(
    float* velocities,
    const float* myelination,
    uint32_t num_axons,
    float myelin_multiplier,
    float max_velocity
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AXON_GPU_H */
