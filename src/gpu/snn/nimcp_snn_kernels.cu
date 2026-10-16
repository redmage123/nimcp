/**
 * @file nimcp_snn_kernels.cu
 * @brief GPU Spiking Neural Network CUDA Kernels
 *
 * WHAT: CUDA kernels for SNN neuron models and learning
 * WHY:  GPU acceleration for spiking neural network simulation
 * HOW:  Custom kernels for LIF, Izhikevich, AdEx, STDP, surrogate gradients
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "SNN_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// LIF Neuron State Management
//=============================================================================

nimcp_lif_state_t* nimcp_lif_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_lif_params_t* params)
{
    if (!ctx || !params || n_neurons == 0) {
        LOG_ERROR("Invalid parameters for LIF state creation");
        return NULL;
    }

    nimcp_lif_state_t* state = (nimcp_lif_state_t*)nimcp_calloc(1, sizeof(nimcp_lif_state_t));
    if (!state) return NULL;

    size_t dims[1] = {n_neurons};

    state->v = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->i_syn = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->spikes = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    // Wave G GPU sync (v17): per-neuron arrays default NULL — bit-identical
    // pre-Wave-G behavior until nimcp_gpu_lif_state_upload_per_neuron_params
    // is called. dirty=true on first creation so the SNN step layer pushes
    // composed (subclass + heterogeneity) per-neuron values on the next step.
    state->tau_mem_per_neuron      = NULL;
    state->v_thresh_per_neuron     = NULL;
    state->per_neuron_params_dirty = true;

    if (!state->v || !state->i_syn || !state->spikes) {
        nimcp_lif_state_destroy(state);
        return NULL;
    }

    // Initialize to resting potential
    nimcp_gpu_fill(ctx, state->v, params->v_rest);
    nimcp_gpu_zeros(ctx, state->i_syn);
    nimcp_gpu_zeros(ctx, state->spikes);

    state->params = *params;

    LOG_DEBUG("Created LIF state for %zu neurons", n_neurons);
    return state;
}

void nimcp_lif_state_destroy(nimcp_lif_state_t* state)
{
    if (!state) return;

    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->i_syn) nimcp_gpu_tensor_destroy(state->i_syn);
    if (state->spikes) nimcp_gpu_tensor_destroy(state->spikes);
    // Wave G GPU sync (v17): optional per-neuron arrays.
    if (state->tau_mem_per_neuron)  nimcp_gpu_tensor_destroy(state->tau_mem_per_neuron);
    if (state->v_thresh_per_neuron) nimcp_gpu_tensor_destroy(state->v_thresh_per_neuron);
    // CB Phase 5 (GPU port): per-receptor conductance arrays.
    if (state->g_ampa)   nimcp_gpu_tensor_destroy(state->g_ampa);
    if (state->g_nmda)   nimcp_gpu_tensor_destroy(state->g_nmda);
    if (state->g_gaba_a) nimcp_gpu_tensor_destroy(state->g_gaba_a);
    if (state->g_gaba_b) nimcp_gpu_tensor_destroy(state->g_gaba_b);
    nimcp_free(state);
}

/* ---------------------------------------------------------------------------
 * CB Phase 5 — GPU conductance arrays: alloc / free / upload / download
 *
 * Stage A wiring: synaptic deposit still runs on CPU; per step the SNN layer
 * uploads the four host g_* buffers, runs the CB kernel (which reads the four
 * g, applies driving force to V, then decays each g by exp(-dt/tau_r)), then
 * downloads g back so the next CPU deposit sees the decayed values.
 * ------------------------------------------------------------------------- */

bool nimcp_gpu_lif_state_alloc_cb_arrays(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    size_t n_neurons)
{
    if (!ctx || !state || n_neurons == 0) {
        LOG_ERROR("alloc_cb_arrays: invalid args (ctx=%p state=%p n=%zu)",
                  (void*)ctx, (void*)state, n_neurons);
        return false;
    }
    if (state->v && state->v->numel != n_neurons) {
        LOG_ERROR("alloc_cb_arrays: n_neurons (%zu) does not match state->v->numel (%zu)",
                  n_neurons, (size_t)state->v->numel);
        return false;
    }
    size_t dims[1] = { n_neurons };
    nimcp_gpu_tensor_t** slots[4] = {
        &state->g_ampa, &state->g_nmda, &state->g_gaba_a, &state->g_gaba_b
    };
    const char* names[4] = { "g_ampa", "g_nmda", "g_gaba_a", "g_gaba_b" };
    for (int i = 0; i < 4; i++) {
        if (*slots[i] && (*slots[i])->numel == n_neurons) continue;
        if (*slots[i]) {
            nimcp_gpu_tensor_destroy(*slots[i]);
            *slots[i] = NULL;
        }
        *slots[i] = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (!*slots[i]) {
            LOG_ERROR("alloc_cb_arrays: %s tensor create failed (n=%zu)",
                      names[i], n_neurons);
            // Best-effort rollback to avoid partial allocation.
            for (int j = 0; j < i; j++) {
                if (*slots[j]) {
                    nimcp_gpu_tensor_destroy(*slots[j]);
                    *slots[j] = NULL;
                }
            }
            return false;
        }
    }
    state->cb_arrays_dirty = true;
    return true;
}

void nimcp_gpu_lif_state_free_cb_arrays(nimcp_lif_state_t* state)
{
    if (!state) return;
    if (state->g_ampa)   { nimcp_gpu_tensor_destroy(state->g_ampa);   state->g_ampa   = NULL; }
    if (state->g_nmda)   { nimcp_gpu_tensor_destroy(state->g_nmda);   state->g_nmda   = NULL; }
    if (state->g_gaba_a) { nimcp_gpu_tensor_destroy(state->g_gaba_a); state->g_gaba_a = NULL; }
    if (state->g_gaba_b) { nimcp_gpu_tensor_destroy(state->g_gaba_b); state->g_gaba_b = NULL; }
    state->cb_arrays_dirty = false;
}

bool nimcp_gpu_lif_state_upload_g(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const float* g_ampa_h,
    const float* g_nmda_h,
    const float* g_gaba_a_h,
    const float* g_gaba_b_h,
    size_t n_neurons)
{
    if (!ctx || !state) return false;
    struct { nimcp_gpu_tensor_t* t; const float* h; const char* name; } pairs[4] = {
        { state->g_ampa,   g_ampa_h,   "g_ampa"   },
        { state->g_nmda,   g_nmda_h,   "g_nmda"   },
        { state->g_gaba_a, g_gaba_a_h, "g_gaba_a" },
        { state->g_gaba_b, g_gaba_b_h, "g_gaba_b" },
    };
    for (int i = 0; i < 4; i++) {
        if (!pairs[i].h) continue;          // skip — caller doesn't have this receptor
        if (!pairs[i].t) {
            LOG_ERROR("upload_g: %s tensor not allocated (call alloc_cb_arrays first)",
                      pairs[i].name);
            return false;
        }
        if (pairs[i].t->numel != n_neurons) {
            LOG_ERROR("upload_g: %s size mismatch (tensor=%zu host=%zu)",
                      pairs[i].name, (size_t)pairs[i].t->numel, n_neurons);
            return false;
        }
        if (!nimcp_gpu_tensor_upload(pairs[i].t, pairs[i].h)) {
            LOG_ERROR("upload_g: %s upload failed", pairs[i].name);
            return false;
        }
    }
    state->cb_arrays_dirty = false;
    return true;
}

bool nimcp_gpu_lif_state_download_g(
    nimcp_gpu_context_t* ctx,
    const nimcp_lif_state_t* state,
    float* g_ampa_h,
    float* g_nmda_h,
    float* g_gaba_a_h,
    float* g_gaba_b_h,
    size_t n_neurons)
{
    if (!ctx || !state) return false;
    struct { const nimcp_gpu_tensor_t* t; float* h; const char* name; } pairs[4] = {
        { state->g_ampa,   g_ampa_h,   "g_ampa"   },
        { state->g_nmda,   g_nmda_h,   "g_nmda"   },
        { state->g_gaba_a, g_gaba_a_h, "g_gaba_a" },
        { state->g_gaba_b, g_gaba_b_h, "g_gaba_b" },
    };
    for (int i = 0; i < 4; i++) {
        if (!pairs[i].h || !pairs[i].t) continue;
        if (pairs[i].t->numel != n_neurons) {
            LOG_ERROR("download_g: %s size mismatch (tensor=%zu host=%zu)",
                      pairs[i].name, (size_t)pairs[i].t->numel, n_neurons);
            return false;
        }
        if (!nimcp_gpu_tensor_to_host(pairs[i].t, pairs[i].h)) {
            LOG_ERROR("download_g: %s download failed", pairs[i].name);
            return false;
        }
    }
    return true;
}

bool nimcp_gpu_lif_state_upload_per_neuron_params(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const float* tau_mem_host,
    const float* v_thresh_host,
    size_t n_neurons)
{
    if (!ctx || !state || !state->v) {
        LOG_ERROR("nimcp_gpu_lif_state_upload_per_neuron_params: invalid args");
        return false;
    }
    // Size sanity — must match the existing membrane buffer.
    if (state->v->numel != n_neurons) {
        LOG_ERROR("nimcp_gpu_lif_state_upload_per_neuron_params: n_neurons "
                  "(%zu) does not match state->v->numel (%zu)",
                  n_neurons, (size_t)state->v->numel);
        return false;
    }

    size_t dims[1] = { n_neurons };

    // τ_mem: NULL → free the device buffer (revert to scalar).
    //        non-NULL → allocate-if-needed then upload.
    if (tau_mem_host == NULL) {
        if (state->tau_mem_per_neuron) {
            nimcp_gpu_tensor_destroy(state->tau_mem_per_neuron);
            state->tau_mem_per_neuron = NULL;
        }
    } else {
        if (!state->tau_mem_per_neuron) {
            state->tau_mem_per_neuron = nimcp_gpu_tensor_create(
                ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
            if (!state->tau_mem_per_neuron) {
                LOG_ERROR("upload_per_neuron_params: τ_mem tensor create failed");
                return false;
            }
        }
        if (!nimcp_gpu_tensor_upload(state->tau_mem_per_neuron, tau_mem_host)) {
            LOG_ERROR("upload_per_neuron_params: τ_mem upload failed");
            return false;
        }
    }

    // v_thresh: same pattern.
    if (v_thresh_host == NULL) {
        if (state->v_thresh_per_neuron) {
            nimcp_gpu_tensor_destroy(state->v_thresh_per_neuron);
            state->v_thresh_per_neuron = NULL;
        }
    } else {
        if (!state->v_thresh_per_neuron) {
            state->v_thresh_per_neuron = nimcp_gpu_tensor_create(
                ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
            if (!state->v_thresh_per_neuron) {
                LOG_ERROR("upload_per_neuron_params: v_thresh tensor create failed");
                return false;
            }
        }
        if (!nimcp_gpu_tensor_upload(state->v_thresh_per_neuron, v_thresh_host)) {
            LOG_ERROR("upload_per_neuron_params: v_thresh upload failed");
            return false;
        }
    }

    return true;
}

//=============================================================================
// LIF Forward Kernel
//=============================================================================

/**
 * @brief GPU kernel: Compute I_syn from CSR synapse storage + flat spike vector
 *
 * Each thread handles one destination neuron. It reads the CSR row,
 * gathers presynaptic spike values from the flat spike vector, and
 * accumulates weighted contributions.
 *
 * @param spike_vector    [total_neurons] flattened spikes (all populations)
 * @param weights         [nnz] CSR synapse weights
 * @param col_indices     [nnz] flat global column indices into spike_vector
 * @param row_ptr         [n_neurons+1] CSR row pointers
 * @param external_current [n_neurons] per-neuron external input
 * @param output_isyn     [n_neurons] output synaptic current
 * @param n_neurons       number of neurons in this population
 */
__global__ void kernel_snn_isyn_csr(
    const float* __restrict__ spike_vector,
    const float* __restrict__ weights,
    const unsigned int* __restrict__ col_indices,
    const unsigned int* __restrict__ row_ptr,
    const float* __restrict__ external_current,
    float* __restrict__ output_isyn,
    size_t n_neurons)
{
    size_t n = blockIdx.x * blockDim.x + threadIdx.x;
    if (n >= n_neurons) return;

    float i_syn = external_current[n];

    unsigned int start = row_ptr[n];
    unsigned int end   = row_ptr[n + 1];

    for (unsigned int j = start; j < end; j++) {
        unsigned int src_idx = col_indices[j];
        if (spike_vector[src_idx] > 0.5f) {
            i_syn += weights[j];
        }
    }

    output_isyn[n] = i_syn;
}

/**
 * @brief Host wrapper: compute I_syn for one population using GPU CSR kernel
 */
bool nimcp_gpu_snn_isyn_csr(
    nimcp_gpu_context_t* ctx,
    const float* d_spike_vector,
    const float* d_weights,
    const unsigned int* d_col_indices,
    const unsigned int* d_row_ptr,
    const float* d_external_current,
    float* d_output_isyn,
    size_t n_neurons)
{
    if (!ctx || !d_spike_vector || !d_weights || !d_col_indices ||
        !d_row_ptr || !d_external_current || !d_output_isyn || n_neurons == 0) {
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_snn_isyn_csr<<<GRID_SIZE(n_neurons), BLOCK_SIZE>>>(
        d_spike_vector, d_weights, d_col_indices, d_row_ptr,
        d_external_current, d_output_isyn, n_neurons);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// CB-GPU-7 — Conductance-based deposit kernel
//
// Replaces the per-neuron host CSR walk in snn_cb_deposit_neuron (which the
// CB-GPU-6 sub-phase timer pinned at dep_ms=3971/total=4007ms — 99% of step
// time). One thread per destination neuron walks its incoming CSR row,
// gathers presynaptic spikes from a flat all-pops spike vector, applies
// per-source short-term depression, and routes the weighted contribution to
// AMPA / NMDA / GABA_A / GABA_B based on a per-pop synapse_type table.
//
// Match the host helper's behavior at src/snn/nimcp_snn_network.c:941
// (snn_cb_deposit_neuron):
//   - SYNAPSE_GENERIC: sign-fallback. positive weight → AMPA, negative → GABA_A.
//   - SYNAPSE_AMPA / NMDA / GABA_A / GABA_B: deposit |weight| into named receptor.
//   - depression dampener: weight × (1 - depression[src_neuron])
//
// External current + basket inhibition + Poisson noise are NOT done here.
// They stay on the host (cheap: O(n_neurons) per pop, no CSR walk) and are
// folded into the resident d_g_* via a separate "host delta" upload step.
//=============================================================================

/**
 * @brief Spike-driven CB deposit for one destination population.
 *
 * @param d_spike_vector  [total_neurons] flat all-pops spike output this step
 * @param d_depression    [total_neurons] per-source depression (1 - dep applied)
 * @param d_weights       [nnz] CSR synapse weights for this dst pop
 * @param d_flat_col_idx  [nnz] flat global neuron index per synapse
 * @param d_src_pop_idx   [nnz] source population index per synapse (for routing)
 * @param d_row_ptr       [n_neurons_pop+1] CSR row offsets for this dst pop
 * @param d_synapse_type  [n_pops] synapse_type for each (src_pop → this dst_pop)
 * @param d_g_ampa        [total_neurons] flat AMPA conductance (additive write)
 * @param d_g_nmda        [total_neurons] flat NMDA conductance (additive write)
 * @param d_g_gaba_a      [total_neurons] flat GABA-A conductance (additive write)
 * @param d_g_gaba_b      [total_neurons] flat GABA-B conductance (additive write)
 * @param dst_pop_offset  flat offset where this dst pop's neurons start
 * @param n_neurons_pop   number of neurons in this dst pop
 *
 * @note The g_* writes use the dst_pop_offset to land in the correct slice
 *       of the shared lif_state arrays. NULL g_* pointers are skipped (the
 *       receptor doesn't exist for this network; matches the host helper's
 *       NULL-safety contract).
 */
__global__ void kernel_snn_cb_deposit_csr(
    const float*    __restrict__ d_spike_vector,
    const float*    __restrict__ d_depression,
    const float*    __restrict__ d_weights,
    const uint32_t* __restrict__ d_flat_col_idx,
    const uint32_t* __restrict__ d_src_pop_idx,
    const uint32_t* __restrict__ d_row_ptr,
    const uint8_t*  __restrict__ d_synapse_type,
    float*          __restrict__ d_g_ampa,
    float*          __restrict__ d_g_nmda,
    float*          __restrict__ d_g_gaba_a,
    float*          __restrict__ d_g_gaba_b,
    uint32_t dst_pop_offset,
    uint32_t n_neurons_pop)
{
    const uint32_t n = blockIdx.x * blockDim.x + threadIdx.x;
    if (n >= n_neurons_pop) return;

    /* Per-neuron deposit accumulators — written once at end so we hit the
     * shared d_g_* arrays four times max (not once per synapse). */
    float dep_ampa = 0.0f, dep_nmda = 0.0f, dep_gaba_a = 0.0f, dep_gaba_b = 0.0f;

    const uint32_t row_start = d_row_ptr[n];
    const uint32_t row_end   = d_row_ptr[n + 1];

    for (uint32_t s = row_start; s < row_end; s++) {
        const uint32_t src_idx = d_flat_col_idx[s];
        if (d_spike_vector[src_idx] <= 0.5f) continue;  // pre didn't spike

        // Effective weight: synaptic weight × (1 - short-term depression).
        // d_depression is per-source-neuron; flat-indexed.
        const float dep_factor = (d_depression != nullptr)
                               ? (1.0f - d_depression[src_idx]) : 1.0f;
        const float w_eff = d_weights[s] * dep_factor;

        // Route by per-(src_pop → this dst_pop) type table.
        const uint32_t src_pop = d_src_pop_idx[s];
        const uint8_t  st      = d_synapse_type[src_pop];

        /* Match host snn_membrane_deposit_synapse exactly: AMPA/NMDA take the
         * SIGNED weight (negative-weight excitatory edges are legal during
         * CB rescale); GABA_A/GABA_B are rectified (synaptic conductance must
         * be ≥ 0; the inhibitory effect comes from the reversal potential).
         * GENERIC + neuromodulators (DA/5HT/ACh) + ELECTRICAL fall through to
         * sign-routing — positive→AMPA, negative→GABA_A. */
        switch (st) {
            case 1: /* SYNAPSE_AMPA */
                dep_ampa   += w_eff;
                break;
            case 2: /* SYNAPSE_NMDA */
                dep_nmda   += w_eff;
                break;
            case 3: /* SYNAPSE_GABA_A */
                dep_gaba_a += (w_eff < 0.0f) ? -w_eff : w_eff;
                break;
            case 4: /* SYNAPSE_GABA_B */
                dep_gaba_b += (w_eff < 0.0f) ? -w_eff : w_eff;
                break;
            default:  /* SYNAPSE_GENERIC = 0, neuromodulators, ELECTRICAL */
                if (w_eff >= 0.0f) dep_ampa   += w_eff;
                else               dep_gaba_a += -w_eff;
                break;
        }
    }

    // Single coalesced write per receptor — additive (kernel runs ON TOP of
    // the resident post-decay g_* state from the previous forward_cb call).
    const uint32_t out_idx = dst_pop_offset + n;
    if (d_g_ampa   && dep_ampa   != 0.0f) d_g_ampa  [out_idx] += dep_ampa;
    if (d_g_nmda   && dep_nmda   != 0.0f) d_g_nmda  [out_idx] += dep_nmda;
    if (d_g_gaba_a && dep_gaba_a != 0.0f) d_g_gaba_a[out_idx] += dep_gaba_a;
    if (d_g_gaba_b && dep_gaba_b != 0.0f) d_g_gaba_b[out_idx] += dep_gaba_b;
}

/**
 * @brief C wrapper: launch the CB deposit kernel for one destination pop.
 *
 * Caller must ensure all device pointers are valid for the duration of the
 * launch. d_g_* may be NULL (receptor not allocated). NULL-safe at the
 * pop-level pointer boundary (returns false on null required arg).
 *
 * **Caller contracts (load-bearing):**
 *
 *  1. **Delayed spike ring (Wave E FFI).** The host helper
 *     snn_cb_deposit_neuron reads `snn_pop_get_delayed_spike_row(src_pop)`
 *     when present, falling back to `src_pop->spike_output` only when the
 *     delayed row is NULL. The kernel only sees `d_spike_vector` — the
 *     caller MUST pre-mux per-pop slices using the same delayed-row
 *     selection rule, otherwise synaptic delay is silently lost.
 *
 *  2. **Dendritic mode is not supported on this kernel.** When the dst
 *     pop runs the Wave H two-compartment integrator (basal/apical), the
 *     host helper writes to `g_*_basal` / `g_*_apical` instead of the
 *     legacy single-compartment `g_*`. The kernel only writes the legacy
 *     receptors. The dispatcher MUST exclude dendritic-mode pops from the
 *     CB-GPU path — already enforced in snn_network_step_cb_gpu via the
 *     `pop->dendritic_enabled` early-out at line ~1371.
 *
 *  3. **No pre-zeroing.** The kernel is purely additive on top of the
 *     resident post-decay g_* state. Caller must NOT call
 *     `cudaMemsetAsync(d_g_*, 0, ...)` before this kernel — that would
 *     wipe the carry-over state from the previous step's forward_cb decay.
 *
 *  4. **NULL g_* policy.** If all four d_g_* are NULL, the launch is a
 *     no-op — wasted work but not a correctness bug. Returns false to
 *     signal the no-op (so callers can detect mis-configured networks).
 */
extern "C" bool nimcp_gpu_snn_cb_deposit_csr(
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
    uint32_t n_neurons_pop)
{
    if (!ctx || !d_spike_vector || !d_weights || !d_flat_col_idx ||
        !d_src_pop_idx || !d_row_ptr || !d_synapse_type ||
        n_neurons_pop == 0) {
        return false;
    }
    /* uint32 overflow guard: out_idx = dst_pop_offset + n where n is in
     * [0, n_neurons_pop). Reject configurations that would wrap. The
     * production network maxes out around 2M neurons total — far below
     * the limit — so this catches mis-passed offsets, not size pressure. */
    if ((uint64_t)dst_pop_offset + (uint64_t)n_neurons_pop > 0xFFFFFFFFu) {
        return false;
    }
    /* All-NULL receptor pointers means the network has no CB conductance
     * arrays allocated — caller should be on the current-based path. Reject
     * the launch (paralleling nimcp_gpu_lif_forward_cb's check at the same
     * file's CB-forward wrapper). */
    if (!d_g_ampa && !d_g_nmda && !d_g_gaba_a && !d_g_gaba_b) {
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_snn_cb_deposit_csr<<<GRID_SIZE(n_neurons_pop), BLOCK_SIZE>>>(
        d_spike_vector, d_depression, d_weights, d_flat_col_idx,
        d_src_pop_idx, d_row_ptr, d_synapse_type,
        d_g_ampa, d_g_nmda, d_g_gaba_a, d_g_gaba_b,
        dst_pop_offset, n_neurons_pop);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// LIF Forward Kernel
//=============================================================================

/* Wave G GPU sync (v17): kernel accepts optional per-neuron τ_mem / v_thresh
 * arrays. NULL → use the scalar argument (bit-identical pre-Wave-G path).
 * non-NULL → load tau_mem[idx] / v_thresh[idx] for each thread.
 *
 * The per-neuron arrays carry the LIF profile that already includes
 * subclass deltas (PV τ=10 ms, SOM τ=30 ms, L5 Betz v_thresh −2 mV, etc.)
 * AND per-neuron heterogeneity σ noise — both stages composed host-side
 * via snn_pop_neuron_lif_params() and uploaded once per (re)build or
 * heterogeneity-change event. */
__global__ void kernel_lif_forward(
    float* v, float* i_syn, float* spikes, const float* input,
    float tau_mem, float tau_syn, float v_thresh, float v_reset, float v_rest,
    float dt, bool hard_reset, size_t n,
    const float* __restrict__ tau_mem_per_neuron,
    const float* __restrict__ v_thresh_per_neuron)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Per-neuron resolution — NULL pointer collapses to scalar (no extra
    // global memory traffic in the homogeneous case).
    const float tau_mem_n  = tau_mem_per_neuron  ? tau_mem_per_neuron[idx]  : tau_mem;
    const float v_thresh_n = v_thresh_per_neuron ? v_thresh_per_neuron[idx] : v_thresh;

    // Decay factors
    float alpha_mem = expf(-dt / tau_mem_n);
    float alpha_syn = expf(-dt / tau_syn);

    // Update synaptic current
    float i = i_syn[idx] * alpha_syn + input[idx];
    i_syn[idx] = i;

    // Update membrane potential
    float membrane = v[idx];
    membrane = alpha_mem * membrane + (1.0f - alpha_mem) * (v_rest + i);

    // Check for spike
    float spike = 0.0f;
    if (membrane >= v_thresh_n) {
        spike = 1.0f;
        if (hard_reset) {
            membrane = v_reset;
        } else {
            membrane = membrane - (v_thresh_n - v_reset);
        }
    }

    v[idx] = membrane;
    spikes[idx] = spike;
}

/**
 * Simplified LIF kernel when synaptic current buffer is not available.
 * Uses input directly as current (no synaptic filtering).
 */
__global__ void kernel_lif_forward_simple(
    float* __restrict__ v,
    float* __restrict__ spikes,
    const float* __restrict__ input,
    float tau_mem, float v_thresh, float v_reset, float v_rest,
    float dt, bool hard_reset, size_t n,
    const float* __restrict__ tau_mem_per_neuron,
    const float* __restrict__ v_thresh_per_neuron)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Per-neuron resolution — see kernel_lif_forward for rationale.
    const float tau_mem_n  = tau_mem_per_neuron  ? tau_mem_per_neuron[idx]  : tau_mem;
    const float v_thresh_n = v_thresh_per_neuron ? v_thresh_per_neuron[idx] : v_thresh;

    // Decay factor for membrane
    float alpha_mem = expf(-dt / tau_mem_n);

    // Update membrane potential (input used directly as current)
    float membrane = v[idx];
    membrane = alpha_mem * membrane + (1.0f - alpha_mem) * (v_rest + input[idx]);

    // Check for spike
    float spike = 0.0f;
    if (membrane >= v_thresh_n) {
        spike = 1.0f;
        if (hard_reset) {
            membrane = v_reset;
        } else {
            membrane = membrane - (v_thresh_n - v_reset);
        }
    }

    v[idx] = membrane;
    spikes[idx] = spike;
}

bool nimcp_gpu_lif_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    if (!ctx || !state || !input) return false;
    if (!state->v || !state->spikes) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = state->v->numel;
    const nimcp_lif_params_t* p = &state->params;

    // Wave G GPU sync (v17): pass optional per-neuron arrays (NULL = scalar
    // fallback, pre-Wave-G bit-identical).
    const float* d_tau_per_neuron =
        state->tau_mem_per_neuron
            ? (const float*)state->tau_mem_per_neuron->data
            : NULL;
    const float* d_vthr_per_neuron =
        state->v_thresh_per_neuron
            ? (const float*)state->v_thresh_per_neuron->data
            : NULL;

    if (state->i_syn != NULL) {
        // Full LIF with synaptic current integration
        kernel_lif_forward<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (float*)state->v->data, (float*)state->i_syn->data,
            (float*)state->spikes->data, (const float*)input->data,
            p->tau_mem, p->tau_syn, p->v_thresh, p->v_reset, p->v_rest,
            p->dt, p->hard_reset, n,
            d_tau_per_neuron, d_vthr_per_neuron);
    } else {
        // Simplified LIF without synaptic current (input used directly)
        kernel_lif_forward_simple<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (float*)state->v->data, (float*)state->spikes->data,
            (const float*)input->data,
            p->tau_mem, p->v_thresh, p->v_reset, p->v_rest,
            p->dt, p->hard_reset, n,
            d_tau_per_neuron, d_vthr_per_neuron);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// CB Phase 5 — Conductance-based LIF kernel
//
// Mirrors snn_membrane_compute_dv() (CPU, conductance_mode=true) bit-for-bit:
//   nmda_block = 1 / (1 + Mg/3.57 * exp(-V/16.13))
//   drive      = (V_rest - V)
//              + g_ampa             * (E_ampa  - V)
//              + g_nmda * nmda_block * (E_nmda - V)
//              + g_gaba_a           * (E_gaba_a - V)
//              + g_gaba_b           * (E_gaba_b - V)
//   dv         = clamp(drive * dt / tau_eff, ±100 mV)
//   dv         = bound(V+dv ∈ [E_min, E_max])
//   V         += dv
//   spike if V >= V_thresh; reset
//   g_r       *= decay_r          (after membrane update; matches CPU ordering)
//
// The decay factors are precomputed once per step on the host and passed in,
// matching the CPU per-pop loop's optimization (avoid expf() per neuron).
//=============================================================================

__device__ inline float cb_nmda_mg_block(float v_mv, float mg_mm) {
    // Standard Jahr-Stevens formula: 1 / (1 + [Mg]/3.57 * exp(-V/16.13))
    return 1.0f / (1.0f + (mg_mm / 3.57f) * expf(-v_mv / 16.13f));
}

__global__ void kernel_lif_forward_cb(
    float* __restrict__ v,
    float* __restrict__ spikes,
    float* __restrict__ g_ampa,
    float* __restrict__ g_nmda,
    float* __restrict__ g_gaba_a,
    float* __restrict__ g_gaba_b,
    float v_rest, float v_thresh, float v_reset,
    float tau_mem, float dt,
    bool hard_reset,
    float e_ampa, float e_nmda, float e_gaba_a, float e_gaba_b,
    float mg_mm,
    float decay_ampa, float decay_nmda, float decay_gaba_a, float decay_gaba_b,
    size_t n,
    const float* __restrict__ tau_mem_per_neuron,
    const float* __restrict__ v_thresh_per_neuron)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    const float tau_n  = tau_mem_per_neuron  ? tau_mem_per_neuron[idx]  : tau_mem;
    const float vthr_n = v_thresh_per_neuron ? v_thresh_per_neuron[idx] : v_thresh;
    const float tau_eff = (tau_n < 1e-3f) ? 1e-3f : tau_n;

    // Per-neuron g — NULL pointer means "this receptor not allocated", treat as 0.
    const float ga = g_ampa   ? g_ampa[idx]   : 0.0f;
    const float gn = g_nmda   ? g_nmda[idx]   : 0.0f;
    const float ga_a = g_gaba_a ? g_gaba_a[idx] : 0.0f;
    const float ga_b = g_gaba_b ? g_gaba_b[idx] : 0.0f;

    float V = v[idx];
    const float nmda_b = cb_nmda_mg_block(V, mg_mm);

    const float leak = (v_rest - V);
    const float drive = leak
                      + ga              * (e_ampa   - V)
                      + gn  * nmda_b    * (e_nmda   - V)
                      + ga_a            * (e_gaba_a - V)
                      + ga_b            * (e_gaba_b - V);

    float dv = drive * (dt / tau_eff);

    // Biophysical clamp: |dv| <= 100 mV per step.
    if (dv >  100.0f) dv =  100.0f;
    if (dv < -100.0f) dv = -100.0f;

    // Reversal-potential bound: V + dv must stay within [E_min, E_max].
    {
        const float v_after = V + dv;
        const float e_max = (e_ampa  > e_nmda)  ? e_ampa  : e_nmda;
        const float e_min = (e_gaba_a < e_gaba_b) ? e_gaba_a : e_gaba_b;
        if (v_after > e_max) dv = e_max - V;
        if (v_after < e_min) dv = e_min - V;
    }

    V += dv;

    float spike = 0.0f;
    if (V >= vthr_n) {
        spike = 1.0f;
        if (hard_reset) {
            V = v_reset;
        } else {
            V = V - (vthr_n - v_reset);
        }
    }
    v[idx] = V;
    spikes[idx] = spike;

    // Per-receptor exponential decay — matches CPU snn_membrane_decay_one.
    if (g_ampa)   g_ampa[idx]   = ga   * decay_ampa;
    if (g_nmda)   g_nmda[idx]   = gn   * decay_nmda;
    if (g_gaba_a) g_gaba_a[idx] = ga_a * decay_gaba_a;
    if (g_gaba_b) g_gaba_b[idx] = ga_b * decay_gaba_b;
}

bool nimcp_gpu_lif_forward_cb(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    float e_ampa_mv,   float e_nmda_mv,
    float e_gaba_a_mv, float e_gaba_b_mv,
    float tau_ampa_ms,   float tau_nmda_ms,
    float tau_gaba_a_ms, float tau_gaba_b_ms,
    float mg_mm)
{
    if (!ctx || !state || !state->v || !state->spikes) {
        LOG_ERROR("nimcp_gpu_lif_forward_cb: invalid args");
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = state->v->numel;
    const nimcp_lif_params_t* p = &state->params;

    // At least one receptor must be allocated, else the caller should be
    // using nimcp_gpu_lif_forward (current-based path).
    if (!state->g_ampa && !state->g_nmda && !state->g_gaba_a && !state->g_gaba_b) {
        LOG_ERROR("nimcp_gpu_lif_forward_cb: no g_* arrays allocated — call "
                  "nimcp_gpu_lif_state_alloc_cb_arrays first");
        return false;
    }

    const float* d_tau  = state->tau_mem_per_neuron
                            ? (const float*)state->tau_mem_per_neuron->data : NULL;
    const float* d_vthr = state->v_thresh_per_neuron
                            ? (const float*)state->v_thresh_per_neuron->data : NULL;

    // Decay factors precomputed host-side: matches CPU snn_membrane_decay_one
    // contract (exp(-dt/tau) per receptor, computed once per step).
    auto safe_decay = [](float dt, float tau_ms) -> float {
        const float t = (tau_ms < 1e-3f) ? 1e-3f : tau_ms;
        return expf(-dt / t);
    };
    const float decay_ampa   = safe_decay(p->dt, tau_ampa_ms);
    const float decay_nmda   = safe_decay(p->dt, tau_nmda_ms);
    const float decay_gaba_a = safe_decay(p->dt, tau_gaba_a_ms);
    const float decay_gaba_b = safe_decay(p->dt, tau_gaba_b_ms);

    kernel_lif_forward_cb<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->v->data,
        (float*)state->spikes->data,
        state->g_ampa   ? (float*)state->g_ampa->data   : NULL,
        state->g_nmda   ? (float*)state->g_nmda->data   : NULL,
        state->g_gaba_a ? (float*)state->g_gaba_a->data : NULL,
        state->g_gaba_b ? (float*)state->g_gaba_b->data : NULL,
        p->v_rest, p->v_thresh, p->v_reset,
        p->tau_mem, p->dt, p->hard_reset,
        e_ampa_mv, e_nmda_mv, e_gaba_a_mv, e_gaba_b_mv, mg_mm,
        decay_ampa, decay_nmda, decay_gaba_a, decay_gaba_b,
        n, d_tau, d_vthr);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Surrogate Gradient Kernels
//=============================================================================

__device__ inline float surrogate_superspike(float x, float beta)
{
    // SuperSpike: 1 / (1 + beta * |x|)^2
    float abs_x = fabsf(x);
    float denom = 1.0f + beta * abs_x;
    return 1.0f / (denom * denom);
}

__device__ inline float surrogate_fast_sigmoid(float x, float beta)
{
    // Fast sigmoid: 0.5 * beta / (1 + beta * |x|)
    return 0.5f * beta / (1.0f + beta * fabsf(x));
}

__device__ inline float surrogate_arctan(float x, float beta)
{
    // Arctan: beta / (pi * (1 + (beta * x)^2))
    float bx = beta * x;
    return beta / (NIMCP_PI_F * (1.0f + bx * bx));
}

__device__ inline float surrogate_triangular(float x, float beta)
{
    // Triangular: max(0, 1 - beta * |x|)
    return fmaxf(0.0f, 1.0f - beta * fabsf(x));
}

__device__ inline float surrogate_gaussian(float x, float beta)
{
    // Gaussian: exp(-beta * x^2)
    return expf(-beta * x * x);
}

__global__ void kernel_surrogate_gradient(
    const float* v, float v_thresh, float* grad,
    int surrogate_type, float beta, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float x = v[idx] - v_thresh;
    float g;

    switch (surrogate_type) {
        case 0:  // SuperSpike
            g = surrogate_superspike(x, beta);
            break;
        case 1:  // Fast sigmoid
            g = surrogate_fast_sigmoid(x, beta);
            break;
        case 2:  // Arctan
            g = surrogate_arctan(x, beta);
            break;
        case 3:  // Triangular
            g = surrogate_triangular(x, beta);
            break;
        case 4:  // Gaussian
            g = surrogate_gaussian(x, beta);
            break;
        default:
            g = surrogate_superspike(x, beta);
    }

    grad[idx] = g;
}

bool nimcp_gpu_surrogate_gradient(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* v,
    float v_thresh,
    nimcp_gpu_tensor_t* grad,
    nimcp_surrogate_type_t surrogate_type,
    float beta)
{
    if (!ctx || !v || !grad) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_surrogate_gradient<<<GRID_SIZE(v->numel), BLOCK_SIZE>>>(
        (const float*)v->data, v_thresh, (float*)grad->data,
        (int)surrogate_type, beta, v->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// LIF Backward with Surrogate Gradient
//=============================================================================

__global__ void kernel_lif_backward(
    const float* v, float v_thresh, const float* grad_output, float* grad_input,
    int surrogate_type, float beta, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float x = v[idx] - v_thresh;
    float surrogate;

    switch (surrogate_type) {
        case 0:
            surrogate = surrogate_superspike(x, beta);
            break;
        case 1:
            surrogate = surrogate_fast_sigmoid(x, beta);
            break;
        case 2:
            surrogate = surrogate_arctan(x, beta);
            break;
        case 3:
            surrogate = surrogate_triangular(x, beta);
            break;
        case 4:
            surrogate = surrogate_gaussian(x, beta);
            break;
        default:
            surrogate = surrogate_superspike(x, beta);
    }

    grad_input[idx] = grad_output[idx] * surrogate;
}

bool nimcp_gpu_lif_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_surrogate_type_t surrogate_type,
    float beta)
{
    if (!ctx || !state || !grad_output || !grad_input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_lif_backward<<<GRID_SIZE(state->v->numel), BLOCK_SIZE>>>(
        (const float*)state->v->data, state->params.v_thresh,
        (const float*)grad_output->data, (float*)grad_input->data,
        (int)surrogate_type, beta, state->v->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Izhikevich Neuron State Management
//=============================================================================

nimcp_izhikevich_state_t* nimcp_izhikevich_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_izhikevich_params_t* params)
{
    if (!ctx || !params || n_neurons == 0) return NULL;

    nimcp_izhikevich_state_t* state = (nimcp_izhikevich_state_t*)nimcp_calloc(1, sizeof(nimcp_izhikevich_state_t));
    if (!state) return NULL;

    size_t dims[1] = {n_neurons};

    state->v = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->u = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->spikes = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->v || !state->u || !state->spikes) {
        nimcp_izhikevich_state_destroy(state);
        return NULL;
    }

    // Initialize to resting state (v=-65mV typical)
    nimcp_gpu_fill(ctx, state->v, -65.0f);
    nimcp_gpu_fill(ctx, state->u, params->b * (-65.0f));
    nimcp_gpu_zeros(ctx, state->spikes);

    state->params = *params;
    return state;
}

void nimcp_izhikevich_state_destroy(nimcp_izhikevich_state_t* state)
{
    if (!state) return;

    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->u) nimcp_gpu_tensor_destroy(state->u);
    if (state->spikes) nimcp_gpu_tensor_destroy(state->spikes);
    nimcp_free(state);
}

//=============================================================================
// Izhikevich Forward Kernel
//=============================================================================

__global__ void kernel_izhikevich_forward(
    float* v, float* u, float* spikes, const float* input,
    float a, float b, float c, float d, float v_thresh, float dt, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float membrane = v[idx];
    float recovery = u[idx];
    float I = input[idx];

    // Spike detection and reset FIRST (standard Izhikevich order)
    float spike = 0.0f;
    if (membrane >= v_thresh) {
        spike = 1.0f;
        membrane = c;
        recovery = recovery + d;
    }

    // Euler integration
    // dv/dt = 0.04*v^2 + 5*v + 140 - u + I
    float dv = (0.04f * membrane * membrane + 5.0f * membrane + 140.0f - recovery + I) * dt;

    // du/dt = a*(b*v - u)
    float du = a * (b * membrane - recovery) * dt;

    membrane += dv;
    recovery += du;

    v[idx] = membrane;
    u[idx] = recovery;
    spikes[idx] = spike;
}

bool nimcp_gpu_izhikevich_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_izhikevich_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    if (!ctx || !state || !input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = state->v->numel;
    const nimcp_izhikevich_params_t* p = &state->params;

    kernel_izhikevich_forward<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->v->data, (float*)state->u->data,
        (float*)state->spikes->data, (const float*)input->data,
        p->a, p->b, p->c, p->d, p->v_thresh, p->dt, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// AdEx Neuron State Management
//=============================================================================

nimcp_adex_state_t* nimcp_adex_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_adex_params_t* params)
{
    if (!ctx || !params || n_neurons == 0) return NULL;

    nimcp_adex_state_t* state = (nimcp_adex_state_t*)nimcp_calloc(1, sizeof(nimcp_adex_state_t));
    if (!state) return NULL;

    size_t dims[1] = {n_neurons};

    state->v = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->w = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->spikes = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->v || !state->w || !state->spikes) {
        nimcp_adex_state_destroy(state);
        return NULL;
    }

    nimcp_gpu_fill(ctx, state->v, params->v_rest);
    nimcp_gpu_zeros(ctx, state->w);
    nimcp_gpu_zeros(ctx, state->spikes);

    state->params = *params;
    return state;
}

void nimcp_adex_state_destroy(nimcp_adex_state_t* state)
{
    if (!state) return;

    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->w) nimcp_gpu_tensor_destroy(state->w);
    if (state->spikes) nimcp_gpu_tensor_destroy(state->spikes);
    nimcp_free(state);
}

//=============================================================================
// AdEx Forward Kernel
//=============================================================================

__global__ void kernel_adex_forward(
    float* v, float* w, float* spikes, const float* input,
    float tau_mem, float tau_w, float v_thresh, float v_reset, float v_rest,
    float v_rheo, float delta_T, float a_sub, float b_spike, float dt, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float membrane = v[idx];
    float adaptation = w[idx];
    float I = input[idx];

    // Exponential term
    float exp_term = delta_T * expf((membrane - v_rheo) / delta_T);

    // dv/dt = (-v + v_rest + exp_term + I - w) / tau_mem
    float dv = (-(membrane - v_rest) + exp_term - adaptation + I) / tau_mem;

    // dw/dt = (a*(v - v_rest) - w) / tau_w
    float dw = (a_sub * (membrane - v_rest) - adaptation) / tau_w;

    membrane += dt * dv;
    adaptation += dt * dw;

    // Spike detection
    float spike = 0.0f;
    if (membrane >= v_thresh) {
        spike = 1.0f;
        membrane = v_reset;
        adaptation += b_spike;
    }

    v[idx] = membrane;
    w[idx] = adaptation;
    spikes[idx] = spike;
}

bool nimcp_gpu_adex_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_adex_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    if (!ctx || !state || !input) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n = state->v->numel;
    const nimcp_adex_params_t* p = &state->params;

    kernel_adex_forward<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->v->data, (float*)state->w->data,
        (float*)state->spikes->data, (const float*)input->data,
        p->tau_mem, p->tau_w, p->v_thresh, p->v_reset, p->v_rest,
        p->v_rheo, p->delta_T, p->a, p->b, p->dt, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Spike Propagation
//=============================================================================

__global__ void kernel_spike_propagate(
    const float* spikes, const float* weights, float* output,
    size_t n_pre, size_t n_post)
{
    size_t post_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (post_idx >= n_post) return;

    float sum = 0.0f;
    for (size_t pre_idx = 0; pre_idx < n_pre; pre_idx++) {
        if (spikes[pre_idx] > 0.0f) {
            sum += weights[pre_idx * n_post + post_idx];
        }
    }
    output[post_idx] += sum;
}

bool nimcp_gpu_spike_propagate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* output)
{
    if (!ctx || !spikes || !weights || !output) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n_pre = spikes->numel;
    size_t n_post = output->numel;

    kernel_spike_propagate<<<GRID_SIZE(n_post), BLOCK_SIZE>>>(
        (const float*)spikes->data, (const float*)weights->data,
        (float*)output->data, n_pre, n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * Sparse spike propagation kernel
 * Only sums weights for neurons that actually spiked
 */
__global__ void kernel_spike_propagate_sparse(
    const uint32_t* spike_indices, size_t n_spikes,
    const float* weights, float* output,
    size_t n_pre, size_t n_post)
{
    size_t post_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (post_idx >= n_post) return;

    float sum = 0.0f;
    for (size_t i = 0; i < n_spikes; i++) {
        uint32_t pre_idx = spike_indices[i];
        if (pre_idx < n_pre) {
            sum += weights[pre_idx * n_post + post_idx];
        }
    }
    output[post_idx] += sum;
}

bool nimcp_gpu_spike_propagate_sparse(
    nimcp_gpu_context_t* ctx,
    const uint32_t* spike_indices,
    size_t n_spikes,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* output)
{
    if (!ctx || !weights || !output) return false;
    if (n_spikes == 0) return true;  // No spikes, nothing to do
    if (!spike_indices) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Infer dimensions from weight matrix (n_pre x n_post)
    size_t n_pre = weights->dims[0];
    size_t n_post = weights->dims[1];

    // Copy spike indices to GPU
    uint32_t* d_indices = NULL;
    cudaMalloc(&d_indices, n_spikes * sizeof(uint32_t));
    cudaMemcpy(d_indices, spike_indices, n_spikes * sizeof(uint32_t), cudaMemcpyHostToDevice);

    kernel_spike_propagate_sparse<<<GRID_SIZE(n_post), BLOCK_SIZE>>>(
        d_indices, n_spikes,
        (const float*)weights->data, (float*)output->data,
        n_pre, n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    cudaFree(d_indices);
    return true;
}

//=============================================================================
// STDP Learning
//=============================================================================

__global__ void kernel_eligibility_trace_update(
    float* trace, const float* spikes, float decay, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    trace[idx] = trace[idx] * decay + spikes[idx] * (1.0f - decay);
}

bool nimcp_gpu_eligibility_trace_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* trace,
    const nimcp_gpu_tensor_t* spikes,
    float decay)
{
    if (!ctx || !trace || !spikes) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_eligibility_trace_update<<<GRID_SIZE(trace->numel), BLOCK_SIZE>>>(
        (float*)trace->data, (const float*)spikes->data, decay, trace->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__global__ void kernel_stdp_pair(
    float* weights,
    const float* pre_spikes, const float* post_spikes,
    const float* pre_trace, const float* post_trace,
    float A_plus, float A_minus, float w_max, float w_min,
    size_t n_pre, size_t n_post)
{
    size_t pre_idx = blockIdx.x;
    size_t post_idx = threadIdx.x;

    if (pre_idx >= n_pre || post_idx >= n_post) return;

    size_t w_idx = pre_idx * n_post + post_idx;
    float w = weights[w_idx];

    float pre_spike = pre_spikes[pre_idx];
    float post_spike = post_spikes[post_idx];
    float pre_tr = pre_trace[pre_idx];
    float post_tr = post_trace[post_idx];

    // LTP: Post spike with pre trace (pre before post)
    float dw_ltp = A_plus * post_spike * pre_tr * (w_max - w);

    // LTD: Pre spike with post trace (post before pre)
    float dw_ltd = A_minus * pre_spike * post_tr * (w - w_min);

    w = fminf(fmaxf(w + dw_ltp - dw_ltd, w_min), w_max);
    weights[w_idx] = w;
}

bool nimcp_gpu_stdp_pair(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace,
    const nimcp_gpu_tensor_t* post_trace,
    const nimcp_stdp_params_t* params)
{
    if (!ctx || !weights || !pre_spikes || !post_spikes || !pre_trace || !post_trace || !params) {
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n_pre = pre_spikes->numel;
    size_t n_post = post_spikes->numel;

    // Launch with pre neurons as blocks, post neurons as threads
    dim3 grid(n_pre);
    dim3 block(n_post < 256 ? n_post : 256);

    kernel_stdp_pair<<<grid, block>>>(
        (float*)weights->data,
        (const float*)pre_spikes->data, (const float*)post_spikes->data,
        (const float*)pre_trace->data, (const float*)post_trace->data,
        params->A_plus, params->A_minus, params->w_max, params->w_min,
        n_pre, n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Triplet STDP Kernels (Pfister & Gerstner 2006)
//=============================================================================

/**
 * @brief Decay all traces by exponential factor
 *
 * Applies trace = trace * exp(-dt/tau) element-wise
 */
__global__ void kernel_decay_traces(float* traces, size_t n, float decay_factor)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    traces[idx] *= decay_factor;
}

/**
 * @brief Update presynaptic traces on spike
 *
 * For each spiking presynaptic neuron:
 *   r1[i] += 1.0  (fast trace for pair-based term)
 *   r2[i] += 1.0  (slow trace for triplet term)
 */
__global__ void kernel_update_presynaptic_traces(
    float* r1, float* r2,
    const float* spikes, size_t n,
    float tau_plus, float tau_x, float dt)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Decay factors
    float decay_r1 = expf(-dt / tau_plus);
    float decay_r2 = expf(-dt / tau_x);

    // Decay then add spike
    float spike = spikes[idx];
    r1[idx] = r1[idx] * decay_r1 + spike;
    r2[idx] = r2[idx] * decay_r2 + spike;
}

/**
 * @brief Update postsynaptic traces on spike
 *
 * For each spiking postsynaptic neuron:
 *   o1[j] += 1.0  (fast trace for pair-based term)
 *   o2[j] += 1.0  (slow trace for triplet term)
 */
__global__ void kernel_update_postsynaptic_traces(
    float* o1, float* o2,
    const float* spikes, size_t n,
    float tau_minus, float tau_y, float dt)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Decay factors
    float decay_o1 = expf(-dt / tau_minus);
    float decay_o2 = expf(-dt / tau_y);

    // Decay then add spike
    float spike = spikes[idx];
    o1[idx] = o1[idx] * decay_o1 + spike;
    o2[idx] = o2[idx] * decay_o2 + spike;
}

/**
 * @brief Triplet STDP LTP kernel
 *
 * When postsynaptic neuron fires:
 *   dw = (A2_plus + A3_plus * o2_before) * r1 * post_spike
 *
 * Note: o2_before is the slow post trace BEFORE the spike is added
 */
__global__ void kernel_triplet_stdp_ltp(
    float* weights,
    const float* r1, const float* o2,
    const float* post_spikes,
    size_t n_pre, size_t n_post,
    float A2_plus, float A3_plus,
    float w_min, float w_max,
    float learning_rate)
{
    size_t pre_idx = blockIdx.x;
    size_t post_idx = threadIdx.x + blockIdx.y * blockDim.x;

    if (pre_idx >= n_pre || post_idx >= n_post) return;

    float post_spike = post_spikes[post_idx];
    if (post_spike < 0.5f) return;  // No post spike, no LTP

    size_t w_idx = pre_idx * n_post + post_idx;

    float pre_trace = r1[pre_idx];
    float post_trace_slow = o2[post_idx];  // o2 before spike was added

    // Triplet LTP: pair term + triplet term
    float dw = learning_rate * (A2_plus + A3_plus * post_trace_slow) * pre_trace;

    // Apply bounded update
    float w = weights[w_idx];
    w = fminf(fmaxf(w + dw, w_min), w_max);
    weights[w_idx] = w;
}

/**
 * @brief Triplet STDP LTD kernel
 *
 * When presynaptic neuron fires:
 *   dw = -(A2_minus + A3_minus * r2_before) * o1 * pre_spike
 *
 * Note: r2_before is the slow pre trace BEFORE the spike is added
 */
__global__ void kernel_triplet_stdp_ltd(
    float* weights,
    const float* o1, const float* r2,
    const float* pre_spikes,
    size_t n_pre, size_t n_post,
    float A2_minus, float A3_minus,
    float w_min, float w_max,
    float learning_rate)
{
    size_t pre_idx = blockIdx.x;
    size_t post_idx = threadIdx.x + blockIdx.y * blockDim.x;

    if (pre_idx >= n_pre || post_idx >= n_post) return;

    float pre_spike = pre_spikes[pre_idx];
    if (pre_spike < 0.5f) return;  // No pre spike, no LTD

    size_t w_idx = pre_idx * n_post + post_idx;

    float post_trace_fast = o1[post_idx];
    float pre_trace_slow = r2[pre_idx];  // r2 before spike was added

    // Triplet LTD: pair term + triplet term
    float dw = -learning_rate * (A2_minus + A3_minus * pre_trace_slow) * post_trace_fast;

    // Apply bounded update
    float w = weights[w_idx];
    w = fminf(fmaxf(w + dw, w_min), w_max);
    weights[w_idx] = w;
}

/**
 * @brief Combined triplet STDP update kernel
 *
 * More efficient combined kernel that computes both LTP and LTD
 * in a single pass for all synapses.
 */
__global__ void kernel_triplet_stdp_update(
    float* weights,
    const float* r1, const float* r2,
    const float* o1, const float* o2,
    const float* pre_spikes, const float* post_spikes,
    size_t n_pre, size_t n_post,
    float A2_plus, float A2_minus,
    float A3_plus, float A3_minus,
    float w_min, float w_max,
    float learning_rate)
{
    size_t pre_idx = blockIdx.x;
    size_t post_idx = threadIdx.x + blockIdx.y * blockDim.x;

    if (pre_idx >= n_pre || post_idx >= n_post) return;

    size_t w_idx = pre_idx * n_post + post_idx;
    float w = weights[w_idx];

    float pre_spike = pre_spikes[pre_idx];
    float post_spike = post_spikes[post_idx];

    // Read traces
    float r1_val = r1[pre_idx];
    float r2_val = r2[pre_idx];
    float o1_val = o1[post_idx];
    float o2_val = o2[post_idx];

    float dw = 0.0f;

    // LTP: triggered by post spike, uses pre traces
    // Note: In the Pfister & Gerstner model, o2 should be the value
    // BEFORE the current spike is added. We assume traces were already
    // updated, so we use the current value which includes the spike.
    // For strict adherence, traces should be updated AFTER weight updates.
    if (post_spike > 0.5f) {
        dw += learning_rate * (A2_plus + A3_plus * o2_val) * r1_val;
    }

    // LTD: triggered by pre spike, uses post traces
    if (pre_spike > 0.5f) {
        dw -= learning_rate * (A2_minus + A3_minus * r2_val) * o1_val;
    }

    // Apply bounded update
    w = fminf(fmaxf(w + dw, w_min), w_max);
    weights[w_idx] = w;
}

/**
 * @brief Legacy triplet STDP interface (for backward compatibility)
 */
bool nimcp_gpu_stdp_triplet(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    nimcp_gpu_tensor_t* pre_trace_fast,
    nimcp_gpu_tensor_t* pre_trace_slow,
    nimcp_gpu_tensor_t* post_trace_fast,
    nimcp_gpu_tensor_t* post_trace_slow,
    const nimcp_stdp_params_t* params)
{
    if (!ctx || !weights || !pre_spikes || !post_spikes ||
        !pre_trace_fast || !pre_trace_slow ||
        !post_trace_fast || !post_trace_slow || !params) {
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n_pre = pre_spikes->numel;
    size_t n_post = post_spikes->numel;

    // Use legacy params to derive triplet params (A3 terms set to 0 for pair-based)
    // This provides backward compatibility while using the new implementation
    float A2_plus = params->A_plus;
    float A2_minus = params->A_minus;
    float A3_plus = 0.0f;  // No triplet contribution in legacy mode
    float A3_minus = 0.0f;
    float learning_rate = 1.0f;

    // Calculate grid dimensions
    dim3 grid(n_pre, (n_post + BLOCK_SIZE - 1) / BLOCK_SIZE);
    dim3 block(n_post < BLOCK_SIZE ? n_post : BLOCK_SIZE);

    // Run combined triplet STDP kernel
    kernel_triplet_stdp_update<<<grid, block>>>(
        (float*)weights->data,
        (const float*)pre_trace_fast->data, (const float*)pre_trace_slow->data,
        (const float*)post_trace_fast->data, (const float*)post_trace_slow->data,
        (const float*)pre_spikes->data, (const float*)post_spikes->data,
        n_pre, n_post,
        A2_plus, A2_minus, A3_plus, A3_minus,
        params->w_min, params->w_max, learning_rate);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Full triplet STDP with tensor interface
 */
bool nimcp_gpu_triplet_stdp_full(
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
    float learning_rate)
{
    if (!ctx || !weights || !pre_spikes || !post_spikes ||
        !r1 || !r2 || !o1 || !o2 || !params) {
        LOG_ERROR("Null parameter in triplet STDP full");
        return false;
    }

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    size_t n_pre = pre_spikes->numel;
    size_t n_post = post_spikes->numel;

    // Step 1: Apply weight updates BEFORE updating traces
    // (This ensures o2 and r2 don't include the current spike)
    dim3 grid(n_pre, (n_post + BLOCK_SIZE - 1) / BLOCK_SIZE);
    dim3 block(n_post < BLOCK_SIZE ? n_post : BLOCK_SIZE);

    kernel_triplet_stdp_update<<<grid, block>>>(
        (float*)weights->data,
        (const float*)r1->data, (const float*)r2->data,
        (const float*)o1->data, (const float*)o2->data,
        (const float*)pre_spikes->data, (const float*)post_spikes->data,
        n_pre, n_post,
        params->A2_plus, params->A2_minus,
        params->A3_plus, params->A3_minus,
        params->w_min, params->w_max, learning_rate);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Step 2: Update presynaptic traces
    kernel_update_presynaptic_traces<<<GRID_SIZE(n_pre), BLOCK_SIZE>>>(
        (float*)r1->data, (float*)r2->data,
        (const float*)pre_spikes->data, n_pre,
        params->tau_plus, params->tau_x, dt);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Step 3: Update postsynaptic traces
    kernel_update_postsynaptic_traces<<<GRID_SIZE(n_post), BLOCK_SIZE>>>(
        (float*)o1->data, (float*)o2->data,
        (const float*)post_spikes->data, n_post,
        params->tau_minus, params->tau_y, dt);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    return true;
}

/**
 * @brief Get default triplet STDP parameters
 *
 * Values based on Pfister & Gerstner (2006) visual cortex fits
 */
void nimcp_triplet_stdp_default_params(nimcp_triplet_stdp_params_t* params)
{
    if (!params) return;

    // Pair-based terms
    params->A2_plus = 0.005f;     // Reduced pair LTP
    params->A2_minus = 0.007f;    // Reduced pair LTD
    params->tau_plus = 16.8f;     // ms
    params->tau_minus = 33.7f;    // ms

    // Triplet terms (these are the key additions)
    params->A3_plus = 0.0063f;    // Triplet LTP amplitude
    params->A3_minus = 0.0f;      // Minimal nearest-neighbor model
    params->tau_x = 101.0f;       // ms (slow pre trace)
    params->tau_y = 125.0f;       // ms (slow post trace)

    // Weight bounds
    params->w_min = 0.0f;
    params->w_max = 1.0f;
}

//=============================================================================
// Triplet STDP DAO Implementation
//=============================================================================

// Forward declarations for DAO method implementations
static int dao_update_traces(nimcp_stdp_dao_t* self, const int* pre_spikes,
                             const int* post_spikes, size_t num_pre_spikes,
                             size_t num_post_spikes, float dt);
static int dao_compute_weight_updates(nimcp_stdp_dao_t* self, float* weights,
                                      const int* pre_indices, const int* post_indices,
                                      size_t num_synapses);
static int dao_apply_updates(nimcp_stdp_dao_t* self, float* weights, float learning_rate);
static int dao_reset(nimcp_stdp_dao_t* self);

nimcp_stdp_dao_t* nimcp_triplet_stdp_create(
    void* gpu_ctx,
    size_t num_pre,
    size_t num_post,
    nimcp_triplet_stdp_params_t* params)
{
    if (!gpu_ctx || num_pre == 0 || num_post == 0) {
        LOG_ERROR("Invalid parameters for triplet STDP creation");
        return NULL;
    }

    nimcp_stdp_dao_t* dao = (nimcp_stdp_dao_t*)nimcp_calloc(1, sizeof(nimcp_stdp_dao_t));
    if (!dao) {
        LOG_ERROR("Failed to allocate DAO structure");
        return NULL;
    }

    dao->state = (nimcp_triplet_stdp_state_t*)nimcp_calloc(1, sizeof(nimcp_triplet_stdp_state_t));
    if (!dao->state) {
        LOG_ERROR("Failed to allocate state structure");
        nimcp_free(dao);
        return NULL;
    }

    dao->state->num_pre = num_pre;
    dao->state->num_post = num_post;
    dao->gpu_context = gpu_ctx;

    // Copy or use default params
    if (params) {
        dao->params = *params;
    } else {
        nimcp_triplet_stdp_default_params(&dao->params);
    }

    // Allocate GPU memory for traces
    cudaError_t err;

    err = cudaMalloc(&dao->state->d_r1, num_pre * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&dao->state->d_r2, num_pre * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&dao->state->d_o1, num_post * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&dao->state->d_o2, num_post * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    // Initialize traces to zero
    cudaMemset(dao->state->d_r1, 0, num_pre * sizeof(float));
    cudaMemset(dao->state->d_r2, 0, num_pre * sizeof(float));
    cudaMemset(dao->state->d_o1, 0, num_post * sizeof(float));
    cudaMemset(dao->state->d_o2, 0, num_post * sizeof(float));

    // Set method pointers
    dao->update_traces = dao_update_traces;
    dao->compute_weight_updates = dao_compute_weight_updates;
    dao->apply_updates = dao_apply_updates;
    dao->reset = dao_reset;

    LOG_DEBUG("Created triplet STDP DAO for %zu pre x %zu post", num_pre, num_post);
    return dao;

cleanup:
    LOG_ERROR("CUDA error allocating triplet STDP traces: %s", cudaGetErrorString(err));
    if (dao->state->d_r1) cudaFree(dao->state->d_r1);
    if (dao->state->d_r2) cudaFree(dao->state->d_r2);
    if (dao->state->d_o1) cudaFree(dao->state->d_o1);
    if (dao->state->d_o2) cudaFree(dao->state->d_o2);
    nimcp_free(dao->state);
    nimcp_free(dao);
    return NULL;
}

void nimcp_triplet_stdp_destroy(nimcp_stdp_dao_t* dao)
{
    if (!dao) return;

    if (dao->state) {
        if (dao->state->d_r1) cudaFree(dao->state->d_r1);
        if (dao->state->d_r2) cudaFree(dao->state->d_r2);
        if (dao->state->d_o1) cudaFree(dao->state->d_o1);
        if (dao->state->d_o2) cudaFree(dao->state->d_o2);
        nimcp_free(dao->state);
    }
    nimcp_free(dao);
}

/**
 * @brief Kernel to update traces from sparse spike indices
 */
__global__ void kernel_update_traces_sparse(
    float* trace, const int* spike_indices, size_t num_spikes,
    float decay, float dt)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_spikes) return;

    int neuron_idx = spike_indices[idx];
    // Atomic add to handle potential duplicate indices
    atomicAdd(&trace[neuron_idx], 1.0f);
}

/**
 * @brief DAO method: update traces based on spike indices
 */
static int dao_update_traces(nimcp_stdp_dao_t* self, const int* pre_spikes,
                             const int* post_spikes, size_t num_pre_spikes,
                             size_t num_post_spikes, float dt)
{
    if (!self || !self->state) return -1;

    nimcp_triplet_stdp_state_t* state = self->state;
    nimcp_triplet_stdp_params_t* params = &self->params;

    // Decay all traces first
    float decay_r1 = expf(-dt / params->tau_plus);
    float decay_r2 = expf(-dt / params->tau_x);
    float decay_o1 = expf(-dt / params->tau_minus);
    float decay_o2 = expf(-dt / params->tau_y);

    kernel_decay_traces<<<GRID_SIZE(state->num_pre), BLOCK_SIZE>>>(
        state->d_r1, state->num_pre, decay_r1);
    kernel_decay_traces<<<GRID_SIZE(state->num_pre), BLOCK_SIZE>>>(
        state->d_r2, state->num_pre, decay_r2);
    kernel_decay_traces<<<GRID_SIZE(state->num_post), BLOCK_SIZE>>>(
        state->d_o1, state->num_post, decay_o1);
    kernel_decay_traces<<<GRID_SIZE(state->num_post), BLOCK_SIZE>>>(
        state->d_o2, state->num_post, decay_o2);

    // Update traces for spiking neurons
    if (num_pre_spikes > 0 && pre_spikes) {
        int* d_pre_spikes;
        cudaMalloc(&d_pre_spikes, num_pre_spikes * sizeof(int));
        cudaMemcpy(d_pre_spikes, pre_spikes, num_pre_spikes * sizeof(int),
                   cudaMemcpyHostToDevice);

        kernel_update_traces_sparse<<<GRID_SIZE(num_pre_spikes), BLOCK_SIZE>>>(
            state->d_r1, d_pre_spikes, num_pre_spikes, decay_r1, dt);
        kernel_update_traces_sparse<<<GRID_SIZE(num_pre_spikes), BLOCK_SIZE>>>(
            state->d_r2, d_pre_spikes, num_pre_spikes, decay_r2, dt);

        cudaFree(d_pre_spikes);
    }

    if (num_post_spikes > 0 && post_spikes) {
        int* d_post_spikes;
        cudaMalloc(&d_post_spikes, num_post_spikes * sizeof(int));
        cudaMemcpy(d_post_spikes, post_spikes, num_post_spikes * sizeof(int),
                   cudaMemcpyHostToDevice);

        kernel_update_traces_sparse<<<GRID_SIZE(num_post_spikes), BLOCK_SIZE>>>(
            state->d_o1, d_post_spikes, num_post_spikes, decay_o1, dt);
        kernel_update_traces_sparse<<<GRID_SIZE(num_post_spikes), BLOCK_SIZE>>>(
            state->d_o2, d_post_spikes, num_post_spikes, decay_o2, dt);

        cudaFree(d_post_spikes);
    }

    return 0;
}

/**
 * @brief Kernel for sparse weight updates
 */
__global__ void kernel_triplet_stdp_sparse_update(
    float* weights,
    const float* r1, const float* r2,
    const float* o1, const float* o2,
    const int* pre_indices, const int* post_indices,
    size_t num_synapses, size_t n_post,
    float A2_plus, float A2_minus,
    float A3_plus, float A3_minus,
    float w_min, float w_max,
    float learning_rate)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_synapses) return;

    int pre_idx = pre_indices[idx];
    int post_idx = post_indices[idx];
    size_t w_idx = pre_idx * n_post + post_idx;

    float w = weights[w_idx];

    // Read traces
    float r1_val = r1[pre_idx];
    float r2_val = r2[pre_idx];
    float o1_val = o1[post_idx];
    float o2_val = o2[post_idx];

    // Compute weight change based on traces
    // LTP component (driven by post trace activity interacting with pre traces)
    float ltp = learning_rate * (A2_plus + A3_plus * o2_val) * r1_val * o1_val;

    // LTD component (driven by pre trace activity interacting with post traces)
    float ltd = learning_rate * (A2_minus + A3_minus * r2_val) * r1_val * o1_val;

    float dw = ltp - ltd;

    // Apply bounded update
    w = fminf(fmaxf(w + dw, w_min), w_max);
    weights[w_idx] = w;
}

/**
 * @brief DAO method: compute weight updates (placeholder, updates applied directly)
 */
static int dao_compute_weight_updates(nimcp_stdp_dao_t* self, float* weights,
                                      const int* pre_indices, const int* post_indices,
                                      size_t num_synapses)
{
    // Weight updates are computed during apply_updates
    // This method is a placeholder for potential future optimization
    // where updates could be computed and cached before application
    (void)self;
    (void)weights;
    (void)pre_indices;
    (void)post_indices;
    (void)num_synapses;
    return 0;
}

/**
 * @brief DAO method: apply weight updates (no-op, updates are immediate)
 */
static int dao_apply_updates(nimcp_stdp_dao_t* self, float* weights, float learning_rate)
{
    // Updates are applied immediately in nimcp_triplet_stdp_step
    (void)self;
    (void)weights;
    (void)learning_rate;
    return 0;
}

/**
 * @brief DAO method: reset all traces to zero
 */
static int dao_reset(nimcp_stdp_dao_t* self)
{
    if (!self || !self->state) return -1;

    nimcp_triplet_stdp_state_t* state = self->state;

    cudaMemset(state->d_r1, 0, state->num_pre * sizeof(float));
    cudaMemset(state->d_r2, 0, state->num_pre * sizeof(float));
    cudaMemset(state->d_o1, 0, state->num_post * sizeof(float));
    cudaMemset(state->d_o2, 0, state->num_post * sizeof(float));

    return 0;
}

int nimcp_triplet_stdp_step(
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
    float learning_rate)
{
    if (!dao || !dao->state || !weights) {
        LOG_ERROR("Invalid parameters for triplet STDP step");
        return -1;
    }

    nimcp_triplet_stdp_state_t* state = dao->state;
    nimcp_triplet_stdp_params_t* params = &dao->params;

    // Step 1: Apply weight updates using current traces (before updating them)
    if (num_synapses > 0 && pre_indices && post_indices) {
        int* d_pre_indices;
        int* d_post_indices;

        cudaMalloc(&d_pre_indices, num_synapses * sizeof(int));
        cudaMalloc(&d_post_indices, num_synapses * sizeof(int));
        cudaMemcpy(d_pre_indices, pre_indices, num_synapses * sizeof(int),
                   cudaMemcpyHostToDevice);
        cudaMemcpy(d_post_indices, post_indices, num_synapses * sizeof(int),
                   cudaMemcpyHostToDevice);

        kernel_triplet_stdp_sparse_update<<<GRID_SIZE(num_synapses), BLOCK_SIZE>>>(
            weights,
            state->d_r1, state->d_r2,
            state->d_o1, state->d_o2,
            d_pre_indices, d_post_indices,
            num_synapses, state->num_post,
            params->A2_plus, params->A2_minus,
            params->A3_plus, params->A3_minus,
            params->w_min, params->w_max,
            learning_rate);

        cudaFree(d_pre_indices);
        cudaFree(d_post_indices);
    }

    // Step 2: Update traces with current spikes
    return dao_update_traces(dao, pre_spikes, post_spikes,
                             num_pre_spikes, num_post_spikes, dt);
}

//=============================================================================
// Utility Functions
//=============================================================================

__global__ void kernel_reset_state(float* v, float v_rest, size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        v[idx] = v_rest;
    }
}

bool nimcp_gpu_snn_reset_state(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* v,
    float v_rest)
{
    if (!ctx || !v) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_reset_state<<<GRID_SIZE(v->numel), BLOCK_SIZE>>>(
        (float*)v->data, v_rest, v->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

__device__ inline float warp_reduce_sum(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset >>= 1) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

__global__ void kernel_spike_count(const float* spikes, uint32_t* count, size_t n)
{
    __shared__ uint32_t shared[BLOCK_SIZE / WARP_SIZE];

    uint32_t sum = 0;
    for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += blockDim.x * gridDim.x) {
        if (spikes[i] > 0.0f) sum++;
    }

    // Warp reduction
    sum = (uint32_t)warp_reduce_sum((float)sum);

    int lane = threadIdx.x % WARP_SIZE;
    int warp_id = threadIdx.x / WARP_SIZE;
    if (lane == 0) shared[warp_id] = sum;
    __syncthreads();

    if (warp_id == 0) {
        sum = (lane < blockDim.x / WARP_SIZE) ? shared[lane] : 0;
        sum = (uint32_t)warp_reduce_sum((float)sum);
        if (lane == 0) atomicAdd(count, sum);
    }
}

bool nimcp_gpu_spike_count(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    uint32_t* count)
{
    if (!ctx || !spikes || !count) return false;

    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    uint32_t* d_count;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_count, sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemset(d_count, 0, sizeof(uint32_t)), GPU_ERROR_CUDA_RUNTIME);

    int grid = GRID_SIZE(spikes->numel);
    grid = grid > 256 ? 256 : grid;
    kernel_spike_count<<<grid, BLOCK_SIZE>>>((const float*)spikes->data, d_count, spikes->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaMemcpy(count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    cudaFree(d_count);

    return true;
}

bool nimcp_gpu_spike_rate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    size_t n_timesteps,
    nimcp_gpu_tensor_t* rates)
{
    if (!ctx || !spikes || !rates || n_timesteps == 0) return false;

    // For 1D spikes tensor (single timestep), rate = spikes / n_timesteps
    // For 2D spikes tensor (timesteps x neurons), mean along axis 0
    if (spikes->ndim == 1) {
        // Single timestep: rate = spikes / n_timesteps
        float scale = 1.0f / (float)n_timesteps;
        nimcp_gpu_mul_scalar(ctx, spikes, scale, rates);
    } else {
        // Multiple timesteps: compute mean along time axis
        nimcp_gpu_mean(ctx, spikes, rates, 0, false);
    }

    return true;
}

//=============================================================================
// High-Level SNN Layer API Implementation
//=============================================================================

nimcp_snn_layer_t* nimcp_snn_lif_layer_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_snn_lif_config_t* config)
{
    if (!ctx || !config || config->num_neurons == 0) return NULL;

    nimcp_snn_layer_t* layer = (nimcp_snn_layer_t*)nimcp_calloc(1, sizeof(nimcp_snn_layer_t));
    if (!layer) return NULL;

    layer->model = NIMCP_SNN_LIF;
    layer->num_neurons = config->num_neurons;
    layer->ctx = ctx;
    layer->refractory_period = config->refractory_period;

    // Convert config to LIF params
    nimcp_lif_params_t params;
    params.tau_mem = config->tau_mem;
    params.tau_syn = config->tau_syn;
    params.v_rest = config->v_rest;
    params.v_thresh = config->v_thresh;
    params.v_reset = config->v_reset;
    params.dt = config->dt;
    params.hard_reset = true;

    layer->lif_state = nimcp_lif_state_create(ctx, config->num_neurons, &params);
    if (!layer->lif_state) {
        nimcp_free(layer);
        return NULL;
    }

    // Create refractory timer if needed
    if (config->refractory_period > 0) {
        size_t dims[] = {config->num_neurons};
        layer->refractory_timer = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (layer->refractory_timer) {
            cudaMemset(layer->refractory_timer->data, 0, config->num_neurons * sizeof(float));
        }
    }

    LOG_DEBUG("Created SNN LIF layer with %zu neurons", config->num_neurons);
    return layer;
}

void nimcp_snn_layer_destroy(nimcp_snn_layer_t* layer)
{
    if (!layer) return;

    if (layer->lif_state) {
        nimcp_lif_state_destroy(layer->lif_state);
    }
    if (layer->izh_state) {
        nimcp_izhikevich_state_destroy(layer->izh_state);
    }
    if (layer->adex_state) {
        nimcp_adex_state_destroy(layer->adex_state);
    }
    if (layer->refractory_timer) {
        nimcp_gpu_tensor_destroy(layer->refractory_timer);
    }

    nimcp_free(layer);
}

nimcp_gpu_tensor_t* nimcp_snn_layer_get_membrane(nimcp_snn_layer_t* layer)
{
    if (!layer) return NULL;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
            return layer->lif_state ? layer->lif_state->v : NULL;
        case NIMCP_SNN_IZHIKEVICH:
            return layer->izh_state ? layer->izh_state->v : NULL;
        case NIMCP_SNN_ADEX:
            return layer->adex_state ? layer->adex_state->v : NULL;
        default:
            return NULL;
    }
}

nimcp_gpu_tensor_t* nimcp_snn_layer_get_spikes(nimcp_snn_layer_t* layer)
{
    if (!layer) return NULL;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
            return layer->lif_state ? layer->lif_state->spikes : NULL;
        case NIMCP_SNN_IZHIKEVICH:
            return layer->izh_state ? layer->izh_state->spikes : NULL;
        case NIMCP_SNN_ADEX:
            return layer->adex_state ? layer->adex_state->spikes : NULL;
        default:
            return NULL;
    }
}

size_t nimcp_snn_layer_get_size(const nimcp_snn_layer_t* layer)
{
    if (!layer) return 0;
    return layer->num_neurons;
}

float nimcp_snn_layer_get_tau_mem(const nimcp_snn_layer_t* layer)
{
    if (!layer) return 0.0f;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
            return layer->lif_state ? layer->lif_state->params.tau_mem : 0.0f;
        case NIMCP_SNN_ADEX:
            return layer->adex_state ? layer->adex_state->params.tau_mem : 0.0f;
        default:
            return 0.0f;
    }
}

bool nimcp_snn_layer_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer)
{
    if (!layer) return false;
    (void)ctx;  // Use layer->ctx for consistency

    float v_rest = 0.0f;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
            if (layer->lif_state && layer->lif_state->v) {
                v_rest = layer->lif_state->params.v_rest;
                return nimcp_gpu_snn_reset_state(layer->ctx, layer->lif_state->v, v_rest);
            }
            break;
        case NIMCP_SNN_IZHIKEVICH:
            if (layer->izh_state && layer->izh_state->v) {
                v_rest = -65.0f;  // Standard Izhikevich resting
                return nimcp_gpu_snn_reset_state(layer->ctx, layer->izh_state->v, v_rest);
            }
            break;
        case NIMCP_SNN_ADEX:
            if (layer->adex_state && layer->adex_state->v) {
                v_rest = layer->adex_state->params.v_rest;
                return nimcp_gpu_snn_reset_state(layer->ctx, layer->adex_state->v, v_rest);
            }
            break;
        default:
            break;
    }

    return false;
}

bool nimcp_snn_layer_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!layer || !input) return false;
    (void)ctx;  // Use layer->ctx for consistency

    switch (layer->model) {
        case NIMCP_SNN_LIF:
            return nimcp_gpu_lif_forward(layer->ctx, layer->lif_state, input);
        case NIMCP_SNN_IZHIKEVICH:
            return nimcp_gpu_izhikevich_forward(layer->ctx, layer->izh_state, input);
        case NIMCP_SNN_ADEX:
            return nimcp_gpu_adex_forward(layer->ctx, layer->adex_state, input);
        default:
            return false;
    }
}

nimcp_gpu_tensor_t* nimcp_snn_lif_step(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!layer || !input) return NULL;

    // Run forward pass
    if (!nimcp_snn_layer_forward(ctx, layer, input)) {
        return NULL;
    }

    // Return spikes tensor
    return nimcp_snn_layer_get_spikes(layer);
}

nimcp_gpu_tensor_t* nimcp_snn_spike_tensor_create(
    nimcp_gpu_context_t* ctx,
    const uint8_t* data,
    const size_t* dims,
    size_t ndim)
{
    if (!ctx || !data || !dims || ndim == 0) return NULL;

    // Calculate total size
    size_t total = 1;
    for (size_t i = 0; i < ndim; i++) {
        total *= dims[i];
    }

    // Create float tensor (spikes as 0.0 or 1.0)
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32);
    if (!tensor) return NULL;

    // Convert uint8 spikes to float and upload
    float* host_data = (float*)nimcp_malloc(total * sizeof(float));
    if (!host_data) {
        nimcp_gpu_tensor_destroy(tensor);
        return NULL;
    }

    for (size_t i = 0; i < total; i++) {
        host_data[i] = data[i] ? 1.0f : 0.0f;
    }

    cudaError_t err = cudaMemcpy(tensor->data, host_data, total * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_free(host_data);

    if (err != cudaSuccess) {
        nimcp_gpu_tensor_destroy(tensor);
        return NULL;
    }

    return tensor;
}

bool nimcp_snn_spike_tensor_to_host(
    const nimcp_gpu_tensor_t* tensor,
    uint8_t* data)
{
    if (!tensor || !data) return false;

    // Download float data
    float* host_data = (float*)nimcp_malloc(tensor->numel * sizeof(float));
    if (!host_data) return false;

    cudaError_t err = cudaMemcpy(host_data, tensor->data, tensor->numel * sizeof(float), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        nimcp_free(host_data);
        return false;
    }

    // Convert float to uint8
    for (size_t i = 0; i < tensor->numel; i++) {
        data[i] = host_data[i] > 0.5f ? 1 : 0;
    }

    nimcp_free(host_data);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/snn/nimcp_snn_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>

#define LOG_MODULE "SNN_GPU"

nimcp_lif_state_t* nimcp_lif_state_create(nimcp_gpu_context_t* ctx, size_t n_neurons,
    const nimcp_lif_params_t* params)
{
    LOG_WARN("CUDA not available - SNN requires GPU");
    return NULL;
}

void nimcp_lif_state_destroy(nimcp_lif_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_lif_forward(nimcp_gpu_context_t* ctx, nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    return false;
}

bool nimcp_gpu_lif_backward(nimcp_gpu_context_t* ctx, const nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input,
    nimcp_surrogate_type_t surrogate_type, float beta)
{
    return false;
}

nimcp_izhikevich_state_t* nimcp_izhikevich_state_create(nimcp_gpu_context_t* ctx,
    size_t n_neurons, const nimcp_izhikevich_params_t* params)
{
    return NULL;
}

void nimcp_izhikevich_state_destroy(nimcp_izhikevich_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_izhikevich_forward(nimcp_gpu_context_t* ctx, nimcp_izhikevich_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    return false;
}

nimcp_adex_state_t* nimcp_adex_state_create(nimcp_gpu_context_t* ctx, size_t n_neurons,
    const nimcp_adex_params_t* params)
{
    return NULL;
}

void nimcp_adex_state_destroy(nimcp_adex_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_adex_forward(nimcp_gpu_context_t* ctx, nimcp_adex_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    return false;
}

bool nimcp_gpu_surrogate_gradient(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* v,
    float v_thresh, nimcp_gpu_tensor_t* grad, nimcp_surrogate_type_t type, float beta)
{
    return false;
}

bool nimcp_gpu_spike_propagate(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* spikes,
    const nimcp_gpu_tensor_t* weights, nimcp_gpu_tensor_t* output)
{
    return false;
}

bool nimcp_gpu_spike_propagate_sparse(nimcp_gpu_context_t* ctx, const uint32_t* indices,
    size_t n_spikes, const nimcp_gpu_tensor_t* weights, nimcp_gpu_tensor_t* output)
{
    return false;
}

bool nimcp_gpu_eligibility_trace_update(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* trace,
    const nimcp_gpu_tensor_t* spikes, float decay)
{
    return false;
}

bool nimcp_gpu_stdp_pair(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace, const nimcp_gpu_tensor_t* post_trace,
    const nimcp_stdp_params_t* params)
{
    return false;
}

bool nimcp_gpu_stdp_triplet(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    nimcp_gpu_tensor_t* pre_trace_fast, nimcp_gpu_tensor_t* pre_trace_slow,
    nimcp_gpu_tensor_t* post_trace_fast, nimcp_gpu_tensor_t* post_trace_slow,
    const nimcp_stdp_params_t* params)
{
    return false;
}

bool nimcp_gpu_triplet_stdp_full(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    nimcp_gpu_tensor_t* r1, nimcp_gpu_tensor_t* r2,
    nimcp_gpu_tensor_t* o1, nimcp_gpu_tensor_t* o2,
    const nimcp_triplet_stdp_params_t* params, float dt, float learning_rate)
{
    (void)ctx; (void)weights; (void)pre_spikes; (void)post_spikes;
    (void)r1; (void)r2; (void)o1; (void)o2;
    (void)params; (void)dt; (void)learning_rate;
    return false;
}

nimcp_stdp_dao_t* nimcp_triplet_stdp_create(void* gpu_ctx, size_t num_pre,
    size_t num_post, nimcp_triplet_stdp_params_t* params)
{
    LOG_WARN("CUDA not available - triplet STDP requires GPU");
    (void)gpu_ctx; (void)num_pre; (void)num_post; (void)params;
    return NULL;
}

void nimcp_triplet_stdp_destroy(nimcp_stdp_dao_t* dao)
{
    if (dao) {
        if (dao->state) nimcp_free(dao->state);
        nimcp_free(dao);
    }
}

int nimcp_triplet_stdp_step(nimcp_stdp_dao_t* dao, const int* pre_spikes,
    const int* post_spikes, size_t num_pre_spikes, size_t num_post_spikes,
    float* weights, const int* pre_indices, const int* post_indices,
    size_t num_synapses, float dt, float learning_rate)
{
    (void)dao; (void)pre_spikes; (void)post_spikes;
    (void)num_pre_spikes; (void)num_post_spikes;
    (void)weights; (void)pre_indices; (void)post_indices;
    (void)num_synapses; (void)dt; (void)learning_rate;
    return -1;
}

void nimcp_triplet_stdp_default_params(nimcp_triplet_stdp_params_t* params)
{
    if (!params) return;

    // Same defaults as CUDA version
    params->A2_plus = 0.005f;
    params->A2_minus = 0.007f;
    params->tau_plus = 16.8f;
    params->tau_minus = 33.7f;
    params->A3_plus = 0.0063f;
    params->A3_minus = 0.0f;
    params->tau_x = 101.0f;
    params->tau_y = 125.0f;
    params->w_min = 0.0f;
    params->w_max = 1.0f;
}

bool nimcp_gpu_snn_reset_state(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* v, float v_rest)
{
    return false;
}

bool nimcp_gpu_spike_count(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* spikes,
    uint32_t* count)
{
    return false;
}

bool nimcp_gpu_spike_rate(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* spikes,
    size_t n_timesteps, nimcp_gpu_tensor_t* rates)
{
    return false;
}

//=============================================================================
// SNN Layer Wrapper API - CPU Fallback Stubs
//=============================================================================

nimcp_snn_layer_t* nimcp_snn_lif_layer_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_snn_lif_config_t* config)
{
    LOG_WARN("CUDA not available - SNN layer requires GPU");
    (void)ctx;
    (void)config;
    return NULL;
}

void nimcp_snn_layer_destroy(nimcp_snn_layer_t* layer)
{
    if (layer) nimcp_free(layer);
}

nimcp_gpu_tensor_t* nimcp_snn_layer_get_membrane(nimcp_snn_layer_t* layer)
{
    (void)layer;
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_snn_layer_get_spikes(nimcp_snn_layer_t* layer)
{
    (void)layer;
    return NULL;
}

size_t nimcp_snn_layer_get_size(const nimcp_snn_layer_t* layer)
{
    (void)layer;
    return 0;
}

float nimcp_snn_layer_get_tau_mem(const nimcp_snn_layer_t* layer)
{
    (void)layer;
    return 0.0f;
}

bool nimcp_snn_layer_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer)
{
    (void)ctx;
    (void)layer;
    return false;
}

bool nimcp_snn_layer_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;
    (void)layer;
    (void)input;
    return false;
}

nimcp_gpu_tensor_t* nimcp_snn_lif_step(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;
    (void)layer;
    (void)input;
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_snn_spike_tensor_create(
    nimcp_gpu_context_t* ctx,
    const uint8_t* data,
    const size_t* dims,
    size_t ndim)
{
    (void)ctx;
    (void)data;
    (void)dims;
    (void)ndim;
    return NULL;
}

bool nimcp_snn_spike_tensor_to_host(
    const nimcp_gpu_tensor_t* tensor,
    uint8_t* data)
{
    (void)tensor;
    (void)data;
    return false;
}

#endif // NIMCP_ENABLE_CUDA
