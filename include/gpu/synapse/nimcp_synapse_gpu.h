/**
 * @file nimcp_synapse_gpu.h
 * @brief GPU-accelerated Synapse Compute Kernels
 *
 * WHAT: CUDA kernels for synaptic transmission, vesicle dynamics, receptor kinetics
 * WHY:  GPU acceleration for large-scale synaptic computation (100K+ synapses)
 * HOW:  Custom kernels for biologically realistic synapse models
 *
 * ARCHITECTURE:
 * - Synaptic transmission: pre->post signal propagation with weight modulation
 * - Vesicle dynamics: Three-pool model (RRP, recycling, reserve)
 * - Receptor kinetics: Hill equation binding, desensitization
 * - Short-term plasticity: Facilitation and depression
 *
 * PERFORMANCE TARGETS:
 * - RTX 4090: 100K synapses in <500us
 * - Memory bandwidth: 90% of peak (1.8 TB/s)
 * - Occupancy: 100% on modern GPUs
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SYNAPSE_GPU_H
#define NIMCP_SYNAPSE_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Synapse Types
//=============================================================================

/**
 * @brief Synapse model type
 */
typedef enum {
    NIMCP_SYNAPSE_SIMPLE = 0,       /**< Simple weight multiplication */
    NIMCP_SYNAPSE_STP = 1,          /**< Short-term plasticity (TM model) */
    NIMCP_SYNAPSE_CONDUCTANCE = 2,  /**< Conductance-based */
    NIMCP_SYNAPSE_NMDA = 3,         /**< NMDA with voltage-dependent Mg2+ block */
    NIMCP_SYNAPSE_AMPA = 4,         /**< Fast AMPA kinetics */
    NIMCP_SYNAPSE_GABA_A = 5,       /**< Fast inhibitory (GABA-A) */
    NIMCP_SYNAPSE_GABA_B = 6        /**< Slow inhibitory (GABA-B) */
} nimcp_synapse_model_t;

//=============================================================================
// Vesicle Pool Parameters
//=============================================================================

/**
 * @brief GPU vesicle pool parameters
 * Tsodyks-Markram model with three-pool extension
 */
typedef struct {
    float U;                /**< Initial release probability (0.0-1.0) */
    float tau_rec;          /**< Recovery time constant (ms) - recycling to RRP */
    float tau_facil;        /**< Facilitation time constant (ms) */
    float tau_inact;        /**< Inactivation time constant (ms) */
    float quantal_size;     /**< Molecules per vesicle (default: 5000) */
    uint32_t rrp_capacity;  /**< Readily releasable pool capacity */
    float dt;               /**< Timestep (ms) */
} nimcp_vesicle_params_t;

/**
 * @brief GPU vesicle pool state
 */
typedef struct {
    nimcp_gpu_tensor_t* u;          /**< Utilization variable [n_synapses] */
    nimcp_gpu_tensor_t* x;          /**< Available resources [n_synapses] */
    nimcp_gpu_tensor_t* y;          /**< Released resources [n_synapses] */
    nimcp_gpu_tensor_t* z;          /**< Inactive resources [n_synapses] */
    nimcp_vesicle_params_t params;  /**< Pool parameters */
} nimcp_gpu_vesicle_state_t;

//=============================================================================
// Receptor Kinetics Parameters
//=============================================================================

/**
 * @brief Receptor binding kinetics parameters
 */
typedef struct {
    float kd;               /**< Dissociation constant (uM) */
    float hill_coef;        /**< Hill coefficient (cooperativity) */
    float tau_rise;         /**< Rise time constant (ms) */
    float tau_decay;        /**< Decay time constant (ms) */
    float tau_desens;       /**< Desensitization time constant (ms) */
    float max_conductance;  /**< Maximum conductance (nS) */
    float reversal;         /**< Reversal potential (mV) */
} nimcp_receptor_params_t;

/**
 * @brief Receptor state
 */
typedef struct {
    nimcp_gpu_tensor_t* occupancy;      /**< Receptor occupancy [n_synapses] */
    nimcp_gpu_tensor_t* desensitization;/**< Desensitization state [n_synapses] */
    nimcp_gpu_tensor_t* conductance;    /**< Current conductance [n_synapses] */
    nimcp_receptor_params_t params;     /**< Kinetics parameters */
} nimcp_gpu_receptor_state_t;

//=============================================================================
// Synapse State Structure
//=============================================================================

/**
 * @brief Complete synapse state for GPU
 */
typedef struct {
    // Connectivity
    nimcp_gpu_tensor_t* pre_ids;        /**< Presynaptic neuron IDs [n_synapses] */
    nimcp_gpu_tensor_t* post_ids;       /**< Postsynaptic neuron IDs [n_synapses] */
    nimcp_gpu_tensor_t* weights;        /**< Synaptic weights [n_synapses] */
    nimcp_gpu_tensor_t* delays;         /**< Axonal delays [n_synapses] (optional) */

    // STP state (Tsodyks-Markram)
    nimcp_gpu_vesicle_state_t* vesicle_state;   /**< Vesicle pool state */

    // Receptor state
    nimcp_gpu_receptor_state_t* receptor_state; /**< Receptor kinetics state */

    // Output
    nimcp_gpu_tensor_t* psc;            /**< Post-synaptic current [n_post] */
    nimcp_gpu_tensor_t* transmission;   /**< Synaptic transmission [n_synapses] */

    // Metadata
    size_t n_synapses;                  /**< Number of synapses */
    size_t n_pre;                       /**< Number of presynaptic neurons */
    size_t n_post;                      /**< Number of postsynaptic neurons */
    nimcp_synapse_model_t model;        /**< Synapse model type */
} nimcp_gpu_synapse_state_t;

//=============================================================================
// Vesicle Pool Lifecycle
//=============================================================================

/**
 * @brief Create GPU vesicle state
 *
 * @param ctx GPU context
 * @param n_synapses Number of synapses
 * @param params Vesicle pool parameters
 * @return Vesicle state or NULL on error
 */
NIMCP_EXPORT nimcp_gpu_vesicle_state_t* nimcp_gpu_vesicle_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_vesicle_params_t* params
);

/**
 * @brief Destroy GPU vesicle state
 */
NIMCP_EXPORT void nimcp_gpu_vesicle_state_destroy(nimcp_gpu_vesicle_state_t* state);

/**
 * @brief Get default vesicle parameters
 */
NIMCP_EXPORT nimcp_vesicle_params_t nimcp_gpu_vesicle_default_params(void);

//=============================================================================
// Receptor State Lifecycle
//=============================================================================

/**
 * @brief Create GPU receptor state
 *
 * @param ctx GPU context
 * @param n_synapses Number of synapses
 * @param params Receptor parameters
 * @return Receptor state or NULL on error
 */
NIMCP_EXPORT nimcp_gpu_receptor_state_t* nimcp_gpu_receptor_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_receptor_params_t* params
);

/**
 * @brief Destroy GPU receptor state
 */
NIMCP_EXPORT void nimcp_gpu_receptor_state_destroy(nimcp_gpu_receptor_state_t* state);

/**
 * @brief Get default AMPA receptor parameters
 */
NIMCP_EXPORT nimcp_receptor_params_t nimcp_gpu_receptor_ampa_params(void);

/**
 * @brief Get default NMDA receptor parameters
 */
NIMCP_EXPORT nimcp_receptor_params_t nimcp_gpu_receptor_nmda_params(void);

/**
 * @brief Get default GABA-A receptor parameters
 */
NIMCP_EXPORT nimcp_receptor_params_t nimcp_gpu_receptor_gabaa_params(void);

//=============================================================================
// Synapse State Lifecycle
//=============================================================================

/**
 * @brief Create GPU synapse state
 *
 * @param ctx GPU context
 * @param n_synapses Number of synapses
 * @param n_pre Number of presynaptic neurons
 * @param n_post Number of postsynaptic neurons
 * @param model Synapse model type
 * @return Synapse state or NULL on error
 */
NIMCP_EXPORT nimcp_gpu_synapse_state_t* nimcp_gpu_synapse_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    size_t n_pre,
    size_t n_post,
    nimcp_synapse_model_t model
);

/**
 * @brief Destroy GPU synapse state
 */
NIMCP_EXPORT void nimcp_gpu_synapse_state_destroy(nimcp_gpu_synapse_state_t* state);

//=============================================================================
// Synaptic Transmission Kernels
//=============================================================================

/**
 * @brief Compute synaptic transmission for all synapses
 *
 * WHAT: Propagate presynaptic activity through synapses
 * WHY:  Core synapse operation - weight * activity * STP
 * HOW:  1 thread per synapse, parallel weight multiplication
 *
 * ALGORITHM:
 * For each synapse i:
 *   transmission[i] = weight[i] * pre_activity[pre_id[i]] * stp_modulation
 *
 * @param ctx GPU context
 * @param state Synapse state
 * @param pre_activity Presynaptic activity [n_pre]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_transmit(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity
);

/**
 * @brief Compute post-synaptic currents by accumulating synaptic transmission
 *
 * WHAT: Sum synaptic inputs to each postsynaptic neuron
 * WHY:  Aggregate multiple synaptic inputs
 * HOW:  Scatter-add with atomics (or segmented reduction)
 *
 * ALGORITHM:
 * For each synapse i:
 *   atomicAdd(psc[post_id[i]], transmission[i])
 *
 * @param ctx GPU context
 * @param state Synapse state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_accumulate_psc(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state
);

/**
 * @brief Complete synapse forward pass
 *
 * WHAT: Combined transmission and accumulation
 * WHY:  Single-call convenience function
 * HOW:  Calls transmit + accumulate
 *
 * @param ctx GPU context
 * @param state Synapse state
 * @param pre_activity Presynaptic activity [n_pre]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity
);

//=============================================================================
// Vesicle Dynamics Kernels
//=============================================================================

/**
 * @brief Update vesicle release probability based on presynaptic activity
 *
 * WHAT: Tsodyks-Markram facilitation model
 * WHY:  Short-term facilitation from residual calcium
 * HOW:  u(t+dt) = U + u(t)*(1-U) * exp(-dt/tau_facil)
 *
 * @param ctx GPU context
 * @param state Vesicle state
 * @param pre_spikes Presynaptic spike indicators [n_synapses]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_vesicle_update_release_prob(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state,
    const nimcp_gpu_tensor_t* pre_spikes
);

/**
 * @brief Compute vesicle release and update resource pools
 *
 * WHAT: Release vesicles and update x, y, z pools
 * WHY:  Model short-term depression from RRP depletion
 * HOW:  Tsodyks-Markram differential equations
 *
 * EQUATIONS:
 *   dx/dt = z/tau_rec - u*x*spike
 *   dy/dt = -y/tau_inact + u*x*spike
 *   dz/dt = y/tau_inact - z/tau_rec
 *
 * @param ctx GPU context
 * @param state Vesicle state
 * @param pre_spikes Presynaptic spikes [n_synapses]
 * @param dt Timestep (ms)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_vesicle_release(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state,
    const nimcp_gpu_tensor_t* pre_spikes,
    float dt
);

/**
 * @brief Get effective synaptic efficacy from vesicle state
 *
 * WHAT: Compute STP modulation factor
 * WHY:  Apply STP to synaptic weights
 * HOW:  efficacy = u * x (utilization * available resources)
 *
 * @param ctx GPU context
 * @param state Vesicle state
 * @param efficacy Output efficacy tensor [n_synapses]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_vesicle_get_efficacy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_vesicle_state_t* state,
    nimcp_gpu_tensor_t* efficacy
);

//=============================================================================
// Receptor Kinetics Kernels
//=============================================================================

/**
 * @brief Update receptor binding from neurotransmitter concentration
 *
 * WHAT: Hill equation binding kinetics
 * WHY:  Model receptor activation with cooperativity
 * HOW:  occupancy = [NT]^n / (Kd^n + [NT]^n)
 *
 * @param ctx GPU context
 * @param state Receptor state
 * @param concentration Neurotransmitter concentration [n_synapses]
 * @param dt Timestep (ms)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_receptor_update_binding(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state,
    const nimcp_gpu_tensor_t* concentration,
    float dt
);

/**
 * @brief Update receptor desensitization
 *
 * WHAT: Model receptor desensitization over time
 * WHY:  Prolonged activation reduces response
 * HOW:  d_desens/dt = (occupancy - desens) / tau_desens
 *
 * @param ctx GPU context
 * @param state Receptor state
 * @param dt Timestep (ms)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_receptor_update_desensitization(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state,
    float dt
);

/**
 * @brief Compute receptor conductance
 *
 * WHAT: Compute synaptic conductance from receptor state
 * WHY:  Generate synaptic current
 * HOW:  g = g_max * occupancy * (1 - desensitization)
 *
 * @param ctx GPU context
 * @param state Receptor state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_receptor_compute_conductance(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state
);

/**
 * @brief Compute synaptic current from conductance
 *
 * WHAT: Ohmic current I = g * (V - E_rev)
 * WHY:  Convert conductance to current for neuron integration
 * HOW:  current = conductance * (V_post - reversal)
 *
 * @param ctx GPU context
 * @param state Receptor state
 * @param post_voltage Postsynaptic voltage [n_post]
 * @param current Output current [n_synapses]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_receptor_compute_current(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_receptor_state_t* state,
    const nimcp_gpu_tensor_t* post_voltage,
    nimcp_gpu_tensor_t* current
);

//=============================================================================
// NMDA Voltage-Dependent Block
//=============================================================================

/**
 * @brief Compute NMDA Mg2+ block factor
 *
 * WHAT: Voltage-dependent magnesium block of NMDA receptors
 * WHY:  NMDA receptors are coincidence detectors
 * HOW:  block = 1 / (1 + [Mg2+]/3.57 * exp(-0.062 * V))
 *
 * @param ctx GPU context
 * @param post_voltage Postsynaptic voltage [n_post]
 * @param mg_block Output Mg2+ block factor [n_post]
 * @param mg_concentration Extracellular Mg2+ (mM, default 1.0)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_nmda_mg_block(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* post_voltage,
    nimcp_gpu_tensor_t* mg_block,
    float mg_concentration
);

//=============================================================================
// Neurotransmitter Diffusion
//=============================================================================

/**
 * @brief Simulate neurotransmitter diffusion in synaptic cleft
 *
 * WHAT: Model neurotransmitter spread and clearance
 * WHY:  Realistic timing of receptor activation
 * HOW:  Exponential rise and decay: c(t) = A * (exp(-t/tau_d) - exp(-t/tau_r))
 *
 * @param ctx GPU context
 * @param release Amount released per synapse [n_synapses]
 * @param concentration Current concentration [n_synapses]
 * @param tau_rise Rise time constant (ms)
 * @param tau_decay Decay time constant (ms)
 * @param dt Timestep (ms)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_neurotransmitter_diffusion(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* release,
    nimcp_gpu_tensor_t* concentration,
    float tau_rise,
    float tau_decay,
    float dt
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Reset synapse state to initial values
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state
);

/**
 * @brief Reset vesicle state to initial values
 */
NIMCP_EXPORT bool nimcp_gpu_vesicle_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state
);

/**
 * @brief Reset receptor state to initial values
 */
NIMCP_EXPORT bool nimcp_gpu_receptor_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state
);

/**
 * @brief Get synapse statistics
 */
NIMCP_EXPORT bool nimcp_gpu_synapse_get_stats(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_synapse_state_t* state,
    float* mean_weight,
    float* mean_transmission,
    float* mean_efficacy
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYNAPSE_GPU_H
