//=============================================================================
// nimcp_snn_gpu.h - GPU Spiking Neural Network Kernels
//=============================================================================
/**
 * @file nimcp_snn_gpu.h
 * @brief GPU-accelerated Spiking Neural Network operations using CUDA
 *
 * WHAT: CUDA kernels for SNN neuron models and spike processing
 * WHY:  Enables massive parallel simulation of spiking networks
 * HOW:  Custom kernels for LIF, Izhikevich, surrogate gradients, STDP
 *
 * ARCHITECTURE:
 * - Neuron models: LIF, Izhikevich, AdEx (Adaptive Exponential)
 * - Surrogate gradients: SuperSpike, fast sigmoid, arctan, triangular
 * - Learning: STDP (pair-based, triplet), eligibility traces
 * - Spike propagation: event-driven processing
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SNN_GPU_H
#define NIMCP_SNN_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Neuron Model Types
//=============================================================================

typedef enum {
    NIMCP_SNN_LIF = 0,            /**< Leaky Integrate-and-Fire */
    NIMCP_SNN_IZHIKEVICH = 1,     /**< Izhikevich model */
    NIMCP_SNN_ADEX = 2,           /**< Adaptive Exponential IF */
    NIMCP_SNN_HH = 3              /**< Hodgkin-Huxley (simplified) */
} nimcp_snn_model_t;

typedef enum {
    NIMCP_SURROGATE_SUPERSPIKE = 0,    /**< SuperSpike surrogate */
    NIMCP_SURROGATE_FAST_SIGMOID = 1,  /**< Fast sigmoid */
    NIMCP_SURROGATE_ARCTAN = 2,        /**< Arctan */
    NIMCP_SURROGATE_TRIANGULAR = 3,    /**< Triangular */
    NIMCP_SURROGATE_GAUSSIAN = 4       /**< Gaussian */
} nimcp_surrogate_type_t;

//=============================================================================
// LIF Neuron Parameters
//=============================================================================

typedef struct {
    float tau_mem;      /**< Membrane time constant (ms) */
    float tau_syn;      /**< Synaptic time constant (ms) */
    float v_thresh;     /**< Spike threshold (mV) */
    float v_reset;      /**< Reset voltage (mV) */
    float v_rest;       /**< Resting potential (mV) */
    float dt;           /**< Timestep (ms) */
    bool hard_reset;    /**< Hard reset (true) or soft reset (false) */
} nimcp_lif_params_t;

//=============================================================================
// Izhikevich Neuron Parameters
//=============================================================================

typedef struct {
    float a;            /**< Recovery time constant */
    float b;            /**< Recovery sensitivity to v */
    float c;            /**< Reset voltage (mV) */
    float d;            /**< Recovery increment after spike */
    float v_thresh;     /**< Spike threshold (mV) */
    float dt;           /**< Timestep (ms) */
} nimcp_izhikevich_params_t;

//=============================================================================
// AdEx Neuron Parameters
//=============================================================================

typedef struct {
    float tau_mem;      /**< Membrane time constant */
    float tau_w;        /**< Adaptation time constant */
    float v_thresh;     /**< Spike threshold */
    float v_reset;      /**< Reset voltage */
    float v_rest;       /**< Resting potential */
    float v_rheo;       /**< Rheobase threshold */
    float delta_T;      /**< Slope factor */
    float a;            /**< Subthreshold adaptation */
    float b;            /**< Spike-triggered adaptation */
    float dt;           /**< Timestep */
} nimcp_adex_params_t;

//=============================================================================
// STDP Parameters
//=============================================================================

typedef struct {
    float A_plus;       /**< LTP amplitude */
    float A_minus;      /**< LTD amplitude */
    float tau_plus;     /**< LTP time constant (ms) */
    float tau_minus;    /**< LTD time constant (ms) */
    float w_max;        /**< Maximum weight */
    float w_min;        /**< Minimum weight */
} nimcp_stdp_params_t;

//=============================================================================
// Triplet STDP Parameters (Pfister & Gerstner 2006)
//=============================================================================

/**
 * @brief Triplet STDP parameters
 *
 * Extends pair-based STDP by considering triplets of spikes for more
 * biologically accurate learning. Based on Pfister & Gerstner (2006).
 *
 * The weight update rule is:
 *   dw = A2_plus * r1 * post_spike + A3_plus * r1 * o2 * post_spike   (LTP)
 *      - A2_minus * o1 * pre_spike - A3_minus * o1 * r2 * pre_spike   (LTD)
 *
 * Where:
 *   r1, r2 = presynaptic traces (fast and slow)
 *   o1, o2 = postsynaptic traces (fast and slow)
 */
typedef struct nimcp_triplet_stdp_params {
    // Pair-based terms (classic STDP)
    float A2_plus;    /**< LTP amplitude for pairs */
    float A2_minus;   /**< LTD amplitude for pairs */
    float tau_plus;   /**< LTP time constant (ms) */
    float tau_minus;  /**< LTD time constant (ms) */

    // Triplet terms
    float A3_plus;    /**< LTP amplitude for triplets (post-post-pre) */
    float A3_minus;   /**< LTD amplitude for triplets (pre-pre-post) */
    float tau_x;      /**< Presynaptic triplet trace time constant (ms) */
    float tau_y;      /**< Postsynaptic triplet trace time constant (ms) */

    // Bounds
    float w_min;      /**< Minimum synaptic weight */
    float w_max;      /**< Maximum synaptic weight */
} nimcp_triplet_stdp_params_t;

/**
 * @brief Triplet STDP state holding spike traces
 *
 * Maintains four traces:
 *   r1 (fast pre): decays with tau_plus, incremented on pre spike
 *   r2 (slow pre): decays with tau_x, incremented on pre spike
 *   o1 (fast post): decays with tau_minus, incremented on post spike
 *   o2 (slow post): decays with tau_y, incremented on post spike
 */
typedef struct nimcp_triplet_stdp_state {
    float* d_r1;      /**< Presynaptic trace (fast, for pairs) */
    float* d_r2;      /**< Presynaptic trace (slow, for triplets) */
    float* d_o1;      /**< Postsynaptic trace (fast, for pairs) */
    float* d_o2;      /**< Postsynaptic trace (slow, for triplets) */
    size_t num_pre;   /**< Number of presynaptic neurons */
    size_t num_post;  /**< Number of postsynaptic neurons */
} nimcp_triplet_stdp_state_t;

/**
 * @brief DAO pattern for STDP state management
 *
 * Encapsulates STDP state and operations with function pointers
 * for flexible, testable learning rule implementations.
 */
typedef struct nimcp_stdp_dao {
    nimcp_triplet_stdp_state_t* state;    /**< Internal state */
    nimcp_triplet_stdp_params_t params;   /**< Learning parameters */
    void* gpu_context;                     /**< GPU context reference */

    /** Update spike traces based on current spikes */
    int (*update_traces)(struct nimcp_stdp_dao* self, const int* pre_spikes,
                         const int* post_spikes, size_t num_pre_spikes,
                         size_t num_post_spikes, float dt);

    /** Compute weight updates without applying them */
    int (*compute_weight_updates)(struct nimcp_stdp_dao* self, float* weights,
                                  const int* pre_indices, const int* post_indices,
                                  size_t num_synapses);

    /** Apply computed weight updates with learning rate */
    int (*apply_updates)(struct nimcp_stdp_dao* self, float* weights,
                         float learning_rate);

    /** Reset all traces to zero */
    int (*reset)(struct nimcp_stdp_dao* self);
} nimcp_stdp_dao_t;

//=============================================================================
// SNN State Structures
//=============================================================================

/**
 * @brief LIF neuron state
 *
 * Per-neuron LIF heterogeneity (Wave G GPU sync, schema v17 — 2026-04-27):
 *   The optional `tau_mem_per_neuron` and `v_thresh_per_neuron` tensors
 *   carry one float per neuron (sized to match `v->numel`). When non-NULL
 *   the LIF kernels load the per-neuron value instead of `params.tau_mem`
 *   / `params.v_thresh`. NULL → kernel falls back to the scalar in
 *   `params` (bit-identical to pre-Wave-G GPU behavior). Populate via
 *   `nimcp_gpu_lif_state_upload_per_neuron_params()`. The arrays also
 *   carry per-population subclass deltas (PV τ=10 ms, SOM τ=30 ms, L5 Betz
 *   v_thresh −2 mV, etc.) — the GPU kernel reads each neuron's resolved
 *   value directly so subclass + heterogeneity compose on GPU same as CPU.
 */
typedef struct {
    nimcp_gpu_tensor_t* v;          /**< Membrane potential */
    nimcp_gpu_tensor_t* i_syn;      /**< Synaptic current */
    nimcp_gpu_tensor_t* spikes;     /**< Output spikes (binary) */
    nimcp_lif_params_t params;      /**< Neuron parameters (scalar fallback) */
    nimcp_gpu_tensor_t* tau_mem_per_neuron;  /**< Optional [n] τ_mem per neuron; NULL = use params.tau_mem */
    nimcp_gpu_tensor_t* v_thresh_per_neuron; /**< Optional [n] v_thresh per neuron; NULL = use params.v_thresh */
    /* Wave G GPU sync (v17): set to true to request a per-neuron-params re-upload
     * from CPU pops on the next GPU step. Owned by the caller (e.g.
     * snn_network_step); the kernel launcher does not read or clear this flag. */
    bool per_neuron_params_dirty;

    /* CB Phase 5 (GPU port). Per-receptor conductance arrays — one float per
     * neuron each, allocated lazily by nimcp_gpu_lif_state_alloc_cb_arrays()
     * when conductance mode is activated. NULL when CB is OFF (kernel falls
     * back to current-based via i_syn). The four arrays mirror the CPU
     * snn_population_t::g_ampa / g_nmda / g_gaba_a / g_gaba_b host buffers.
     *
     * Stage A contract: synaptic deposit still runs on CPU; this struct
     * carries the per-step-uploaded snapshot the GPU LIF kernel reads. The
     * dirty flag is set true by the host after each deposit pass so the
     * SNN step layer knows to re-upload before the kernel launch. Kernel
     * does not clear; caller (nimcp_gpu_lif_forward_cb) downloads g back
     * after the kernel updates them via the per-step decay step. */
    nimcp_gpu_tensor_t* g_ampa;     /**< Optional [n] AMPA conductance, NULL when CB OFF */
    nimcp_gpu_tensor_t* g_nmda;     /**< Optional [n] NMDA conductance, NULL when CB OFF */
    nimcp_gpu_tensor_t* g_gaba_a;   /**< Optional [n] GABA_A conductance, NULL when CB OFF */
    nimcp_gpu_tensor_t* g_gaba_b;   /**< Optional [n] GABA_B conductance, NULL when CB OFF */
    bool cb_arrays_dirty;           /**< Host-side dirty flag — re-upload g_* before next step */
} nimcp_lif_state_t;

/**
 * @brief Izhikevich neuron state
 */
typedef struct {
    nimcp_gpu_tensor_t* v;          /**< Membrane potential */
    nimcp_gpu_tensor_t* u;          /**< Recovery variable */
    nimcp_gpu_tensor_t* spikes;     /**< Output spikes */
    nimcp_izhikevich_params_t params;
} nimcp_izhikevich_state_t;

/**
 * @brief AdEx neuron state
 */
typedef struct {
    nimcp_gpu_tensor_t* v;          /**< Membrane potential */
    nimcp_gpu_tensor_t* w;          /**< Adaptation current */
    nimcp_gpu_tensor_t* spikes;     /**< Output spikes */
    nimcp_adex_params_t params;
} nimcp_adex_state_t;

//=============================================================================
// LIF Neuron Model
//=============================================================================

/**
 * @brief Create LIF neuron state
 */
NIMCP_EXPORT nimcp_lif_state_t* nimcp_lif_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_lif_params_t* params
);

/**
 * @brief Destroy LIF neuron state
 */
NIMCP_EXPORT void nimcp_lif_state_destroy(nimcp_lif_state_t* state);

/**
 * @brief Upload optional per-neuron τ_mem and v_thresh arrays to the
 *        GPU LIF state (Wave G GPU sync, schema v17).
 *
 * Either argument may be NULL — that array stays unallocated on the device
 * (kernel falls back to the scalar in `state->params`). Pass non-NULL to
 * allocate / overwrite the device buffer with the host data. To clear a
 * previously-uploaded array, call this function again with NULL for that
 * field — the device buffer is freed and the kernel falls back to scalar.
 *
 * Bit-identity contract: with both arguments NULL (or never called) the
 * GPU LIF kernel runs exactly as pre-Wave-G — so existing callers that
 * have not opted into heterogeneity see no behavior change.
 *
 * Thread safety: caller must hold the network's mutex (or otherwise
 * serialize against the step loop) — the function does not lock.
 *
 * @param ctx              GPU context (NOT NULL).
 * @param state            LIF state to update (NOT NULL).
 * @param tau_mem_host     Host array [n_neurons] of τ_mem values, or NULL.
 * @param v_thresh_host    Host array [n_neurons] of v_thresh values, or NULL.
 * @param n_neurons        Must match `state->v->numel`.
 * @return true on success, false on context/state mismatch or alloc failure.
 */
NIMCP_EXPORT bool nimcp_gpu_lif_state_upload_per_neuron_params(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const float* tau_mem_host,
    const float* v_thresh_host,
    size_t n_neurons
);

/**
 * @brief LIF forward pass
 *
 * Implements: dv/dt = (v_rest - v + R*I) / tau_mem
 * Spike if v > v_thresh, then reset to v_reset
 *
 * @param ctx GPU context
 * @param state LIF neuron state
 * @param input Synaptic input current
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lif_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* input
);

/* ===========================================================================
 * CB Phase 5 — GPU conductance-based LIF
 * ========================================================================= */

/**
 * @brief Allocate the four per-receptor conductance tensors on the GPU.
 *
 * Idempotent: if all four are already allocated and sized to n_neurons,
 * returns true without re-allocating. Re-allocates if sizes mismatch.
 *
 * Must be called once after CB is activated (typically by snn_network_step
 * the first time it sees conductance_enabled+cb_gpu_enabled). Pairs with
 * nimcp_gpu_lif_state_free_cb_arrays() during shutdown.
 *
 * @param ctx       GPU context (NOT NULL)
 * @param state     LIF state (NOT NULL); state->v->numel must equal n_neurons
 * @param n_neurons Total neurons across all populations (matches v size)
 * @return true on success, false on alloc failure / size mismatch
 */
NIMCP_EXPORT bool nimcp_gpu_lif_state_alloc_cb_arrays(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    size_t n_neurons
);

/**
 * @brief Free the four per-receptor conductance tensors. Idempotent.
 */
NIMCP_EXPORT void nimcp_gpu_lif_state_free_cb_arrays(
    nimcp_lif_state_t* state
);

/**
 * @brief Upload host conductance buffers into the GPU tensors.
 *
 * Called by the SNN step layer after the CPU synaptic-deposit pass populates
 * the host g_* arrays. Any host pointer may be NULL — that receptor's
 * device buffer is left unchanged (use this to skip uploading receptors
 * the network doesn't have allocated).
 *
 * @param ctx        GPU context
 * @param state      LIF state with cb arrays allocated
 * @param g_ampa_h   Host [n] AMPA conductance (or NULL)
 * @param g_nmda_h   Host [n] NMDA (or NULL)
 * @param g_gaba_a_h Host [n] GABA_A (or NULL)
 * @param g_gaba_b_h Host [n] GABA_B (or NULL)
 * @param n_neurons  Length of each non-NULL host buffer
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lif_state_upload_g(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const float* g_ampa_h,
    const float* g_nmda_h,
    const float* g_gaba_a_h,
    const float* g_gaba_b_h,
    size_t n_neurons
);

/**
 * @brief Download GPU conductance state back to host buffers.
 *
 * After the GPU LIF kernel runs the per-step decay (g *= exp(-dt/τ_r)),
 * the SNN step layer needs the decayed values back on the host so the
 * next CPU synaptic-deposit pass starts from the right baseline. Any
 * host pointer may be NULL to skip that receptor.
 *
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lif_state_download_g(
    nimcp_gpu_context_t* ctx,
    const nimcp_lif_state_t* state,
    float* g_ampa_h,
    float* g_nmda_h,
    float* g_gaba_a_h,
    float* g_gaba_b_h,
    size_t n_neurons
);

/**
 * @brief Conductance-based LIF forward pass.
 *
 * Implements the same dynamics as snn_membrane_compute_dv() with
 * conductance_mode=true:
 *   drive   = (v_rest - v)
 *           + g_ampa             * (E_ampa   - v)
 *           + g_nmda * mg_block  * (E_nmda   - v)
 *           + g_gaba_a           * (E_gaba_a - v)
 *           + g_gaba_b           * (E_gaba_b - v)
 *   dv      = clamp(drive * dt / tau_eff, ±100 mV)
 *   dv      = bound by reversal potentials (V cannot cross any E_r)
 *   v      += dv
 *   spike   = (v >= v_thresh) ? 1 : 0
 *   if spike: v = v_reset (or v -= (v_thresh - v_reset) for soft reset)
 *   g_r    *= decay_r          (decay applied AFTER membrane update,
 *                               matching CPU per-step ordering)
 *
 * NMDA Mg²⁺ block: nmda_block = 1 / (1 + Mg/3.57 * exp(-V/16.13))
 *
 * Per-neuron heterogeneity: like nimcp_gpu_lif_forward, when the optional
 * tau_mem_per_neuron / v_thresh_per_neuron arrays are populated the kernel
 * uses them; otherwise the scalars in state->params.
 *
 * @param ctx           GPU context
 * @param state         LIF state with cb arrays allocated + uploaded
 * @param e_ampa_mv     AMPA reversal (mV)  — typically  0
 * @param e_nmda_mv     NMDA reversal (mV)  — typically  0
 * @param e_gaba_a_mv   GABA_A reversal (mV) — typically -75
 * @param e_gaba_b_mv   GABA_B reversal (mV) — typically -75
 * @param tau_ampa_ms   AMPA decay τ (ms)   — typically  2
 * @param tau_nmda_ms   NMDA decay τ (ms)   — typically 100
 * @param tau_gaba_a_ms GABA_A decay τ (ms) — typically 10
 * @param tau_gaba_b_ms GABA_B decay τ (ms) — typically 150
 * @param mg_mm         Extracellular [Mg²⁺] (mM) — typically 1.0
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lif_forward_cb(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    float e_ampa_mv,   float e_nmda_mv,
    float e_gaba_a_mv, float e_gaba_b_mv,
    float tau_ampa_ms,   float tau_nmda_ms,
    float tau_gaba_a_ms, float tau_gaba_b_ms,
    float mg_mm
);

/**
 * @brief Compute I_syn from CSR synapse storage on GPU
 *
 * Sparse gather: for each destination neuron, sum weights of incoming
 * synapses whose source neurons spiked. Uses flat column indices that
 * map (src_pop, src_neuron) to a global spike vector offset.
 *
 * @param ctx GPU context
 * @param d_spike_vector Device pointer: flattened spikes [total_neurons]
 * @param d_weights Device pointer: CSR weights [nnz]
 * @param d_col_indices Device pointer: flat column indices [nnz]
 * @param d_row_ptr Device pointer: CSR row pointers [n_neurons+1]
 * @param d_external_current Device pointer: external currents [n_neurons]
 * @param d_output_isyn Device pointer: output I_syn [n_neurons]
 * @param n_neurons Number of neurons in this population
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_snn_isyn_csr(
    nimcp_gpu_context_t* ctx,
    const float* d_spike_vector,
    const float* d_weights,
    const unsigned int* d_col_indices,
    const unsigned int* d_row_ptr,
    const float* d_external_current,
    float* d_output_isyn,
    size_t n_neurons);

/**
 * @brief CB-GPU-7: Conductance-based deposit kernel for one destination pop.
 *
 * Replaces the per-neuron host CSR walk in snn_cb_deposit_neuron. One thread
 * per dst neuron walks its incoming CSR, gathers presynaptic spikes from the
 * flat all-pops spike vector, applies short-term depression, and routes the
 * weighted contribution into AMPA / NMDA / GABA_A / GABA_B based on a
 * per-(src_pop → this dst_pop) synapse_type table.
 *
 * Behavior matches the host helper at src/snn/nimcp_snn_network.c
 * snn_cb_deposit_neuron() for the spike-driven CSR portion. External
 * current, basket inhibition, and Poisson noise are NOT done here — they
 * stay on the host (cheap, no CSR walk) and are folded into d_g_* via a
 * separate "host delta" upload step before the deposit kernel runs.
 *
 * The d_g_* arrays are GPU-resident across steps (kernel_lif_forward_cb
 * decays at end of step). This kernel writes ADDITIVELY on top of that
 * post-decay state.
 *
 * @param ctx GPU context
 * @param d_spike_vector  Device: [total_neurons] flat all-pops spike output
 * @param d_depression    Device: [total_neurons] per-source depression (or NULL)
 * @param d_weights       Device: [nnz] CSR synapse weights for this dst pop
 * @param d_flat_col_idx  Device: [nnz] flat global neuron index per synapse
 * @param d_src_pop_idx   Device: [nnz] source pop index per synapse (routing key)
 * @param d_row_ptr       Device: [n_neurons_pop+1] CSR row offsets
 * @param d_synapse_type  Device: [n_pops] synapse_type for each (src_pop → this dst)
 * @param d_g_ampa        Device: [total_neurons] flat AMPA conductance (NULL ok)
 * @param d_g_nmda        Device: [total_neurons] flat NMDA conductance (NULL ok)
 * @param d_g_gaba_a      Device: [total_neurons] flat GABA-A conductance (NULL ok)
 * @param d_g_gaba_b      Device: [total_neurons] flat GABA-B conductance (NULL ok)
 * @param dst_pop_offset  Flat offset where this dst pop begins in d_g_*
 * @param n_neurons_pop   Number of neurons in this dst pop
 * @return true on launch success
 */
NIMCP_EXPORT bool nimcp_gpu_snn_cb_deposit_csr(
    nimcp_gpu_context_t* ctx,
    const float*    d_spike_vector,
    const float*    d_depression,
    const float*    d_weights,
    const uint32_t* d_flat_col_idx,
    const uint32_t* d_src_pop_idx,
    const uint32_t* d_row_ptr,
    const uint8_t*  d_synapse_type,
    float*          d_g_ampa,
    float*          d_g_nmda,
    float*          d_g_gaba_a,
    float*          d_g_gaba_b,
    uint32_t dst_pop_offset,
    uint32_t n_neurons_pop);

/**
 * @brief LIF backward pass with surrogate gradient
 *
 * @param ctx GPU context
 * @param state LIF neuron state
 * @param grad_output Gradient from upstream
 * @param grad_input Output: gradient w.r.t. input
 * @param surrogate_type Type of surrogate gradient
 * @param beta Surrogate gradient sharpness
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lif_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_surrogate_type_t surrogate_type,
    float beta
);

//=============================================================================
// Izhikevich Neuron Model
//=============================================================================

/**
 * @brief Create Izhikevich neuron state
 */
NIMCP_EXPORT nimcp_izhikevich_state_t* nimcp_izhikevich_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_izhikevich_params_t* params
);

/**
 * @brief Destroy Izhikevich neuron state
 */
NIMCP_EXPORT void nimcp_izhikevich_state_destroy(nimcp_izhikevich_state_t* state);

/**
 * @brief Izhikevich forward pass
 *
 * Implements:
 *   dv/dt = 0.04*v^2 + 5*v + 140 - u + I
 *   du/dt = a*(b*v - u)
 *   if v >= 30mV: v = c, u = u + d
 */
NIMCP_EXPORT bool nimcp_gpu_izhikevich_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_izhikevich_state_t* state,
    const nimcp_gpu_tensor_t* input
);

//=============================================================================
// AdEx Neuron Model
//=============================================================================

/**
 * @brief Create AdEx neuron state
 */
NIMCP_EXPORT nimcp_adex_state_t* nimcp_adex_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_adex_params_t* params
);

/**
 * @brief Destroy AdEx neuron state
 */
NIMCP_EXPORT void nimcp_adex_state_destroy(nimcp_adex_state_t* state);

/**
 * @brief AdEx forward pass
 */
NIMCP_EXPORT bool nimcp_gpu_adex_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_adex_state_t* state,
    const nimcp_gpu_tensor_t* input
);

//=============================================================================
// Surrogate Gradient Functions
//=============================================================================

/**
 * @brief Compute surrogate gradient
 *
 * @param ctx GPU context
 * @param v Membrane potential
 * @param v_thresh Spike threshold
 * @param grad Output gradient tensor
 * @param surrogate_type Type of surrogate function
 * @param beta Sharpness parameter
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_surrogate_gradient(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* v,
    float v_thresh,
    nimcp_gpu_tensor_t* grad,
    nimcp_surrogate_type_t surrogate_type,
    float beta
);

//=============================================================================
// Spike Propagation
//=============================================================================

/**
 * @brief Propagate spikes through synapses
 *
 * Computes: I_post = W @ spikes_pre (sparse matrix-vector multiply)
 *
 * @param ctx GPU context
 * @param spikes Pre-synaptic spike tensor (binary)
 * @param weights Synaptic weight matrix
 * @param output Post-synaptic current
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_spike_propagate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Event-driven spike propagation (sparse)
 *
 * Only processes non-zero spikes for efficiency
 *
 * @param ctx GPU context
 * @param spike_indices Indices of neurons that spiked
 * @param n_spikes Number of spikes
 * @param weights Synaptic weight matrix
 * @param output Post-synaptic current
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_spike_propagate_sparse(
    nimcp_gpu_context_t* ctx,
    const uint32_t* spike_indices,
    size_t n_spikes,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* output
);

//=============================================================================
// STDP Learning
//=============================================================================

/**
 * @brief Update eligibility traces
 *
 * For E-prop and other eligibility-based learning
 * e = e * decay + spike * (1 - decay)
 */
NIMCP_EXPORT bool nimcp_gpu_eligibility_trace_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* trace,
    const nimcp_gpu_tensor_t* spikes,
    float decay
);

/**
 * @brief Pair-based STDP update
 *
 * Classic Hebbian STDP:
 * - Pre before post (dt > 0): potentiation (A_plus * exp(-dt/tau_plus))
 * - Post before pre (dt < 0): depression (A_minus * exp(dt/tau_minus))
 *
 * @param ctx GPU context
 * @param weights Synaptic weights (in-place update)
 * @param pre_spikes Pre-synaptic spikes
 * @param post_spikes Post-synaptic spikes
 * @param pre_trace Pre-synaptic eligibility trace
 * @param post_trace Post-synaptic eligibility trace
 * @param params STDP parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stdp_pair(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace,
    const nimcp_gpu_tensor_t* post_trace,
    const nimcp_stdp_params_t* params
);

/**
 * @brief Triplet STDP update (legacy interface)
 *
 * Enhanced STDP that considers triplets of spikes
 */
NIMCP_EXPORT bool nimcp_gpu_stdp_triplet(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    nimcp_gpu_tensor_t* pre_trace_fast,
    nimcp_gpu_tensor_t* pre_trace_slow,
    nimcp_gpu_tensor_t* post_trace_fast,
    nimcp_gpu_tensor_t* post_trace_slow,
    const nimcp_stdp_params_t* params
);

//=============================================================================
// Triplet STDP DAO API (Pfister & Gerstner 2006)
//=============================================================================

/**
 * @brief Create triplet STDP DAO
 *
 * Creates a Data Access Object for triplet STDP learning with full
 * state management and parameterized learning rules.
 *
 * @param gpu_ctx GPU context
 * @param num_pre Number of presynaptic neurons
 * @param num_post Number of postsynaptic neurons
 * @param params Triplet STDP parameters
 * @return DAO object or NULL on failure
 */
NIMCP_EXPORT nimcp_stdp_dao_t* nimcp_triplet_stdp_create(
    void* gpu_ctx,
    size_t num_pre,
    size_t num_post,
    nimcp_triplet_stdp_params_t* params
);

/**
 * @brief Destroy triplet STDP DAO
 *
 * Frees all GPU memory and destroys the DAO object.
 *
 * @param dao DAO to destroy
 */
NIMCP_EXPORT void nimcp_triplet_stdp_destroy(nimcp_stdp_dao_t* dao);

/**
 * @brief Perform one step of triplet STDP learning
 *
 * Updates traces based on current spikes and modifies weights accordingly.
 * This is a convenience function that combines trace update and weight update.
 *
 * The triplet rule (Pfister & Gerstner 2006):
 *   - LTP: dw = (A2_plus + A3_plus * o2) * r1 * post_spike
 *   - LTD: dw = -(A2_minus + A3_minus * r2) * o1 * pre_spike
 *
 * @param dao STDP DAO object
 * @param pre_spikes Array of presynaptic neuron indices that spiked
 * @param post_spikes Array of postsynaptic neuron indices that spiked
 * @param num_pre_spikes Number of presynaptic spikes
 * @param num_post_spikes Number of postsynaptic spikes
 * @param weights Weight matrix (num_pre x num_post), modified in-place
 * @param pre_indices Presynaptic indices for synapse connections
 * @param post_indices Postsynaptic indices for synapse connections
 * @param num_synapses Number of synapses to update
 * @param dt Timestep (ms)
 * @param learning_rate Global learning rate multiplier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nimcp_triplet_stdp_step(
    nimcp_stdp_dao_t* dao,
    const int* pre_spikes,
    const int* post_spikes,
    size_t num_pre_spikes,
    size_t num_post_spikes,
    float* weights,
    const int* pre_indices,
    const int* post_indices,
    size_t num_synapses,
    float dt,
    float learning_rate
);

/**
 * @brief Full triplet STDP update with tensor interface
 *
 * Higher-level interface using GPU tensors instead of raw arrays.
 * Updates traces and applies weight changes in a single call.
 *
 * @param ctx GPU context
 * @param weights Weight tensor (modified in-place)
 * @param pre_spikes Presynaptic spike tensor (binary 0/1)
 * @param post_spikes Postsynaptic spike tensor (binary 0/1)
 * @param r1 Fast presynaptic trace (updated in-place)
 * @param r2 Slow presynaptic trace (updated in-place)
 * @param o1 Fast postsynaptic trace (updated in-place)
 * @param o2 Slow postsynaptic trace (updated in-place)
 * @param params Triplet STDP parameters
 * @param dt Timestep (ms)
 * @param learning_rate Learning rate multiplier
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_triplet_stdp_full(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    nimcp_gpu_tensor_t* r1,
    nimcp_gpu_tensor_t* r2,
    nimcp_gpu_tensor_t* o1,
    nimcp_gpu_tensor_t* o2,
    const nimcp_triplet_stdp_params_t* params,
    float dt,
    float learning_rate
);

/**
 * @brief Get default triplet STDP parameters
 *
 * Returns biologically plausible default parameters based on
 * experimental data from Pfister & Gerstner (2006).
 *
 * @param params Output parameter structure to fill
 */
NIMCP_EXPORT void nimcp_triplet_stdp_default_params(
    nimcp_triplet_stdp_params_t* params
);

//=============================================================================
// High-Level SNN Layer API (E2E Test Convenience)
//=============================================================================

/**
 * @brief SNN layer configuration for easy creation
 */
typedef struct {
    size_t num_neurons;         /**< Number of neurons in layer */
    float tau_mem;              /**< Membrane time constant (ms) */
    float tau_syn;              /**< Synaptic time constant (ms) */
    float v_rest;               /**< Resting potential (mV) */
    float v_thresh;             /**< Spike threshold (mV) */
    float v_reset;              /**< Reset potential (mV) */
    float dt;                   /**< Timestep (ms) */
    float refractory_period;    /**< Refractory period (ms) */
    nimcp_snn_model_t model;    /**< Neuron model type */
} nimcp_snn_lif_config_t;

/**
 * @brief Unified SNN layer handle
 */
typedef struct nimcp_snn_layer_s {
    nimcp_snn_model_t model;
    size_t num_neurons;
    nimcp_lif_state_t* lif_state;
    nimcp_izhikevich_state_t* izh_state;
    nimcp_adex_state_t* adex_state;
    nimcp_gpu_context_t* ctx;
    float refractory_period;
    nimcp_gpu_tensor_t* refractory_timer;  /**< Time remaining in refractory */
} nimcp_snn_layer_t;

/**
 * @brief Create SNN layer from configuration
 */
NIMCP_EXPORT nimcp_snn_layer_t* nimcp_snn_lif_layer_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_snn_lif_config_t* config
);

/**
 * @brief Destroy SNN layer
 */
NIMCP_EXPORT void nimcp_snn_layer_destroy(nimcp_snn_layer_t* layer);

/**
 * @brief Get membrane potential tensor
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_snn_layer_get_membrane(nimcp_snn_layer_t* layer);

/**
 * @brief Get output spikes tensor
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_snn_layer_get_spikes(nimcp_snn_layer_t* layer);

/**
 * @brief Get layer size (number of neurons)
 */
NIMCP_EXPORT size_t nimcp_snn_layer_get_size(const nimcp_snn_layer_t* layer);

/**
 * @brief Get membrane time constant
 */
NIMCP_EXPORT float nimcp_snn_layer_get_tau_mem(const nimcp_snn_layer_t* layer);

/**
 * @brief Reset layer state to initial values
 */
NIMCP_EXPORT bool nimcp_snn_layer_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer
);

/**
 * @brief Forward pass for one timestep
 */
NIMCP_EXPORT bool nimcp_snn_layer_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input
);

/**
 * @brief Forward pass that returns spikes tensor
 * @return Pointer to internal spikes tensor (do not free)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_snn_lif_step(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input
);

/**
 * @brief Create spike tensor from host data
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_snn_spike_tensor_create(
    nimcp_gpu_context_t* ctx,
    const uint8_t* data,
    const size_t* dims,
    size_t ndim
);

/**
 * @brief Copy spike tensor to host
 */
NIMCP_EXPORT bool nimcp_snn_spike_tensor_to_host(
    const nimcp_gpu_tensor_t* tensor,
    uint8_t* data
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Reset neuron state to resting values
 */
NIMCP_EXPORT bool nimcp_gpu_snn_reset_state(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* v,
    float v_rest
);

/**
 * @brief Count spikes in tensor
 */
NIMCP_EXPORT bool nimcp_gpu_spike_count(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    uint32_t* count
);

/**
 * @brief Compute spike rate (spikes per timestep)
 */
NIMCP_EXPORT bool nimcp_gpu_spike_rate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    size_t n_timesteps,
    nimcp_gpu_tensor_t* rates
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SNN_GPU_H
