/**
 * @file nimcp_gpu_plasticity_bridge.h
 * @brief GPU Plasticity Bridge - Wires GPU plasticity kernels into training
 *
 * WHAT: Bridge layer between CPU training code and GPU plasticity CUDA kernels
 * WHY:  GPU-accelerated STDP/BCM/homeostatic updates for 2M neuron brains
 * HOW:  Extracts neuron/synapse data to GPU tensors, runs kernels, writes back
 *
 * ARCHITECTURE:
 * - gpu_plasticity_stdp_batch: GPU STDP for SNN populations
 * - gpu_plasticity_bcm_batch: GPU BCM threshold learning
 * - gpu_plasticity_homeostatic_batch: GPU homeostatic scaling
 * - gpu_plasticity_coordinator_update: GPU-accelerated coordinator step
 *
 * All functions fall back to CPU when gpu_ctx is NULL or GPU fails.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026-03-29
 */

#ifndef NIMCP_GPU_PLASTICITY_BRIDGE_H
#define NIMCP_GPU_PLASTICITY_BRIDGE_H

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations to avoid circular includes */
struct nimcp_gpu_context_s;
struct snn_network_s;
struct snn_training_ctx_s;

/* Opaque handle for persistent GPU plasticity state (avoids per-call allocs) */
typedef struct gpu_plasticity_state_s gpu_plasticity_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create persistent GPU plasticity state
 *
 * Allocates GPU tensors for traces/weights/spikes that persist across calls.
 * Must be called once during brain init and destroyed during brain cleanup.
 *
 * @param gpu_ctx GPU context
 * @param max_neurons Maximum neuron count for homeostatic tensors
 * @param max_synapses Maximum synapse count for STDP/BCM tensors
 * @return State handle or NULL on failure
 */
NIMCP_EXPORT gpu_plasticity_state_t* gpu_plasticity_state_create(
    struct nimcp_gpu_context_s* gpu_ctx,
    uint32_t max_neurons,
    uint32_t max_synapses
);

/**
 * @brief Destroy persistent GPU plasticity state
 * @param state State to destroy (NULL-safe)
 */
NIMCP_EXPORT void gpu_plasticity_state_destroy(gpu_plasticity_state_t* state);

/**
 * @brief GPU-accelerated STDP for SNN network
 *
 * Extracts spike timing from SNN populations, uploads to GPU tensors,
 * runs STDP kernel, downloads weight updates back to CPU synapses.
 *
 * @param gpu_ctx GPU context
 * @param state Persistent GPU state (NULL = allocate temp tensors)
 * @param snn SNN network with populations and synapses
 * @param ctx SNN training context with STDP parameters
 * @return Number of synapses updated, or 0 on failure/fallback
 */
NIMCP_EXPORT uint32_t gpu_plasticity_stdp_apply(
    struct nimcp_gpu_context_s* gpu_ctx,
    gpu_plasticity_state_t* state,
    struct snn_network_s* snn,
    struct snn_training_ctx_s* ctx
);

/**
 * @brief GPU-accelerated homeostatic plasticity update
 *
 * Uploads firing rates to GPU, computes scaling factors and intrinsic
 * plasticity updates, downloads results.
 *
 * @param gpu_ctx GPU context
 * @param state Persistent GPU state
 * @param firing_rates CPU array of firing rates [n_neurons]
 * @param n_neurons Number of neurons
 * @param target_rate Target firing rate (Hz)
 * @param dt Time step (ms)
 * @return 0 on success, -1 on failure
 */
NIMCP_EXPORT int gpu_plasticity_homeostatic_update(
    struct nimcp_gpu_context_s* gpu_ctx,
    gpu_plasticity_state_t* state,
    float* firing_rates,
    uint32_t n_neurons,
    float target_rate,
    float dt
);

/**
 * @brief GPU-accelerated plasticity coordinator update
 *
 * Runs STDP trace updates, BCM threshold adaptation, and homeostatic
 * scaling all on GPU in a single batch, reducing PCIe round-trips.
 *
 * @param gpu_ctx GPU context
 * @param state Persistent GPU state
 * @param coordinator Plasticity coordinator (for mechanism registry)
 * @param current_time_ms Current simulation time
 * @param dt Time step
 * @return Number of mechanisms updated, or -1 on failure
 */
NIMCP_EXPORT int gpu_plasticity_coordinator_update(
    struct nimcp_gpu_context_s* gpu_ctx,
    gpu_plasticity_state_t* state,
    void* coordinator,
    uint64_t current_time_ms,
    float dt
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GPU_PLASTICITY_BRIDGE_H */
