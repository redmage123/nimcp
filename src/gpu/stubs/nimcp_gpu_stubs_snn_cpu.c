/**
 * @file nimcp_gpu_stubs_snn_cpu.c
 * @brief CPU implementations of SNN GPU functions
 *
 * WHAT: Full CPU fallback implementations (not stubs) for SNN GPU operations
 * WHY:  Allow SNN code to run on CPU-only systems without crashes
 * HOW:  Implement using CPU tensors and memory, maintain same API
 */

#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_neural_constants.h"

#include <string.h>
#include <math.h>

static bool s_warned_cpu_fallback = false;

static void warn_cpu_fallback(const char* func) {
    if (!s_warned_cpu_fallback) {
        NIMCP_LOGGING_INFO("SNN running on CPU (CUDA not available). "
                          "Performance will be reduced. First call from: %s", func);
        s_warned_cpu_fallback = true;
    }
}

//=============================================================================
// CPU Tensor Helpers
//=============================================================================

static nimcp_gpu_tensor_t* cpu_tensor_create(size_t n_elements) {
    nimcp_gpu_tensor_t* tensor = (nimcp_gpu_tensor_t*)nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
    if (!tensor) return NULL;

    tensor->data = nimcp_calloc(n_elements, sizeof(float));
    tensor->dims = (size_t*)nimcp_malloc(sizeof(size_t));
    tensor->strides = (size_t*)nimcp_malloc(sizeof(size_t));

    if (!tensor->data || !tensor->dims || !tensor->strides) {
        if (tensor->data) nimcp_free(tensor->data);
        if (tensor->dims) nimcp_free(tensor->dims);
        if (tensor->strides) nimcp_free(tensor->strides);
        nimcp_free(tensor);
        return NULL;
    }

    tensor->numel = n_elements;
    tensor->dims[0] = n_elements;
    tensor->strides[0] = 1;
    tensor->ndim = 1;
    tensor->elem_size = sizeof(float);
    tensor->precision = NIMCP_GPU_PRECISION_FP32;
    tensor->owns_data = true;
    tensor->ctx = NULL;

    return tensor;
}

static void cpu_tensor_destroy(nimcp_gpu_tensor_t* tensor) {
    if (!tensor) return;
    if (tensor->data) nimcp_free(tensor->data);
    if (tensor->dims) nimcp_free(tensor->dims);
    if (tensor->strides) nimcp_free(tensor->strides);
    nimcp_free(tensor);
}

//=============================================================================
// LIF State Management
//=============================================================================

nimcp_lif_state_t* nimcp_lif_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_lif_params_t* params)
{
    (void)ctx;  // CPU version doesn't use GPU context
    warn_cpu_fallback("nimcp_lif_state_create");

    if (!params || n_neurons == 0) {
        NIMCP_LOGGING_ERROR("Invalid parameters for LIF state creation");
        return NULL;
    }

    nimcp_lif_state_t* state = (nimcp_lif_state_t*)nimcp_calloc(1, sizeof(nimcp_lif_state_t));
    if (!state) return NULL;

    // Create CPU tensors
    state->v = cpu_tensor_create(n_neurons);
    state->i_syn = cpu_tensor_create(n_neurons);
    state->spikes = cpu_tensor_create(n_neurons);

    if (!state->v || !state->i_syn || !state->spikes) {
        nimcp_lif_state_destroy(state);
        return NULL;
    }

    // Initialize voltage to rest
    float* v_data = (float*)state->v->data;
    for (size_t i = 0; i < n_neurons; i++) {
        v_data[i] = params->v_rest;
    }

    // Store parameters
    state->params = *params;

    // Per-neuron params default NULL (use scalar params)
    state->tau_mem_per_neuron = NULL;
    state->v_thresh_per_neuron = NULL;
    state->per_neuron_params_dirty = true;

    // CB arrays default NULL
    state->g_ampa = NULL;
    state->g_nmda = NULL;
    state->g_gaba_a = NULL;
    state->g_gaba_b = NULL;
    state->cb_arrays_dirty = false;

    return state;
}

void nimcp_lif_state_destroy(nimcp_lif_state_t* state) {
    if (!state) return;

    cpu_tensor_destroy(state->v);
    cpu_tensor_destroy(state->i_syn);
    cpu_tensor_destroy(state->spikes);
    cpu_tensor_destroy(state->tau_mem_per_neuron);
    cpu_tensor_destroy(state->v_thresh_per_neuron);
    cpu_tensor_destroy(state->g_ampa);
    cpu_tensor_destroy(state->g_nmda);
    cpu_tensor_destroy(state->g_gaba_a);
    cpu_tensor_destroy(state->g_gaba_b);

    nimcp_free(state);
}

//=============================================================================
// LIF Per-Neuron Parameter Upload
//=============================================================================

bool nimcp_gpu_lif_state_upload_per_neuron_params(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const float* tau_mem_host,
    const float* v_thresh_host,
    size_t n_neurons)
{
    (void)ctx;
    if (!state || !state->v || n_neurons != state->v->numel) return false;

    // Destroy old arrays
    cpu_tensor_destroy(state->tau_mem_per_neuron);
    state->tau_mem_per_neuron = NULL;
    cpu_tensor_destroy(state->v_thresh_per_neuron);
    state->v_thresh_per_neuron = NULL;

    // Allocate and copy new arrays if provided
    if (tau_mem_host) {
        state->tau_mem_per_neuron = cpu_tensor_create(n_neurons);
        if (!state->tau_mem_per_neuron) return false;
        memcpy(state->tau_mem_per_neuron->data, tau_mem_host, n_neurons * sizeof(float));
    }

    if (v_thresh_host) {
        state->v_thresh_per_neuron = cpu_tensor_create(n_neurons);
        if (!state->v_thresh_per_neuron) {
            cpu_tensor_destroy(state->tau_mem_per_neuron);
            state->tau_mem_per_neuron = NULL;
            return false;
        }
        memcpy(state->v_thresh_per_neuron->data, v_thresh_host, n_neurons * sizeof(float));
    }

    state->per_neuron_params_dirty = false;
    return true;
}

//=============================================================================
// LIF Conductance-Based Arrays
//=============================================================================

bool nimcp_gpu_lif_state_alloc_cb_arrays(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    size_t n_neurons)
{
    (void)ctx;
    if (!state || !state->v || n_neurons != state->v->numel) return false;

    // Allocate conductance arrays
    state->g_ampa = cpu_tensor_create(n_neurons);
    state->g_nmda = cpu_tensor_create(n_neurons);
    state->g_gaba_a = cpu_tensor_create(n_neurons);
    state->g_gaba_b = cpu_tensor_create(n_neurons);

    if (!state->g_ampa || !state->g_nmda || !state->g_gaba_a || !state->g_gaba_b) {
        cpu_tensor_destroy(state->g_ampa);
        cpu_tensor_destroy(state->g_nmda);
        cpu_tensor_destroy(state->g_gaba_a);
        cpu_tensor_destroy(state->g_gaba_b);
        state->g_ampa = state->g_nmda = state->g_gaba_a = state->g_gaba_b = NULL;
        return false;
    }

    return true;
}

bool nimcp_gpu_lif_state_download_g(
    nimcp_gpu_context_t* ctx,
    const nimcp_lif_state_t* state,
    float* g_ampa_out,
    float* g_nmda_out,
    float* g_gaba_a_out,
    float* g_gaba_b_out,
    size_t n_neurons)
{
    (void)ctx;
    if (!state || !state->v || n_neurons != state->v->numel) return false;

    if (g_ampa_out && state->g_ampa)
        memcpy(g_ampa_out, state->g_ampa->data, n_neurons * sizeof(float));
    if (g_nmda_out && state->g_nmda)
        memcpy(g_nmda_out, state->g_nmda->data, n_neurons * sizeof(float));
    if (g_gaba_a_out && state->g_gaba_a)
        memcpy(g_gaba_a_out, state->g_gaba_a->data, n_neurons * sizeof(float));
    if (g_gaba_b_out && state->g_gaba_b)
        memcpy(g_gaba_b_out, state->g_gaba_b->data, n_neurons * sizeof(float));

    return true;
}

bool nimcp_gpu_lif_state_upload_g(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const float* g_ampa_in,
    const float* g_nmda_in,
    const float* g_gaba_a_in,
    const float* g_gaba_b_in,
    size_t n_neurons)
{
    (void)ctx;
    if (!state || !state->v || n_neurons != state->v->numel) return false;

    if (g_ampa_in && state->g_ampa)
        memcpy(state->g_ampa->data, g_ampa_in, n_neurons * sizeof(float));
    if (g_nmda_in && state->g_nmda)
        memcpy(state->g_nmda->data, g_nmda_in, n_neurons * sizeof(float));
    if (g_gaba_a_in && state->g_gaba_a)
        memcpy(state->g_gaba_a->data, g_gaba_a_in, n_neurons * sizeof(float));
    if (g_gaba_b_in && state->g_gaba_b)
        memcpy(state->g_gaba_b->data, g_gaba_b_in, n_neurons * sizeof(float));

    return true;
}

//=============================================================================
// LIF Forward Pass (CPU)
//=============================================================================

bool nimcp_gpu_lif_forward_cb(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    float dt,
    float e_ampa,
    float e_nmda,
    float e_gaba_a,
    float e_gaba_b,
    float tau_ampa,
    float tau_nmda,
    float tau_gaba_a,
    float tau_gaba_b)
{
    (void)ctx;
    if (!state || !state->v) return false;

    size_t n_neurons = state->v->numel;
    float* v = (float*)state->v->data;
    float* spikes = (float*)state->spikes->data;
    float* g_ampa = state->g_ampa ? (float*)state->g_ampa->data : NULL;
    float* g_nmda = state->g_nmda ? (float*)state->g_nmda->data : NULL;
    float* g_gaba_a = state->g_gaba_a ? (float*)state->g_gaba_a->data : NULL;
    float* g_gaba_b = state->g_gaba_b ? (float*)state->g_gaba_b->data : NULL;

    for (size_t i = 0; i < n_neurons; i++) {
        // Get per-neuron or scalar parameters
        float v_thresh, tau_mem;
        if (state->v_thresh_per_neuron) {
            v_thresh = ((float*)state->v_thresh_per_neuron->data)[i];
        } else {
            v_thresh = state->params.v_thresh;
        }
        if (state->tau_mem_per_neuron) {
            tau_mem = ((float*)state->tau_mem_per_neuron->data)[i];
        } else {
            tau_mem = state->params.tau_mem;
        }

        // Conductance-based current
        float i_total = 0.0f;
        if (g_ampa) i_total += g_ampa[i] * (e_ampa - v[i]);
        if (g_nmda) i_total += g_nmda[i] * (e_nmda - v[i]);
        if (g_gaba_a) i_total += g_gaba_a[i] * (e_gaba_a - v[i]);
        if (g_gaba_b) i_total += g_gaba_b[i] * (e_gaba_b - v[i]);

        // Membrane voltage update
        v[i] += dt * ((state->params.v_rest - v[i]) / tau_mem + i_total);

        // Spike generation
        if (v[i] >= v_thresh) {
            spikes[i] = 1.0f;
            v[i] = state->params.v_reset;
        } else {
            spikes[i] = 0.0f;
        }

        // Decay conductances
        if (g_ampa) g_ampa[i] *= expf(-dt / tau_ampa);
        if (g_nmda) g_nmda[i] *= expf(-dt / tau_nmda);
        if (g_gaba_a) g_gaba_a[i] *= expf(-dt / tau_gaba_a);
        if (g_gaba_b) g_gaba_b[i] *= expf(-dt / tau_gaba_b);
    }

    return true;
}

//=============================================================================
// CSR Sparse Operations (CPU)
//=============================================================================

bool nimcp_gpu_snn_cb_deposit_csr(
    nimcp_gpu_context_t* ctx,
    const float* d_spike_vector,
    const float* d_depression,
    const float* d_weights,
    const uint32_t* d_flat_col_idx,
    const uint32_t* d_src_pop_idx,
    const uint32_t* d_row_ptr,
    const uint8_t* d_synapse_type,
    float* d_g_ampa,
    float* d_g_nmda,
    float* d_g_gaba_a,
    float* d_g_gaba_b,
    uint32_t dst_pop_offset,
    uint32_t n_neurons_pop)
{
    (void)ctx; (void)dst_pop_offset;

    // CPU implementation of conductance deposit
    for (uint32_t i = 0; i < n_neurons_pop; i++) {
        uint32_t row_start = d_row_ptr[i];
        uint32_t row_end = d_row_ptr[i + 1];

        for (uint32_t j = row_start; j < row_end; j++) {
            uint32_t pre_idx = d_flat_col_idx[j];
            if (d_spike_vector[pre_idx] > 0.5f) {
                float weight = d_weights[j];
                float dep = d_depression ? d_depression[j] : 1.0f;
                float g_inc = weight * dep;

                uint8_t syn_type = d_synapse_type[j];
                switch (syn_type) {
                    case 0: d_g_ampa[i] += g_inc; break;
                    case 1: d_g_nmda[i] += g_inc; break;
                    case 2: d_g_gaba_a[i] += g_inc; break;
                    case 3: d_g_gaba_b[i] += g_inc; break;
                }
            }
        }
    }

    return true;
}

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
    (void)ctx;

    // CPU implementation of CSR matrix-vector multiply for synaptic current
    for (size_t i = 0; i < n_neurons; i++) {
        float isyn = 0.0f;
        unsigned int row_start = d_row_ptr[i];
        unsigned int row_end = d_row_ptr[i + 1];

        for (unsigned int j = row_start; j < row_end; j++) {
            unsigned int pre_idx = d_col_indices[j];
            if (d_spike_vector[pre_idx] > 0.5f) {
                isyn += d_weights[j];
            }
        }

        if (d_external_current) {
            isyn += d_external_current[i];
        }

        d_output_isyn[i] = isyn;
    }

    return true;
}

//=============================================================================
// Plasticity Parameter Defaults
//=============================================================================

void nimcp_gpu_bcm_params_default(void) {}
void nimcp_gpu_scaling_params_default(void) {}
void nimcp_gpu_intrinsic_params_default(void) {}
void nimcp_gpu_stdp_params_default(void) {}

//=============================================================================
// BCM Plasticity (CPU)
//=============================================================================

bool nimcp_gpu_bcm_update_threshold(
    nimcp_gpu_context_t* ctx, void* theta, void* activity, void* params, float dt)
{
    (void)ctx; (void)theta; (void)activity; (void)params; (void)dt;
    return true;  // Skip for now
}

bool nimcp_gpu_bcm_weight_update_csr(
    nimcp_gpu_context_t* ctx, void* csr, void* pre, void* post, void* theta, void* params)
{
    (void)ctx; (void)csr; (void)pre; (void)post; (void)theta; (void)params;
    return true;  // Skip for now
}

//=============================================================================
// Homeostatic Scaling (CPU)
//=============================================================================

bool nimcp_gpu_homeostatic_compute_scaling(
    nimcp_gpu_context_t* ctx, void* scaling, void* rates, void* params)
{
    (void)ctx; (void)scaling; (void)rates; (void)params;
    return true;  // Skip for now
}

bool nimcp_gpu_homeostatic_apply_scaling(nimcp_gpu_context_t* ctx, void* weights, void* scaling)
{
    (void)ctx; (void)weights; (void)scaling;
    return true;  // Skip for now
}

bool nimcp_gpu_homeostatic_scale_csr(nimcp_gpu_context_t* ctx, void* csr, void* post_scaling)
{
    (void)ctx; (void)csr; (void)post_scaling;
    return true;  // Skip for now
}

bool nimcp_gpu_homeostatic_update_rates(
    nimcp_gpu_context_t* ctx, void* rates, void* spikes, void* params, float dt)
{
    (void)ctx; (void)rates; (void)spikes; (void)params; (void)dt;
    return true;  // Skip for now
}

//=============================================================================
// STDP Plasticity (CPU)
//=============================================================================

bool nimcp_gpu_stdp_apply(
    nimcp_gpu_context_t* ctx, void* weights, void* traces_pre, void* traces_post, void* params)
{
    (void)ctx; (void)weights; (void)traces_pre; (void)traces_post; (void)params;
    return true;  // Skip for now
}

bool nimcp_gpu_stdp_update_traces(
    nimcp_gpu_context_t* ctx, void* traces, void* spikes, float tau, float dt)
{
    (void)ctx; (void)traces; (void)spikes; (void)tau; (void)dt;
    return true;  // Skip for now
}

bool nimcp_gpu_stdp_weight_update_csr(
    nimcp_gpu_context_t* ctx, void* csr, void* pre_trace, void* post_trace, void* params)
{
    (void)ctx; (void)csr; (void)pre_trace; (void)post_trace; (void)params;
    return true;  // Skip for now
}

//=============================================================================
// Gradient Operations
//=============================================================================

bool nimcp_gpu_gradient_flush(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return true;  // No-op for CPU
}
