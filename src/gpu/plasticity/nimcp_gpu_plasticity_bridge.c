/**
 * @file nimcp_gpu_plasticity_bridge.c
 * @brief GPU Plasticity Bridge Implementation
 *
 * WHAT: Bridges CPU training code to GPU plasticity CUDA kernels
 * WHY:  Offload STDP/BCM/homeostatic to GPU for 2M neuron brains
 * HOW:  Upload neuron data to GPU tensors, run kernels, download results
 *
 * DESIGN NOTES:
 * - Persistent gpu_plasticity_state_t avoids per-call GPU tensor allocation
 * - All functions return gracefully on failure (caller falls back to CPU)
 * - GPU tensors sized to max capacity; actual counts passed per call
 * - SNN populations reference neurons via neuron_ids into neural_network_t
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026-03-29
 */

#include "gpu/plasticity/nimcp_gpu_plasticity_bridge.h"
#include "gpu/plasticity/nimcp_plasticity_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_types.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Persistent GPU Plasticity State
 * ============================================================================ */

struct gpu_plasticity_state_s {
    nimcp_gpu_context_t* gpu_ctx;

    /* STDP tensors (sized for max_neurons) */
    nimcp_gpu_tensor_t* pre_traces;      /* Pre-synaptic eligibility traces */
    nimcp_gpu_tensor_t* post_traces;     /* Post-synaptic eligibility traces */
    nimcp_gpu_tensor_t* pre_spikes;      /* Pre-synaptic spike vector */
    nimcp_gpu_tensor_t* post_spikes;     /* Post-synaptic spike vector */

    /* BCM tensors */
    nimcp_gpu_tensor_t* bcm_thresholds;  /* Sliding thresholds */
    nimcp_gpu_tensor_t* pre_activity;    /* Pre-synaptic activity */
    nimcp_gpu_tensor_t* post_activity;   /* Post-synaptic activity */

    /* Homeostatic tensors (sized for max_neurons) */
    nimcp_gpu_tensor_t* avg_rates;       /* Running average firing rates */
    nimcp_gpu_tensor_t* scaling_factors; /* Synaptic scaling factors */
    nimcp_gpu_tensor_t* spike_input;     /* Spike input for rate update */

    /* Default parameters */
    nimcp_gpu_stdp_params_t stdp_params;
    nimcp_gpu_bcm_params_t bcm_params;
    nimcp_gpu_scaling_params_t scaling_params;
    nimcp_gpu_intrinsic_params_t intrinsic_params;

    uint32_t max_neurons;
    uint32_t max_synapses;
    bool initialized;
};

gpu_plasticity_state_t* gpu_plasticity_state_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t max_neurons,
    uint32_t max_synapses)
{
    if (!gpu_ctx || max_neurons == 0) {
        return NULL;
    }

    gpu_plasticity_state_t* state = nimcp_calloc(1, sizeof(gpu_plasticity_state_t));
    if (!state) return NULL;

    state->gpu_ctx = gpu_ctx;
    state->max_neurons = max_neurons;
    state->max_synapses = max_synapses > 0 ? max_synapses : max_neurons * 320;

    /* Initialize default parameters */
    state->stdp_params = nimcp_gpu_stdp_params_default();
    state->bcm_params = nimcp_gpu_bcm_params_default();
    state->scaling_params = nimcp_gpu_scaling_params_default();
    state->intrinsic_params = nimcp_gpu_intrinsic_params_default();

    /* Allocate GPU tensors sized for neuron populations.
     * Weight matrices are allocated per-call for actual population pairs. */
    size_t n_dim = (size_t)max_neurons;
    state->pre_traces = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                 NIMCP_GPU_PRECISION_FP32);
    state->post_traces = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                  NIMCP_GPU_PRECISION_FP32);
    state->pre_spikes = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                 NIMCP_GPU_PRECISION_FP32);
    state->post_spikes = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                  NIMCP_GPU_PRECISION_FP32);

    /* BCM tensors */
    state->bcm_thresholds = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                     NIMCP_GPU_PRECISION_FP32);
    state->pre_activity = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                   NIMCP_GPU_PRECISION_FP32);
    state->post_activity = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                    NIMCP_GPU_PRECISION_FP32);

    /* Homeostatic tensors */
    state->avg_rates = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                NIMCP_GPU_PRECISION_FP32);
    state->scaling_factors = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                      NIMCP_GPU_PRECISION_FP32);
    state->spike_input = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                                  NIMCP_GPU_PRECISION_FP32);

    /* Verify all critical tensors allocated */
    if (!state->pre_traces || !state->post_traces ||
        !state->pre_spikes || !state->post_spikes ||
        !state->avg_rates || !state->scaling_factors) {
        NIMCP_LOGGING_WARN("GPU plasticity state: some tensors failed to allocate, "
                          "falling back to CPU for those paths");
    }

    state->initialized = true;
    NIMCP_LOGGING_INFO("GPU plasticity state created: max_neurons=%u, max_synapses=%u",
                      max_neurons, max_synapses);
    return state;
}

void gpu_plasticity_state_destroy(gpu_plasticity_state_t* state)
{
    if (!state) return;

    nimcp_gpu_tensor_destroy(state->pre_traces);
    nimcp_gpu_tensor_destroy(state->post_traces);
    nimcp_gpu_tensor_destroy(state->pre_spikes);
    nimcp_gpu_tensor_destroy(state->post_spikes);
    nimcp_gpu_tensor_destroy(state->bcm_thresholds);
    nimcp_gpu_tensor_destroy(state->pre_activity);
    nimcp_gpu_tensor_destroy(state->post_activity);
    nimcp_gpu_tensor_destroy(state->avg_rates);
    nimcp_gpu_tensor_destroy(state->scaling_factors);
    nimcp_gpu_tensor_destroy(state->spike_input);

    nimcp_free(state);
}

/* ============================================================================
 * GPU STDP Apply
 *
 * WHAT: GPU-accelerated STDP for SNN network
 * WHY:  CPU STDP iterates O(n_neurons * synapses_per_neuron) serially
 * HOW:  Extract population spike data → GPU trace update + weight update →
 *       write back via sparse synapse handles
 *
 * For each adjacent population pair (pre→post), we:
 * 1. Extract binary spike vectors from neuron last_spike timestamps
 * 2. Build trace vectors from neuron STDP traces
 * 3. Build dense weight sub-matrix for the population pair
 * 4. Run GPU STDP kernels (trace update + weight update)
 * 5. Download and write back updated weights
 * ============================================================================ */

uint32_t gpu_plasticity_stdp_apply(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    snn_network_t* snn,
    snn_training_ctx_t* ctx)
{
    if (!gpu_ctx || !snn || !ctx) return 0;
    if (!snn->neural_net) return 0;
    if (snn->n_populations < 2) return 0;

    neural_network_t net = snn->neural_net;
    uint32_t total_updates = 0;

    /* Get STDP parameters (use defaults matching snn_stdp_compute_delta_w) */
    nimcp_gpu_stdp_params_t gpu_params;
    if (state) {
        gpu_params = state->stdp_params;
    } else {
        gpu_params = nimcp_gpu_stdp_params_default();
    }

    /* Process each adjacent population pair */
    for (uint32_t pop_idx = 0; pop_idx + 1 < snn->n_populations; pop_idx++) {
        snn_population_t* pre_pop = snn->populations[pop_idx];
        snn_population_t* post_pop = snn->populations[pop_idx + 1];

        if (!pre_pop || !post_pop) continue;
        if (pre_pop->n_neurons == 0 || post_pop->n_neurons == 0) continue;

        uint32_t n_pre = pre_pop->n_neurons;
        uint32_t n_post = post_pop->n_neurons;

        /* Skip pop pairs too large to densify. This path builds a dense
         * [n_post x n_pre] weight matrix (see below) — for the 64K-neuron
         * language ensembles (broca/wernicke) that is 64000x64000x4 = 16.4 GB,
         * which OOMs the GPU every step and thrashes the whole device. Those
         * pops are sparse 8-neuron ensembles (SNN_LANG_NEURONS_PER_POP) trained
         * by the concept->word projection-EMA path (snn_language_bridge_learn_pair),
         * not by generic adjacent-pair Hebbian STDP, so skipping them here loses
         * nothing. Cap at 16M elements (64 MB dense matrix); real STDP pops are
         * a few thousand neurons and stay far under. */
        if ((size_t)n_pre * (size_t)n_post > (size_t)16u * 1024u * 1024u) {
            continue;
        }

        /* Allocate host buffers */
        float* h_pre_spikes = nimcp_calloc(n_pre, sizeof(float));
        float* h_post_spikes = nimcp_calloc(n_post, sizeof(float));
        float* h_pre_traces = nimcp_calloc(n_pre, sizeof(float));
        float* h_post_traces = nimcp_calloc(n_post, sizeof(float));

        if (!h_pre_spikes || !h_post_spikes || !h_pre_traces || !h_post_traces) {
            nimcp_free(h_pre_spikes);
            nimcp_free(h_post_spikes);
            nimcp_free(h_pre_traces);
            nimcp_free(h_post_traces);
            continue;
        }

        /* Extract spike states from neurons referenced by population neuron_ids.
         * A neuron "spiked" if its last_spike timestamp is recent (within 1ms).
         * Traces are computed from spike timing: trace = exp(-dt/tau) where
         * dt is the time since last spike. This matches the trace-based STDP
         * formulation used by the GPU kernels. */
        uint64_t sim_time = snn->sim ? snn->sim->current_time_us : 0;
        float tau_pre = gpu_params.tau_plus > 0.0f ? gpu_params.tau_plus : 20.0f;
        float tau_post = gpu_params.tau_minus > 0.0f ? gpu_params.tau_minus : 20.0f;

        for (uint32_t i = 0; i < n_pre; i++) {
            neuron_t* neuron = neural_network_get_neuron(net, pre_pop->neuron_ids[i]);
            if (!neuron) continue;
            /* Neuron fired recently if last_spike within 1ms of current time */
            bool fired = (neuron->last_spike > 0 &&
                         (sim_time - neuron->last_spike) < 1000);
            h_pre_spikes[i] = fired ? 1.0f : 0.0f;
            /* Compute trace from spike timing: trace decays exponentially */
            if (neuron->last_spike > 0 && sim_time > neuron->last_spike) {
                float dt_ms = (float)(sim_time - neuron->last_spike) / 1000.0f;
                h_pre_traces[i] = expf(-dt_ms / tau_pre);
            }
        }
        for (uint32_t i = 0; i < n_post; i++) {
            neuron_t* neuron = neural_network_get_neuron(net, post_pop->neuron_ids[i]);
            if (!neuron) continue;
            bool fired = (neuron->last_spike > 0 &&
                         (sim_time - neuron->last_spike) < 1000);
            h_post_spikes[i] = fired ? 1.0f : 0.0f;
            if (neuron->last_spike > 0 && sim_time > neuron->last_spike) {
                float dt_ms = (float)(sim_time - neuron->last_spike) / 1000.0f;
                h_post_traces[i] = expf(-dt_ms / tau_post);
            }
        }

        /* Build dense weight matrix [n_post x n_pre] from sparse synapse storage.
         * For each pre neuron, scan outgoing synapses targeting post neurons. */
        float* h_weights = nimcp_calloc((size_t)n_post * n_pre, sizeof(float));
        if (!h_weights) {
            nimcp_free(h_pre_spikes);
            nimcp_free(h_post_spikes);
            nimcp_free(h_pre_traces);
            nimcp_free(h_post_traces);
            continue;
        }

        /* Build a lookup: post_neuron_id → index in post_pop */
        uint32_t max_neuron_id = neural_network_get_num_neurons(net);
        uint32_t* post_id_to_idx = nimcp_calloc(max_neuron_id, sizeof(uint32_t));
        if (!post_id_to_idx) {
            nimcp_free(h_weights);
            nimcp_free(h_pre_spikes);
            nimcp_free(h_post_spikes);
            nimcp_free(h_pre_traces);
            nimcp_free(h_post_traces);
            continue;
        }
        /* Initialize to sentinel (UINT32_MAX = not in post population) */
        memset(post_id_to_idx, 0xFF, max_neuron_id * sizeof(uint32_t));
        for (uint32_t i = 0; i < n_post; i++) {
            if (post_pop->neuron_ids[i] < max_neuron_id) {
                post_id_to_idx[post_pop->neuron_ids[i]] = i;
            }
        }

        /* Extract weights: for each pre neuron, scan outgoing synapses */
        for (uint32_t pre_i = 0; pre_i < n_pre; pre_i++) {
            neuron_t* pre_neuron = neural_network_get_neuron(net, pre_pop->neuron_ids[pre_i]);
            if (!pre_neuron) continue;
            uint32_t out_count = NEURON_OUT_COUNT(pre_neuron);
            for (uint32_t s = 0; s < out_count; s++) {
                synapse_handle_t* handle = NEURON_OUT_HANDLE(pre_neuron, s);
                if (!handle) continue;
                uint32_t target_id = handle->target_neuron_id;
                if (target_id >= max_neuron_id) continue;
                uint32_t post_i = post_id_to_idx[target_id];
                if (post_i == UINT32_MAX) continue; /* Not in post population */
                h_weights[post_i * n_pre + pre_i] = handle->weight;
            }
        }

        nimcp_free(post_id_to_idx);

        /* Upload to GPU tensors */
        size_t pre_dim = (size_t)n_pre;
        size_t post_dim = (size_t)n_post;
        size_t weight_dims[2] = { post_dim, pre_dim };

        nimcp_gpu_tensor_t* g_pre_spikes = nimcp_gpu_tensor_from_host(
            gpu_ctx, h_pre_spikes, &pre_dim, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* g_post_spikes = nimcp_gpu_tensor_from_host(
            gpu_ctx, h_post_spikes, &post_dim, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* g_pre_traces = nimcp_gpu_tensor_from_host(
            gpu_ctx, h_pre_traces, &pre_dim, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* g_post_traces = nimcp_gpu_tensor_from_host(
            gpu_ctx, h_post_traces, &post_dim, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* g_weights = nimcp_gpu_tensor_from_host(
            gpu_ctx, h_weights, weight_dims, 2, NIMCP_GPU_PRECISION_FP32);

        if (!g_pre_spikes || !g_post_spikes || !g_pre_traces ||
            !g_post_traces || !g_weights) {
            NIMCP_LOGGING_WARN("GPU STDP: tensor upload failed for pop pair %u→%u",
                              pop_idx, pop_idx + 1);
            goto next_pair;
        }

        /* Step 1: Update eligibility traces on GPU */
        float dt = snn->sim ? snn->sim->dt_ms : 1.0f;
        bool trace_ok = nimcp_gpu_stdp_update_traces(
            gpu_ctx, g_pre_traces, g_post_traces,
            g_pre_spikes, g_post_spikes, dt, &gpu_params);

        /* Step 2: Apply STDP weight update on GPU */
        bool stdp_ok = false;
        if (trace_ok) {
            stdp_ok = nimcp_gpu_stdp_apply(
                gpu_ctx, g_weights,
                g_pre_spikes, g_post_spikes,
                g_pre_traces, g_post_traces,
                &gpu_params);
        }

        if (stdp_ok) {
            /* Download updated weights */
            nimcp_gpu_tensor_to_host(g_weights, h_weights);

            /* Write weights back to sparse synapse storage */
            uint32_t* post_id_to_idx2 = nimcp_calloc(max_neuron_id, sizeof(uint32_t));
            if (post_id_to_idx2) {
                memset(post_id_to_idx2, 0xFF, max_neuron_id * sizeof(uint32_t));
                for (uint32_t i = 0; i < n_post; i++) {
                    if (post_pop->neuron_ids[i] < max_neuron_id) {
                        post_id_to_idx2[post_pop->neuron_ids[i]] = i;
                    }
                }

                for (uint32_t pre_i = 0; pre_i < n_pre; pre_i++) {
                    neuron_t* pre_neuron = neural_network_get_neuron(
                        net, pre_pop->neuron_ids[pre_i]);
                    if (!pre_neuron) continue;
                    uint32_t out_count = NEURON_OUT_COUNT(pre_neuron);
                    for (uint32_t s = 0; s < out_count; s++) {
                        synapse_handle_t* handle = NEURON_OUT_HANDLE(pre_neuron, s);
                        if (!handle) continue;
                        uint32_t target_id = handle->target_neuron_id;
                        if (target_id >= max_neuron_id) continue;
                        uint32_t post_i = post_id_to_idx2[target_id];
                        if (post_i == UINT32_MAX) continue;
                        float new_w = h_weights[post_i * n_pre + pre_i];
                        if (fabsf(new_w - handle->weight) > 1e-7f) {
                            handle->weight = new_w;
                            total_updates++;
                        }
                    }
                }
                nimcp_free(post_id_to_idx2);
            }

            /* Note: Traces are ephemeral (computed from spike timing each call).
             * No trace writeback needed — neuron->last_spike is the ground truth. */
        }

    next_pair:
        nimcp_gpu_tensor_destroy(g_pre_spikes);
        nimcp_gpu_tensor_destroy(g_post_spikes);
        nimcp_gpu_tensor_destroy(g_pre_traces);
        nimcp_gpu_tensor_destroy(g_post_traces);
        nimcp_gpu_tensor_destroy(g_weights);
        nimcp_free(h_pre_spikes);
        nimcp_free(h_post_spikes);
        nimcp_free(h_pre_traces);
        nimcp_free(h_post_traces);
        nimcp_free(h_weights);
    }

    if (total_updates > 0) {
        NIMCP_LOGGING_DEBUG("GPU STDP: %u weight updates across %u population pairs",
                          total_updates, snn->n_populations - 1);
    }

    return total_updates;
}

/* ============================================================================
 * GPU Homeostatic Plasticity Update
 *
 * WHAT: GPU-accelerated homeostatic rate tracking and scaling
 * WHY:  O(n_neurons) rate update + scaling computation parallelizes well
 * HOW:  Upload firing rates → GPU rate update + scaling → download
 * ============================================================================ */

int gpu_plasticity_homeostatic_update(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    float* firing_rates,
    uint32_t n_neurons,
    float target_rate,
    float dt)
{
    if (!gpu_ctx || !firing_rates || n_neurons == 0) return -1;

    /* Use persistent state scaling params if available */
    nimcp_gpu_scaling_params_t params;
    if (state) {
        params = state->scaling_params;
    } else {
        params = nimcp_gpu_scaling_params_default();
    }
    params.target_rate = target_rate;

    /* Upload firing rates to GPU.
     * BUG FIX: Previously this used a transient g_rates tensor; the running
     * average update + scaling landed there and the host buffer was freed on
     * return — leaving state->avg_rates uninitialized so the downstream
     * gpu_plasticity_homeostatic_scale_weights() consumer (which reads
     * state->avg_rates) saw garbage. Use the persistent state tensor when
     * available so the running average and scaled rates survive the call. */
    size_t n_dim = (size_t)n_neurons;
    nimcp_gpu_tensor_t* g_rates = NULL;
    bool own_rates = false;
    if (state && state->avg_rates && n_neurons <= state->max_neurons) {
        g_rates = state->avg_rates;
        if (!nimcp_gpu_tensor_upload(g_rates, firing_rates)) {
            return -1;
        }
    } else {
        g_rates = nimcp_gpu_tensor_from_host(
            gpu_ctx, firing_rates, &n_dim, 1, NIMCP_GPU_PRECISION_FP32);
        own_rates = true;
    }
    if (!g_rates) return -1;

    /* Create or use persistent scaling factors tensor */
    nimcp_gpu_tensor_t* g_scaling = NULL;
    bool own_scaling = false;
    if (state && state->scaling_factors && n_neurons <= state->max_neurons) {
        g_scaling = state->scaling_factors;
    } else {
        g_scaling = nimcp_gpu_tensor_create(gpu_ctx, &n_dim, 1,
                                             NIMCP_GPU_PRECISION_FP32);
        own_scaling = true;
    }

    if (!g_scaling) {
        if (own_rates) nimcp_gpu_tensor_destroy(g_rates);
        return -1;
    }

    /* Step 1: Update rate estimates on GPU
     * rate = rate + (spike - rate) * (1 - exp(-dt/tau)) */
    nimcp_gpu_tensor_t* g_spikes = nimcp_gpu_tensor_from_host(
        gpu_ctx, firing_rates, &n_dim, 1, NIMCP_GPU_PRECISION_FP32);
    if (g_spikes) {
        nimcp_gpu_homeostatic_update_rates(gpu_ctx, g_rates, g_spikes,
                                            dt, &params);
        nimcp_gpu_tensor_destroy(g_spikes);
    }

    /* Step 2: Compute scaling factors
     * factor = (target_rate / actual_rate)^alpha */
    bool scale_ok = nimcp_gpu_homeostatic_compute_scaling(
        gpu_ctx, g_scaling, g_rates, &params);

    /* Step 3: Apply scaling factors to rates
     * rate_new = clamp(rate * scaling_factor, 0, max_rate)
     * Without this step, scaling factors were computed but never used. */
    if (scale_ok) {
        nimcp_gpu_homeostatic_apply_scaling(
            gpu_ctx, g_rates, g_scaling,
            0.0f,   /* w_min: rates cannot go negative */
            1000.0f  /* w_max: reasonable upper bound for firing rate (Hz) */
        );
    }

    int result = -1;
    if (scale_ok) {
        /* Download updated rates back to caller */
        nimcp_gpu_tensor_to_host(g_rates, firing_rates);
        result = 0;

        NIMCP_LOGGING_DEBUG("GPU homeostatic update: %u neurons, target_rate=%.1f Hz",
                          n_neurons, target_rate);
    }

    if (own_rates) nimcp_gpu_tensor_destroy(g_rates);
    if (own_scaling) {
        nimcp_gpu_tensor_destroy(g_scaling);
    }

    return result;
}

/* ============================================================================
 * GPU Plasticity Coordinator Update
 *
 * WHAT: GPU-accelerated batch update for STDP traces + BCM + homeostatic
 * WHY:  Reduce PCIe round-trips by batching all plasticity math on GPU
 * HOW:  Run trace decay + BCM threshold + rate tracking in one GPU pass
 *
 * The coordinator's registered mechanism callbacks still run on CPU,
 * but the heavy numerical computation is GPU-accelerated.
 * ============================================================================ */

int gpu_plasticity_coordinator_update(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    void* coordinator,
    uint64_t current_time_ms,
    float dt)
{
    if (!gpu_ctx || !state || !state->initialized || dt <= 0.0f) return -1;

    int updates = 0;

    /* GPU batch 1: STDP trace decay (exponential decay of all traces)
     * trace *= exp(-dt/tau). No spikes during coordinator tick — just decay. */
    if (state->pre_traces && state->post_traces) {
        size_t zero_dim = state->max_neurons;
        nimcp_gpu_tensor_t* zero_spikes = nimcp_gpu_tensor_create(
            gpu_ctx, &zero_dim, 1, NIMCP_GPU_PRECISION_FP32);
        if (zero_spikes) {
            nimcp_gpu_stdp_update_traces(
                gpu_ctx, state->pre_traces, state->post_traces,
                zero_spikes, zero_spikes, dt, &state->stdp_params);
            nimcp_gpu_tensor_destroy(zero_spikes);
            updates++;
        }
    }

    /* GPU batch 2: BCM threshold adaptation
     * theta = theta + (post^p - theta) * dt/tau */
    if (state->bcm_thresholds && state->post_activity) {
        nimcp_gpu_bcm_update_threshold(
            gpu_ctx, state->bcm_thresholds, state->post_activity,
            dt, &state->bcm_params);
        updates++;
    }

    /* GPU batch 3: Homeostatic rate tracking
     * rate = rate + (spike - rate) * (1 - exp(-dt/tau)) */
    if (state->avg_rates && state->spike_input) {
        nimcp_gpu_homeostatic_update_rates(
            gpu_ctx, state->avg_rates, state->spike_input,
            dt, &state->scaling_params);
        updates++;
    }

    (void)coordinator;       /* Coordinator handle reserved for future use */
    (void)current_time_ms;   /* Timing info reserved for interval gating */

    return updates;
}

/* ============================================================================
 * GPU Plasticity Weight Updates on CSR Sparse Weights
 *
 * These functions apply plasticity weight modifications directly to the
 * GPU weight cache's CSR sparse tensors, avoiding GPU→CPU→GPU round-trips.
 * Call AFTER trace/threshold/rate updates, BEFORE next forward pass.
 * ============================================================================ */

#include "gpu/sparse/nimcp_sparse_gpu.h"

/**
 * Apply STDP weight updates to CSR sparse weights on GPU.
 *
 * For each non-zero weight w[i] at (row, col):
 *   pre_t = pre_traces[col], post_t = post_traces[row]
 *   if pre_t > threshold: w[i] += a_plus * lr * pre_t    (LTP)
 *   if post_t > threshold: w[i] -= a_minus * lr * post_t  (LTD)
 *   w[i] = clamp(w[i], w_min, w_max)
 */
int gpu_plasticity_stdp_update_weights(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    nimcp_sparse_tensor_t* sparse_weights,  /* CSR on GPU */
    float learning_rate)
{
    if (!gpu_ctx || !state || !sparse_weights) return -1;
    if (sparse_weights->format != SPARSE_FORMAT_CSR) return -1;
    if (!state->pre_traces || !state->post_traces) return -1;

    int nnz = sparse_weights->data.csr.nnz;
    if (nnz == 0) return 0;

    /* Call CUDA kernel: nimcp_gpu_stdp_weight_update defined in plasticity_gpu.h */
    extern int nimcp_gpu_stdp_weight_update_csr(
        nimcp_gpu_context_t* ctx,
        float* csr_values, const int* row_ptrs, const int* col_indices,
        const float* pre_traces, const float* post_traces,
        int nnz, int rows,
        float a_plus, float a_minus, float lr,
        float min_trace, float w_min, float w_max);

    return nimcp_gpu_stdp_weight_update_csr(
        gpu_ctx,
        sparse_weights->data.csr.values,
        sparse_weights->data.csr.row_ptrs,
        sparse_weights->data.csr.col_indices,
        (const float*)state->pre_traces->data,
        (const float*)state->post_traces->data,
        nnz, sparse_weights->data.csr.rows,
        state->stdp_params.A_plus,
        state->stdp_params.A_minus,
        learning_rate,
        0.1f,  /* min_trace_threshold */
        -1.0f, 1.0f);  /* weight bounds */
}

/**
 * Apply BCM weight updates to CSR sparse weights on GPU.
 *
 * For each weight w[i] at (row, col):
 *   phi = post_act[row] * (post_act[row] - threshold[row])
 *   w[i] += lr * phi * pre_act[col]
 *   w[i] = clamp(w[i], 0, 1)
 */
int gpu_plasticity_bcm_update_weights(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    nimcp_sparse_tensor_t* sparse_weights,
    float learning_rate)
{
    if (!gpu_ctx || !state || !sparse_weights) return -1;
    if (sparse_weights->format != SPARSE_FORMAT_CSR) return -1;
    if (!state->bcm_thresholds || !state->pre_activity || !state->post_activity) return -1;

    int nnz = sparse_weights->data.csr.nnz;
    if (nnz == 0) return 0;

    extern int nimcp_gpu_bcm_weight_update_csr(
        nimcp_gpu_context_t* ctx,
        float* csr_values, const int* row_ptrs, const int* col_indices,
        const float* post_activity, const float* pre_activity,
        const float* thresholds,
        int nnz, int rows,
        float lr, float w_min, float w_max);

    return nimcp_gpu_bcm_weight_update_csr(
        gpu_ctx,
        sparse_weights->data.csr.values,
        sparse_weights->data.csr.row_ptrs,
        sparse_weights->data.csr.col_indices,
        (const float*)state->post_activity->data,
        (const float*)state->pre_activity->data,
        (const float*)state->bcm_thresholds->data,
        nnz, sparse_weights->data.csr.rows,
        learning_rate,
        -1.0f, 1.0f);
}

/**
 * Apply homeostatic synaptic scaling to CSR sparse weights on GPU.
 *
 * For each row (neuron): scale = target_rate / max(avg_rate[row], eps)
 * For each weight w[i] in that row: w[i] *= clamp(scale, 0.1, 10.0)
 */
int gpu_plasticity_homeostatic_scale_weights(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    nimcp_sparse_tensor_t* sparse_weights,
    float target_rate)
{
    if (!gpu_ctx || !state || !sparse_weights) return -1;
    if (sparse_weights->format != SPARSE_FORMAT_CSR) return -1;
    if (!state->avg_rates) return -1;

    int nnz = sparse_weights->data.csr.nnz;
    if (nnz == 0) return 0;

    extern int nimcp_gpu_homeostatic_scale_csr(
        nimcp_gpu_context_t* ctx,
        float* csr_values, const int* row_ptrs,
        const float* avg_rates,
        int nnz, int rows,
        float target_rate, float min_scale, float max_scale);

    return nimcp_gpu_homeostatic_scale_csr(
        gpu_ctx,
        sparse_weights->data.csr.values,
        sparse_weights->data.csr.row_ptrs,
        (const float*)state->avg_rates->data,
        nnz, sparse_weights->data.csr.rows,
        target_rate,
        0.1f, 10.0f);
}

/* ============================================================================
 * Wrapper: apply all three plasticity updates to all layers
 * ============================================================================ */

#include "gpu/training/nimcp_training_bridge.h"

int gpu_plasticity_update_all_weights(
    nimcp_gpu_context_t* gpu_ctx,
    gpu_plasticity_state_t* state,
    nimcp_gpu_weight_cache_t* wc,
    float learning_rate,
    float target_rate)
{
    if (!gpu_ctx || !state || !wc || !wc->sparse_weights) return -1;

    int total = 0;
    for (uint32_t li = 0; li + 1 < wc->num_layers; li++) {
        nimcp_sparse_tensor_t* sw = wc->sparse_weights[li];
        if (!sw) continue;

        int rc;
        rc = gpu_plasticity_stdp_update_weights(gpu_ctx, state, sw, learning_rate);
        if (rc == 0) total++;
        rc = gpu_plasticity_bcm_update_weights(gpu_ctx, state, sw, learning_rate * 0.1f);
        if (rc == 0) total++;
        rc = gpu_plasticity_homeostatic_scale_weights(gpu_ctx, state, sw, target_rate);
        if (rc == 0) total++;
    }
    return total;
}
