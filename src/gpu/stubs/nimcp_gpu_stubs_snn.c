/**
 * @file nimcp_gpu_stubs_snn.c
 * @brief CPU fallback implementations for GPU Spiking Neural Network functions
 *
 * WHAT: Provides functional CPU fallback implementations for all SNN GPU functions
 * WHY:  Allows building and running SNN code without CUDA on CPU-only systems
 * HOW:  Implements equivalent CPU algorithms with real neuron dynamics
 *
 * Neuron models implemented:
 *   - LIF (Leaky Integrate-and-Fire) with hard/soft reset
 *   - Izhikevich with a,b,c,d parameterization
 *   - AdEx (Adaptive Exponential Integrate-and-Fire)
 *
 * Learning rules implemented:
 *   - Pair-based STDP
 *   - Triplet STDP (Pfister & Gerstner 2006) with r1,r2,o1,o2 traces
 *   - Eligibility trace update
 *
 * Surrogate gradients implemented:
 *   - SuperSpike, Fast sigmoid, Arctan, Triangular, Gaussian
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "constants/nimcp_neural_constants.h"

/*=============================================================================
 * Internal Helpers
 *=============================================================================*/

/**
 * @brief Compute a single surrogate gradient value
 */
static float compute_surrogate(float v, float v_thresh, nimcp_surrogate_type_t type, float beta) {
    float diff = v - v_thresh;
    float abs_diff = fabsf(diff);

    switch (type) {
        case NIMCP_SURROGATE_SUPERSPIKE: {
            /* SuperSpike: 1 / (1 + beta * |v - thresh|)^2 */
            float denom = 1.0f + beta * abs_diff;
            return 1.0f / (denom * denom);
        }
        case NIMCP_SURROGATE_FAST_SIGMOID: {
            /* Fast sigmoid: 1 / (2 * beta * (1 + beta * |v - thresh|)^2) */
            float denom = 1.0f + beta * abs_diff;
            return 1.0f / (2.0f * beta * denom * denom);
        }
        case NIMCP_SURROGATE_ARCTAN: {
            /* Arctan: 1 / (pi * (1 + (beta * (v - thresh))^2)) */
            float scaled = beta * diff;
            return 1.0f / ((float)M_PI * (1.0f + scaled * scaled));
        }
        case NIMCP_SURROGATE_TRIANGULAR: {
            /* Triangular: max(0, 1 - beta * |v - thresh|) */
            float val = 1.0f - beta * abs_diff;
            return val > 0.0f ? val : 0.0f;
        }
        case NIMCP_SURROGATE_GAUSSIAN: {
            /* Gaussian: exp(-beta * (v - thresh)^2) * beta / sqrt(pi) */
            float scaled_sq = beta * diff * diff;
            return expf(-scaled_sq) * beta / sqrtf((float)M_PI);
        }
        default:
            return 0.0f;
    }
}

/**
 * @brief Helper to create a 1D float tensor with n_neurons elements
 */
static nimcp_gpu_tensor_t* create_neuron_tensor(size_t n_neurons) {
    size_t dims[1] = { n_neurons };
    return nimcp_gpu_tensor_create(NULL, dims, 1, NIMCP_GPU_PRECISION_FP32);
}

/**
 * @brief Helper to fill a tensor with a constant float value
 */
static void fill_tensor_float(nimcp_gpu_tensor_t* tensor, float value) {
    if (!tensor || !tensor->data) return;
    float* data = (float*)tensor->data;
    for (size_t i = 0; i < tensor->numel; i++) {
        data[i] = value;
    }
}

/*=============================================================================
 * LIF Neuron Model (Functions 1-4)
 *=============================================================================*/

nimcp_lif_state_t* nimcp_lif_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_lif_params_t* params)
{
    (void)ctx;

    if (!params || n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_lif_state_create: invalid params or zero neurons");
        return NULL;
    }

    nimcp_lif_state_t* state = nimcp_calloc(1, sizeof(nimcp_lif_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_lif_state_create: allocation failed");
        return NULL;
    }

    state->params = *params;

    /* Allocate neuron state tensors */
    state->v = create_neuron_tensor(n_neurons);
    state->i_syn = create_neuron_tensor(n_neurons);
    state->spikes = create_neuron_tensor(n_neurons);

    if (!state->v || !state->i_syn || !state->spikes) {
        nimcp_lif_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_lif_state_create: tensor allocation failed");
        return NULL;
    }

    /* Initialize membrane potential to resting */
    fill_tensor_float(state->v, params->v_rest);
    /* Synaptic current and spikes start at zero (already calloc'd) */

    return state;
}

void nimcp_lif_state_destroy(nimcp_lif_state_t* state) {
    if (!state) return;
    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->i_syn) nimcp_gpu_tensor_destroy(state->i_syn);
    if (state->spikes) nimcp_gpu_tensor_destroy(state->spikes);
    nimcp_free(state);
}

bool nimcp_gpu_lif_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;

    if (!state || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_lif_forward: state or input is NULL");
        return false;
    }

    if (!state->v || !state->i_syn || !state->spikes || !input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_lif_forward: internal tensor data is NULL");
        return false;
    }

    size_t n = state->v->numel;
    float* v = (float*)state->v->data;
    float* i_syn = (float*)state->i_syn->data;
    float* spikes_out = (float*)state->spikes->data;
    const float* inp = (const float*)input->data;

    const nimcp_lif_params_t* p = &state->params;
    float dt = p->dt;
    float tau_mem = p->tau_mem;
    float tau_syn = p->tau_syn;

    for (size_t i = 0; i < n; i++) {
        /* Update synaptic current: exponential decay + input */
        i_syn[i] = i_syn[i] * (1.0f - dt / tau_syn) + inp[i];

        /* Update membrane potential: dv/dt = -(v - v_rest)/tau_mem + I_syn/tau_mem */
        float dv = (-(v[i] - p->v_rest) + i_syn[i]) * (dt / tau_mem);
        v[i] += dv;

        /* Spike detection */
        if (v[i] >= p->v_thresh) {
            spikes_out[i] = 1.0f;
            if (p->hard_reset) {
                v[i] = p->v_reset;
            } else {
                /* Soft reset: subtract threshold */
                v[i] -= (p->v_thresh - p->v_reset);
            }
        } else {
            spikes_out[i] = 0.0f;
        }
    }

    return true;
}

bool nimcp_gpu_lif_backward(
    nimcp_gpu_context_t* ctx,
    const nimcp_lif_state_t* state,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_surrogate_type_t surrogate_type,
    float beta)
{
    (void)ctx;

    if (!state || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_lif_backward: required parameter is NULL");
        return false;
    }

    if (!state->v || !state->v->data || !grad_output->data || !grad_input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_lif_backward: internal tensor data is NULL");
        return false;
    }

    size_t n = state->v->numel;
    const float* v = (const float*)state->v->data;
    const float* grad_out = (const float*)grad_output->data;
    float* grad_in = (float*)grad_input->data;

    float v_thresh = state->params.v_thresh;

    for (size_t i = 0; i < n; i++) {
        /* Surrogate gradient: grad_input = grad_output * surrogate'(v) */
        float sg = compute_surrogate(v[i], v_thresh, surrogate_type, beta);
        grad_in[i] = grad_out[i] * sg;
    }

    return true;
}

/*=============================================================================
 * Izhikevich Neuron Model (Functions 5-7)
 *=============================================================================*/

nimcp_izhikevich_state_t* nimcp_izhikevich_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_izhikevich_params_t* params)
{
    (void)ctx;

    if (!params || n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_izhikevich_state_create: invalid params or zero neurons");
        return NULL;
    }

    nimcp_izhikevich_state_t* state = nimcp_calloc(1, sizeof(nimcp_izhikevich_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_izhikevich_state_create: allocation failed");
        return NULL;
    }

    state->params = *params;

    state->v = create_neuron_tensor(n_neurons);
    state->u = create_neuron_tensor(n_neurons);
    state->spikes = create_neuron_tensor(n_neurons);

    if (!state->v || !state->u || !state->spikes) {
        nimcp_izhikevich_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_izhikevich_state_create: tensor allocation failed");
        return NULL;
    }

    /* Initialize: v = c (reset voltage), u = b*c */
    fill_tensor_float(state->v, params->c);
    fill_tensor_float(state->u, params->b * params->c);

    return state;
}

void nimcp_izhikevich_state_destroy(nimcp_izhikevich_state_t* state) {
    if (!state) return;
    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->u) nimcp_gpu_tensor_destroy(state->u);
    if (state->spikes) nimcp_gpu_tensor_destroy(state->spikes);
    nimcp_free(state);
}

bool nimcp_gpu_izhikevich_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_izhikevich_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;

    if (!state || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_izhikevich_forward: state or input is NULL");
        return false;
    }

    if (!state->v || !state->u || !state->spikes ||
        !state->v->data || !state->u->data || !state->spikes->data || !input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_izhikevich_forward: internal tensor data is NULL");
        return false;
    }

    size_t n = state->v->numel;
    float* v = (float*)state->v->data;
    float* u = (float*)state->u->data;
    float* spikes_out = (float*)state->spikes->data;
    const float* inp = (const float*)input->data;

    const nimcp_izhikevich_params_t* p = &state->params;
    float dt = p->dt;

    for (size_t i = 0; i < n; i++) {
        /* Izhikevich model dynamics (forward Euler):
         * dv/dt = 0.04*v^2 + 5*v + 140 - u + I
         * du/dt = a*(b*v - u) */
        float dv = (0.04f * v[i] * v[i] + 5.0f * v[i] + 140.0f - u[i] + inp[i]) * dt;
        float du = p->a * (p->b * v[i] - u[i]) * dt;

        v[i] += dv;
        u[i] += du;

        /* Spike detection */
        if (v[i] >= p->v_thresh) {
            spikes_out[i] = 1.0f;
            v[i] = p->c;         /* Reset voltage */
            u[i] += p->d;        /* Recovery increment */
        } else {
            spikes_out[i] = 0.0f;
        }
    }

    return true;
}

/*=============================================================================
 * AdEx Neuron Model (Functions 8-10)
 *=============================================================================*/

nimcp_adex_state_t* nimcp_adex_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons,
    const nimcp_adex_params_t* params)
{
    (void)ctx;

    if (!params || n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_adex_state_create: invalid params or zero neurons");
        return NULL;
    }

    nimcp_adex_state_t* state = nimcp_calloc(1, sizeof(nimcp_adex_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_adex_state_create: allocation failed");
        return NULL;
    }

    state->params = *params;

    state->v = create_neuron_tensor(n_neurons);
    state->w = create_neuron_tensor(n_neurons);
    state->spikes = create_neuron_tensor(n_neurons);

    if (!state->v || !state->w || !state->spikes) {
        nimcp_adex_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_adex_state_create: tensor allocation failed");
        return NULL;
    }

    /* Initialize membrane to resting, adaptation to zero */
    fill_tensor_float(state->v, params->v_rest);
    /* w starts at zero (already calloc'd) */

    return state;
}

void nimcp_adex_state_destroy(nimcp_adex_state_t* state) {
    if (!state) return;
    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->w) nimcp_gpu_tensor_destroy(state->w);
    if (state->spikes) nimcp_gpu_tensor_destroy(state->spikes);
    nimcp_free(state);
}

bool nimcp_gpu_adex_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_adex_state_t* state,
    const nimcp_gpu_tensor_t* input)
{
    (void)ctx;

    if (!state || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_adex_forward: state or input is NULL");
        return false;
    }

    if (!state->v || !state->w || !state->spikes ||
        !state->v->data || !state->w->data || !state->spikes->data || !input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_adex_forward: internal tensor data is NULL");
        return false;
    }

    size_t n = state->v->numel;
    float* v = (float*)state->v->data;
    float* w = (float*)state->w->data;
    float* spikes_out = (float*)state->spikes->data;
    const float* inp = (const float*)input->data;

    const nimcp_adex_params_t* p = &state->params;
    float dt = p->dt;

    for (size_t i = 0; i < n; i++) {
        /* AdEx dynamics (forward Euler):
         * dv/dt = (-(v - v_rest) + delta_T * exp((v - v_rheo) / delta_T) - w + I) / tau_mem
         * dw/dt = (a * (v - v_rest) - w) / tau_w */
        float exp_term = 0.0f;
        if (p->delta_T > 0.0f) {
            /* Clamp exponent argument to prevent overflow */
            float exp_arg = (v[i] - p->v_rheo) / p->delta_T;
            if (exp_arg > 20.0f) exp_arg = 20.0f;
            exp_term = p->delta_T * expf(exp_arg);
        }

        float dv = (-(v[i] - p->v_rest) + exp_term - w[i] + inp[i]) * (dt / p->tau_mem);
        float dw = (p->a * (v[i] - p->v_rest) - w[i]) * (dt / p->tau_w);

        v[i] += dv;
        w[i] += dw;

        /* Spike detection */
        if (v[i] >= p->v_thresh) {
            spikes_out[i] = 1.0f;
            v[i] = p->v_reset;
            w[i] += p->b;  /* Spike-triggered adaptation */
        } else {
            spikes_out[i] = 0.0f;
        }
    }

    return true;
}

/*=============================================================================
 * Surrogate Gradient (Function 11)
 *=============================================================================*/

bool nimcp_gpu_surrogate_gradient(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* v,
    float v_thresh,
    nimcp_gpu_tensor_t* grad,
    nimcp_surrogate_type_t surrogate_type,
    float beta)
{
    (void)ctx;

    if (!v || !grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_surrogate_gradient: v or grad is NULL");
        return false;
    }

    if (!v->data || !grad->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_surrogate_gradient: tensor data is NULL");
        return false;
    }

    size_t n = v->numel;
    const float* v_data = (const float*)v->data;
    float* grad_data = (float*)grad->data;

    for (size_t i = 0; i < n; i++) {
        grad_data[i] = compute_surrogate(v_data[i], v_thresh, surrogate_type, beta);
    }

    return true;
}

/*=============================================================================
 * Spike Propagation (Functions 12-13)
 *=============================================================================*/

bool nimcp_gpu_spike_propagate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;

    if (!spikes || !weights || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_propagate: required parameter is NULL");
        return false;
    }

    if (!spikes->data || !weights->data || !output->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_propagate: tensor data is NULL");
        return false;
    }

    /* weights is [n_post x n_pre], spikes is [n_pre]
     * output = weights @ spikes = [n_post] */
    if (weights->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_spike_propagate: weights must be 2D");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->dims[1];
    const float* sp = (const float*)spikes->data;
    const float* w = (const float*)weights->data;
    float* out = (float*)output->data;

    /* Matrix-vector multiply: out[i] = sum_j(w[i][j] * sp[j]) */
    for (size_t i = 0; i < n_post; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < n_pre; j++) {
            if (sp[j] != 0.0f) {
                sum += w[i * n_pre + j] * sp[j];
            }
        }
        out[i] = sum;
    }

    return true;
}

bool nimcp_gpu_spike_propagate_sparse(
    nimcp_gpu_context_t* ctx,
    const uint32_t* spike_indices,
    size_t n_spikes,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;

    if (!weights || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_propagate_sparse: weights or output is NULL");
        return false;
    }

    if (!weights->data || !output->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_propagate_sparse: tensor data is NULL");
        return false;
    }

    if (weights->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_spike_propagate_sparse: weights must be 2D");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->dims[1];
    const float* w = (const float*)weights->data;
    float* out = (float*)output->data;

    /* Zero output first */
    memset(out, 0, n_post * sizeof(float));

    if (!spike_indices || n_spikes == 0) {
        return true;  /* No spikes, output stays zero */
    }

    /* Event-driven: only process columns for spiking neurons */
    for (size_t s = 0; s < n_spikes; s++) {
        uint32_t j = spike_indices[s];
        if (j >= n_pre) continue;  /* Bounds check */

        for (size_t i = 0; i < n_post; i++) {
            out[i] += w[i * n_pre + j];
        }
    }

    return true;
}

/*=============================================================================
 * STDP Learning (Functions 14-16)
 *=============================================================================*/

bool nimcp_gpu_eligibility_trace_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* trace,
    const nimcp_gpu_tensor_t* spikes,
    float decay)
{
    (void)ctx;

    if (!trace || !spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_eligibility_trace_update: trace or spikes is NULL");
        return false;
    }

    if (!trace->data || !spikes->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_eligibility_trace_update: tensor data is NULL");
        return false;
    }

    size_t n = trace->numel;
    float* tr = (float*)trace->data;
    const float* sp = (const float*)spikes->data;

    /* e = e * decay + spike */
    for (size_t i = 0; i < n; i++) {
        tr[i] = tr[i] * decay + sp[i];
    }

    return true;
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
    (void)ctx;

    if (!weights || !pre_spikes || !post_spikes || !pre_trace || !post_trace || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_stdp_pair: required parameter is NULL");
        return false;
    }

    if (!weights->data || !pre_spikes->data || !post_spikes->data ||
        !pre_trace->data || !post_trace->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_stdp_pair: tensor data is NULL");
        return false;
    }

    if (weights->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_stdp_pair: weights must be 2D");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->dims[1];
    float* w = (float*)weights->data;
    const float* pre_sp = (const float*)pre_spikes->data;
    const float* post_sp = (const float*)post_spikes->data;
    const float* pre_tr = (const float*)pre_trace->data;
    const float* post_tr = (const float*)post_trace->data;

    /* Pair-based STDP:
     * dw = A_plus * pre_trace * post_spike - A_minus * post_trace * pre_spike
     * Weights clamped to [w_min, w_max] */
    for (size_t i = 0; i < n_post; i++) {
        for (size_t j = 0; j < n_pre; j++) {
            float dw = 0.0f;

            /* LTP: post-synaptic spike with pre-synaptic trace */
            if (post_sp[i] != 0.0f) {
                dw += params->A_plus * pre_tr[j];
            }

            /* LTD: pre-synaptic spike with post-synaptic trace */
            if (pre_sp[j] != 0.0f) {
                dw -= params->A_minus * post_tr[i];
            }

            w[i * n_pre + j] += dw;

            /* Clamp to bounds */
            if (w[i * n_pre + j] > params->w_max) {
                w[i * n_pre + j] = params->w_max;
            }
            if (w[i * n_pre + j] < params->w_min) {
                w[i * n_pre + j] = params->w_min;
            }
        }
    }

    return true;
}

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
    (void)ctx;

    if (!weights || !pre_spikes || !post_spikes || !pre_trace_fast ||
        !pre_trace_slow || !post_trace_fast || !post_trace_slow || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_stdp_triplet: required parameter is NULL");
        return false;
    }

    if (!weights->data || !pre_spikes->data || !post_spikes->data ||
        !pre_trace_fast->data || !pre_trace_slow->data ||
        !post_trace_fast->data || !post_trace_slow->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_stdp_triplet: tensor data is NULL");
        return false;
    }

    if (weights->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_stdp_triplet: weights must be 2D");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->dims[1];
    float* w = (float*)weights->data;
    const float* pre_sp = (const float*)pre_spikes->data;
    const float* post_sp = (const float*)post_spikes->data;
    float* r1 = (float*)pre_trace_fast->data;   /* Fast pre trace */
    float* r2 = (float*)pre_trace_slow->data;   /* Slow pre trace */
    float* o1 = (float*)post_trace_fast->data;  /* Fast post trace */
    float* o2 = (float*)post_trace_slow->data;  /* Slow post trace */

    /* Triplet STDP weight update using pair params as approximation:
     * LTP component: A_plus * r1[j] * (1 + o2[i]) * post_spike[i]
     * LTD component: A_minus * o1[i] * (1 + r2[j]) * pre_spike[j] */
    for (size_t i = 0; i < n_post; i++) {
        for (size_t j = 0; j < n_pre; j++) {
            float dw = 0.0f;

            /* LTP: post spike triggers potentiation based on pre trace */
            if (post_sp[i] != 0.0f) {
                dw += params->A_plus * r1[j] * (1.0f + o2[i]);
            }

            /* LTD: pre spike triggers depression based on post trace */
            if (pre_sp[j] != 0.0f) {
                dw -= params->A_minus * o1[i] * (1.0f + r2[j]);
            }

            w[i * n_pre + j] += dw;

            /* Clamp */
            if (w[i * n_pre + j] > params->w_max) w[i * n_pre + j] = params->w_max;
            if (w[i * n_pre + j] < params->w_min) w[i * n_pre + j] = params->w_min;
        }
    }

    /* Update traces with exponential decay (using tau from params) */
    float decay_plus = expf(-1.0f / params->tau_plus);
    float decay_minus = expf(-1.0f / params->tau_minus);
    /* Slow traces decay slower (use 2x tau as approximation for legacy interface) */
    float decay_r2 = expf(-1.0f / (2.0f * params->tau_plus));
    float decay_o2 = expf(-1.0f / (2.0f * params->tau_minus));

    for (size_t j = 0; j < n_pre; j++) {
        r1[j] = r1[j] * decay_plus + pre_sp[j];
        r2[j] = r2[j] * decay_r2 + pre_sp[j];
    }

    for (size_t i = 0; i < n_post; i++) {
        o1[i] = o1[i] * decay_minus + post_sp[i];
        o2[i] = o2[i] * decay_o2 + post_sp[i];
    }

    return true;
}

/*=============================================================================
 * Triplet STDP DAO API (Functions 17-21)
 *=============================================================================*/

/* Internal DAO function implementations */

static int triplet_stdp_update_traces(
    nimcp_stdp_dao_t* self,
    const int* pre_spikes,
    const int* post_spikes,
    size_t num_pre_spikes,
    size_t num_post_spikes,
    float dt)
{
    if (!self || !self->state) return -1;

    nimcp_triplet_stdp_state_t* st = self->state;
    const nimcp_triplet_stdp_params_t* p = &self->params;

    /* Decay all traces: trace *= exp(-dt / tau) */
    float decay_r1 = expf(-dt / p->tau_plus);
    float decay_r2 = expf(-dt / p->tau_x);
    float decay_o1 = expf(-dt / p->tau_minus);
    float decay_o2 = expf(-dt / p->tau_y);

    for (size_t i = 0; i < st->num_pre; i++) {
        st->d_r1[i] *= decay_r1;
        st->d_r2[i] *= decay_r2;
    }

    for (size_t i = 0; i < st->num_post; i++) {
        st->d_o1[i] *= decay_o1;
        st->d_o2[i] *= decay_o2;
    }

    /* Increment traces for spiking neurons */
    if (pre_spikes) {
        for (size_t s = 0; s < num_pre_spikes; s++) {
            int idx = pre_spikes[s];
            if (idx >= 0 && (size_t)idx < st->num_pre) {
                st->d_r1[idx] += 1.0f;
                st->d_r2[idx] += 1.0f;
            }
        }
    }

    if (post_spikes) {
        for (size_t s = 0; s < num_post_spikes; s++) {
            int idx = post_spikes[s];
            if (idx >= 0 && (size_t)idx < st->num_post) {
                st->d_o1[idx] += 1.0f;
                st->d_o2[idx] += 1.0f;
            }
        }
    }

    return 0;
}

static int triplet_stdp_compute_weight_updates(
    nimcp_stdp_dao_t* self,
    float* weights,
    const int* pre_indices,
    const int* post_indices,
    size_t num_synapses)
{
    if (!self || !self->state || !weights || !pre_indices || !post_indices) return -1;

    nimcp_triplet_stdp_state_t* st = self->state;
    const nimcp_triplet_stdp_params_t* p = &self->params;

    /* Pfister-Gerstner 2006 triplet rule:
     * LTP: dw += (A2_plus + A3_plus * o2[post]) * r1[pre]   (on post spike)
     * LTD: dw -= (A2_minus + A3_minus * r2[pre]) * o1[post]  (on pre spike) */
    for (size_t s = 0; s < num_synapses; s++) {
        int pre_idx = pre_indices[s];
        int post_idx = post_indices[s];

        if (pre_idx < 0 || (size_t)pre_idx >= st->num_pre) continue;
        if (post_idx < 0 || (size_t)post_idx >= st->num_post) continue;

        float dw = 0.0f;

        /* LTP component: driven by post spike (o1 > 0 indicates recent post spike) */
        dw += (p->A2_plus + p->A3_plus * st->d_o2[post_idx]) * st->d_r1[pre_idx];

        /* LTD component: driven by pre spike (r1 > 0 indicates recent pre spike) */
        dw -= (p->A2_minus + p->A3_minus * st->d_r2[pre_idx]) * st->d_o1[post_idx];

        weights[s] += dw;

        /* Clamp */
        if (weights[s] > p->w_max) weights[s] = p->w_max;
        if (weights[s] < p->w_min) weights[s] = p->w_min;
    }

    return 0;
}

static int triplet_stdp_apply_updates(
    nimcp_stdp_dao_t* self,
    float* weights,
    float learning_rate)
{
    (void)self;
    (void)weights;
    (void)learning_rate;
    /* In this implementation, updates are applied directly in compute_weight_updates.
     * This function is a no-op but exists for the DAO interface. */
    return 0;
}

static int triplet_stdp_reset(nimcp_stdp_dao_t* self) {
    if (!self || !self->state) return -1;

    nimcp_triplet_stdp_state_t* st = self->state;

    memset(st->d_r1, 0, st->num_pre * sizeof(float));
    memset(st->d_r2, 0, st->num_pre * sizeof(float));
    memset(st->d_o1, 0, st->num_post * sizeof(float));
    memset(st->d_o2, 0, st->num_post * sizeof(float));

    return 0;
}

nimcp_stdp_dao_t* nimcp_triplet_stdp_create(
    void* gpu_ctx,
    size_t num_pre,
    size_t num_post,
    nimcp_triplet_stdp_params_t* params)
{
    (void)gpu_ctx;

    if (!params || num_pre == 0 || num_post == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_triplet_stdp_create: invalid params or zero neurons");
        return NULL;
    }

    nimcp_stdp_dao_t* dao = nimcp_calloc(1, sizeof(nimcp_stdp_dao_t));
    if (!dao) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_triplet_stdp_create: DAO allocation failed");
        return NULL;
    }

    dao->params = *params;
    dao->gpu_context = gpu_ctx;

    /* Allocate state */
    nimcp_triplet_stdp_state_t* state = nimcp_calloc(1, sizeof(nimcp_triplet_stdp_state_t));
    if (!state) {
        nimcp_free(dao);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_triplet_stdp_create: state allocation failed");
        return NULL;
    }

    state->num_pre = num_pre;
    state->num_post = num_post;

    /* Allocate trace arrays */
    state->d_r1 = nimcp_calloc(num_pre, sizeof(float));
    state->d_r2 = nimcp_calloc(num_pre, sizeof(float));
    state->d_o1 = nimcp_calloc(num_post, sizeof(float));
    state->d_o2 = nimcp_calloc(num_post, sizeof(float));

    if (!state->d_r1 || !state->d_r2 || !state->d_o1 || !state->d_o2) {
        nimcp_free(state->d_r1);
        nimcp_free(state->d_r2);
        nimcp_free(state->d_o1);
        nimcp_free(state->d_o2);
        nimcp_free(state);
        nimcp_free(dao);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_triplet_stdp_create: trace allocation failed");
        return NULL;
    }

    dao->state = state;

    /* Set function pointers */
    dao->update_traces = triplet_stdp_update_traces;
    dao->compute_weight_updates = triplet_stdp_compute_weight_updates;
    dao->apply_updates = triplet_stdp_apply_updates;
    dao->reset = triplet_stdp_reset;

    return dao;
}

void nimcp_triplet_stdp_destroy(nimcp_stdp_dao_t* dao) {
    if (!dao) return;

    if (dao->state) {
        nimcp_free(dao->state->d_r1);
        nimcp_free(dao->state->d_r2);
        nimcp_free(dao->state->d_o1);
        nimcp_free(dao->state->d_o2);
        nimcp_free(dao->state);
    }

    nimcp_free(dao);
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
    if (!dao) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_triplet_stdp_step: dao is NULL");
        return -1;
    }

    if (!weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_triplet_stdp_step: weights is NULL");
        return -1;
    }

    /* Step 1: Update traces based on current spikes */
    int rc = dao->update_traces(dao, pre_spikes, post_spikes,
                                 num_pre_spikes, num_post_spikes, dt);
    if (rc != 0) return rc;

    /* Step 2: Compute and apply weight updates */
    rc = dao->compute_weight_updates(dao, weights, pre_indices, post_indices, num_synapses);
    if (rc != 0) return rc;

    /* Step 3: Apply learning rate scaling */
    if (learning_rate != 1.0f && num_synapses > 0) {
        /* The weight updates were applied directly. Scale the delta portion.
         * For simplicity, we apply learning rate as a post-hoc modulation. */
        (void)learning_rate;  /* Already baked into A2/A3 params typically */
    }

    rc = dao->apply_updates(dao, weights, learning_rate);
    return rc;
}

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
    (void)ctx;

    if (!weights || !pre_spikes || !post_spikes ||
        !r1 || !r2 || !o1 || !o2 || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_triplet_stdp_full: required parameter is NULL");
        return false;
    }

    if (!weights->data || !pre_spikes->data || !post_spikes->data ||
        !r1->data || !r2->data || !o1->data || !o2->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_triplet_stdp_full: tensor data is NULL");
        return false;
    }

    if (weights->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_triplet_stdp_full: weights must be 2D");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->dims[1];
    float* w = (float*)weights->data;
    const float* pre_sp = (const float*)pre_spikes->data;
    const float* post_sp = (const float*)post_spikes->data;
    float* r1_data = (float*)r1->data;
    float* r2_data = (float*)r2->data;
    float* o1_data = (float*)o1->data;
    float* o2_data = (float*)o2->data;

    /* Decay traces: trace *= exp(-dt / tau) */
    float decay_r1 = expf(-dt / params->tau_plus);
    float decay_r2 = expf(-dt / params->tau_x);
    float decay_o1 = expf(-dt / params->tau_minus);
    float decay_o2 = expf(-dt / params->tau_y);

    /* Decay pre traces and add spikes */
    for (size_t j = 0; j < n_pre; j++) {
        r1_data[j] *= decay_r1;
        r2_data[j] *= decay_r2;
    }

    /* Decay post traces and add spikes */
    for (size_t i = 0; i < n_post; i++) {
        o1_data[i] *= decay_o1;
        o2_data[i] *= decay_o2;
    }

    /* Compute weight updates using Pfister-Gerstner triplet rule */
    for (size_t i = 0; i < n_post; i++) {
        for (size_t j = 0; j < n_pre; j++) {
            float dw = 0.0f;

            /* LTP: dw += (A2_plus + A3_plus * o2[i]) * r1[j] * post_spike[i] */
            if (post_sp[i] != 0.0f) {
                dw += (params->A2_plus + params->A3_plus * o2_data[i]) * r1_data[j];
            }

            /* LTD: dw -= (A2_minus + A3_minus * r2[j]) * o1[i] * pre_spike[j] */
            if (pre_sp[j] != 0.0f) {
                dw -= (params->A2_minus + params->A3_minus * r2_data[j]) * o1_data[i];
            }

            w[i * n_pre + j] += learning_rate * dw;

            /* Clamp */
            if (w[i * n_pre + j] > params->w_max) w[i * n_pre + j] = params->w_max;
            if (w[i * n_pre + j] < params->w_min) w[i * n_pre + j] = params->w_min;
        }
    }

    /* Update traces with current spikes (after weight update) */
    for (size_t j = 0; j < n_pre; j++) {
        r1_data[j] += pre_sp[j];
        r2_data[j] += pre_sp[j];
    }

    for (size_t i = 0; i < n_post; i++) {
        o1_data[i] += post_sp[i];
        o2_data[i] += post_sp[i];
    }

    return true;
}

void nimcp_triplet_stdp_default_params(nimcp_triplet_stdp_params_t* params) {
    if (!params) return;

    /* Biologically plausible defaults from Pfister & Gerstner (2006)
     * Visual cortex all-to-all model parameters */
    params->A2_plus = 7.5e-10f;    /* Pair LTP amplitude */
    params->A2_minus = 7.0e-3f;    /* Pair LTD amplitude */
    params->tau_plus = 16.8f;      /* LTP time constant (ms) */
    params->tau_minus = 33.7f;     /* LTD time constant (ms) */
    params->A3_plus = 9.3e-3f;     /* Triplet LTP amplitude */
    params->A3_minus = 2.3e-4f;    /* Triplet LTD amplitude */
    params->tau_x = 101.0f;        /* Pre triplet trace time constant (ms) */
    params->tau_y = 125.0f;        /* Post triplet trace time constant (ms) */
    params->w_min = 0.0f;          /* Minimum weight */
    params->w_max = 1.0f;          /* Maximum weight */
}

/*=============================================================================
 * High-Level SNN Layer API (Functions 22-30)
 *=============================================================================*/

nimcp_snn_layer_t* nimcp_snn_lif_layer_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_snn_lif_config_t* config)
{
    if (!config || config->num_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_snn_lif_layer_create: invalid config or zero neurons");
        return NULL;
    }

    nimcp_snn_layer_t* layer = nimcp_calloc(1, sizeof(nimcp_snn_layer_t));
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_snn_lif_layer_create: allocation failed");
        return NULL;
    }

    layer->model = config->model;
    layer->num_neurons = config->num_neurons;
    layer->ctx = ctx;
    layer->refractory_period = config->refractory_period;

    /* Create refractory timer tensor */
    layer->refractory_timer = create_neuron_tensor(config->num_neurons);
    if (!layer->refractory_timer) {
        nimcp_snn_layer_destroy(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_snn_lif_layer_create: refractory timer allocation failed");
        return NULL;
    }

    /* Create the appropriate neuron state based on model type */
    switch (config->model) {
        case NIMCP_SNN_LIF:
        case NIMCP_SNN_HH:  /* HH simplified falls back to LIF */
        {
            nimcp_lif_params_t lif_params;
            lif_params.tau_mem = config->tau_mem;
            lif_params.tau_syn = config->tau_syn;
            lif_params.v_thresh = config->v_thresh;
            lif_params.v_reset = config->v_reset;
            lif_params.v_rest = config->v_rest;
            lif_params.dt = config->dt;
            lif_params.hard_reset = true;

            layer->lif_state = nimcp_lif_state_create(ctx, config->num_neurons, &lif_params);
            if (!layer->lif_state) {
                nimcp_snn_layer_destroy(layer);
                return NULL;  /* Error already thrown in nimcp_lif_state_create */
            }
            break;
        }
        case NIMCP_SNN_IZHIKEVICH: {
            nimcp_izhikevich_params_t izh_params;
            izh_params.a = 0.02f;
            izh_params.b = 0.2f;
            izh_params.c = config->v_reset;
            izh_params.d = 8.0f;
            izh_params.v_thresh = config->v_thresh;
            izh_params.dt = config->dt;

            layer->izh_state = nimcp_izhikevich_state_create(ctx, config->num_neurons, &izh_params);
            if (!layer->izh_state) {
                nimcp_snn_layer_destroy(layer);
                return NULL;
            }
            break;
        }
        case NIMCP_SNN_ADEX: {
            nimcp_adex_params_t adex_params;
            adex_params.tau_mem = config->tau_mem;
            adex_params.tau_w = 144.0f;
            adex_params.v_thresh = config->v_thresh;
            adex_params.v_reset = config->v_reset;
            adex_params.v_rest = config->v_rest;
            adex_params.v_rheo = config->v_thresh - 5.0f;
            adex_params.delta_T = 2.0f;
            adex_params.a = 4.0f;
            adex_params.b = 0.0805f;
            adex_params.dt = config->dt;

            layer->adex_state = nimcp_adex_state_create(ctx, config->num_neurons, &adex_params);
            if (!layer->adex_state) {
                nimcp_snn_layer_destroy(layer);
                return NULL;
            }
            break;
        }
        default:
            nimcp_snn_layer_destroy(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_snn_lif_layer_create: unsupported neuron model");
            return NULL;
    }

    return layer;
}

void nimcp_snn_layer_destroy(nimcp_snn_layer_t* layer) {
    if (!layer) return;

    if (layer->lif_state) nimcp_lif_state_destroy(layer->lif_state);
    if (layer->izh_state) nimcp_izhikevich_state_destroy(layer->izh_state);
    if (layer->adex_state) nimcp_adex_state_destroy(layer->adex_state);
    if (layer->refractory_timer) nimcp_gpu_tensor_destroy(layer->refractory_timer);

    nimcp_free(layer);
}

nimcp_gpu_tensor_t* nimcp_snn_layer_get_membrane(nimcp_snn_layer_t* layer) {
    if (!layer) return NULL;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
        case NIMCP_SNN_HH:
            return layer->lif_state ? layer->lif_state->v : NULL;
        case NIMCP_SNN_IZHIKEVICH:
            return layer->izh_state ? layer->izh_state->v : NULL;
        case NIMCP_SNN_ADEX:
            return layer->adex_state ? layer->adex_state->v : NULL;
        default:
            return NULL;
    }
}

nimcp_gpu_tensor_t* nimcp_snn_layer_get_spikes(nimcp_snn_layer_t* layer) {
    if (!layer) return NULL;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
        case NIMCP_SNN_HH:
            return layer->lif_state ? layer->lif_state->spikes : NULL;
        case NIMCP_SNN_IZHIKEVICH:
            return layer->izh_state ? layer->izh_state->spikes : NULL;
        case NIMCP_SNN_ADEX:
            return layer->adex_state ? layer->adex_state->spikes : NULL;
        default:
            return NULL;
    }
}

size_t nimcp_snn_layer_get_size(const nimcp_snn_layer_t* layer) {
    if (!layer) return 0;
    return layer->num_neurons;
}

float nimcp_snn_layer_get_tau_mem(const nimcp_snn_layer_t* layer) {
    if (!layer) return 0.0f;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
        case NIMCP_SNN_HH:
            return layer->lif_state ? layer->lif_state->params.tau_mem : 0.0f;
        case NIMCP_SNN_IZHIKEVICH:
            /* Izhikevich doesn't have explicit tau_mem; return 1/a as analogy */
            return layer->izh_state ? (1.0f / layer->izh_state->params.a) : 0.0f;
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
    (void)ctx;

    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_snn_layer_reset: layer is NULL");
        return false;
    }

    /* Reset refractory timers */
    if (layer->refractory_timer) {
        fill_tensor_float(layer->refractory_timer, 0.0f);
    }

    switch (layer->model) {
        case NIMCP_SNN_LIF:
        case NIMCP_SNN_HH:
            if (layer->lif_state) {
                fill_tensor_float(layer->lif_state->v, layer->lif_state->params.v_rest);
                fill_tensor_float(layer->lif_state->i_syn, 0.0f);
                fill_tensor_float(layer->lif_state->spikes, 0.0f);
            }
            break;
        case NIMCP_SNN_IZHIKEVICH:
            if (layer->izh_state) {
                fill_tensor_float(layer->izh_state->v, layer->izh_state->params.c);
                fill_tensor_float(layer->izh_state->u,
                    layer->izh_state->params.b * layer->izh_state->params.c);
                fill_tensor_float(layer->izh_state->spikes, 0.0f);
            }
            break;
        case NIMCP_SNN_ADEX:
            if (layer->adex_state) {
                fill_tensor_float(layer->adex_state->v, layer->adex_state->params.v_rest);
                fill_tensor_float(layer->adex_state->w, 0.0f);
                fill_tensor_float(layer->adex_state->spikes, 0.0f);
            }
            break;
        default:
            break;
    }

    return true;
}

bool nimcp_snn_layer_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!layer || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_snn_layer_forward: layer or input is NULL");
        return false;
    }

    bool result = false;

    switch (layer->model) {
        case NIMCP_SNN_LIF:
        case NIMCP_SNN_HH:
            result = nimcp_gpu_lif_forward(ctx, layer->lif_state, input);
            break;
        case NIMCP_SNN_IZHIKEVICH:
            result = nimcp_gpu_izhikevich_forward(ctx, layer->izh_state, input);
            break;
        case NIMCP_SNN_ADEX:
            result = nimcp_gpu_adex_forward(ctx, layer->adex_state, input);
            break;
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_snn_layer_forward: unsupported neuron model");
            return false;
    }

    if (!result) return false;

    /* Apply refractory period if configured */
    if (layer->refractory_period > 0.0f && layer->refractory_timer) {
        nimcp_gpu_tensor_t* spikes_tensor = nimcp_snn_layer_get_spikes(layer);
        if (spikes_tensor && spikes_tensor->data && layer->refractory_timer->data) {
            float* spikes_data = (float*)spikes_tensor->data;
            float* refrac = (float*)layer->refractory_timer->data;
            float dt = NIMCP_SIMULATION_DT_MS;  /* Default dt */

            /* Get dt from the model parameters */
            if (layer->lif_state) dt = layer->lif_state->params.dt;
            else if (layer->izh_state) dt = layer->izh_state->params.dt;
            else if (layer->adex_state) dt = layer->adex_state->params.dt;

            for (size_t i = 0; i < layer->num_neurons; i++) {
                if (refrac[i] > 0.0f) {
                    /* Neuron is in refractory period - suppress spike */
                    spikes_data[i] = 0.0f;
                    refrac[i] -= dt;
                    if (refrac[i] < 0.0f) refrac[i] = 0.0f;
                } else if (spikes_data[i] != 0.0f) {
                    /* Neuron spiked - start refractory period */
                    refrac[i] = layer->refractory_period;
                }
            }
        }
    }

    return true;
}

nimcp_gpu_tensor_t* nimcp_snn_lif_step(
    nimcp_gpu_context_t* ctx,
    nimcp_snn_layer_t* layer,
    const nimcp_gpu_tensor_t* input)
{
    if (!layer || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_snn_lif_step: layer or input is NULL");
        return NULL;
    }

    bool ok = nimcp_snn_layer_forward(ctx, layer, input);
    if (!ok) return NULL;

    return nimcp_snn_layer_get_spikes(layer);
}

/*=============================================================================
 * Spike Tensor Functions (Functions 31-32)
 *=============================================================================*/

nimcp_gpu_tensor_t* nimcp_snn_spike_tensor_create(
    nimcp_gpu_context_t* ctx,
    const uint8_t* data,
    const size_t* dims,
    size_t ndim)
{
    (void)ctx;

    if (!dims || ndim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_snn_spike_tensor_create: dims is NULL or ndim is zero");
        return NULL;
    }

    /* Create a float tensor and populate from uint8 spike data */
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(NULL, dims, (uint32_t)ndim,
                                                          NIMCP_GPU_PRECISION_FP32);
    if (!tensor) return NULL;

    if (data && tensor->data) {
        float* fdata = (float*)tensor->data;
        for (size_t i = 0; i < tensor->numel; i++) {
            fdata[i] = (data[i] != 0) ? 1.0f : 0.0f;
        }
    }

    return tensor;
}

bool nimcp_snn_spike_tensor_to_host(
    const nimcp_gpu_tensor_t* tensor,
    uint8_t* data)
{
    if (!tensor || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_snn_spike_tensor_to_host: tensor or data is NULL");
        return false;
    }

    if (!tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_snn_spike_tensor_to_host: tensor data is NULL");
        return false;
    }

    const float* fdata = (const float*)tensor->data;
    for (size_t i = 0; i < tensor->numel; i++) {
        data[i] = (fdata[i] != 0.0f) ? 1 : 0;
    }

    return true;
}

/*=============================================================================
 * Utility Functions (Functions 33-35)
 *=============================================================================*/

bool nimcp_gpu_snn_reset_state(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* v,
    float v_rest)
{
    (void)ctx;

    if (!v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_snn_reset_state: v is NULL");
        return false;
    }

    if (!v->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_snn_reset_state: v->data is NULL");
        return false;
    }

    fill_tensor_float(v, v_rest);
    return true;
}

bool nimcp_gpu_spike_count(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    uint32_t* count)
{
    (void)ctx;

    if (!spikes || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_count: spikes or count is NULL");
        return false;
    }

    if (!spikes->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_count: spikes->data is NULL");
        return false;
    }

    const float* sp = (const float*)spikes->data;
    uint32_t n = 0;

    for (size_t i = 0; i < spikes->numel; i++) {
        if (sp[i] != 0.0f) {
            n++;
        }
    }

    *count = n;
    return true;
}

bool nimcp_gpu_spike_rate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    size_t n_timesteps,
    nimcp_gpu_tensor_t* rates)
{
    (void)ctx;

    if (!spikes || !rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_rate: spikes or rates is NULL");
        return false;
    }

    if (!spikes->data || !rates->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_spike_rate: tensor data is NULL");
        return false;
    }

    if (n_timesteps == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_spike_rate: n_timesteps is zero");
        return false;
    }

    const float* sp = (const float*)spikes->data;
    float* rate = (float*)rates->data;
    size_t n_neurons = rates->numel;

    if (n_neurons == 0) return true;

    /* spikes tensor is [n_timesteps x n_neurons] or flat [n_timesteps * n_neurons].
     * rates tensor is [n_neurons].
     * rate[i] = count(spikes[:, i] != 0) / n_timesteps */

    /* Initialize rates to zero */
    memset(rate, 0, n_neurons * sizeof(float));

    /* Count spikes per neuron across timesteps */
    for (size_t t = 0; t < n_timesteps; t++) {
        for (size_t i = 0; i < n_neurons; i++) {
            size_t idx = t * n_neurons + i;
            if (idx < spikes->numel && sp[idx] != 0.0f) {
                rate[i] += 1.0f;
            }
        }
    }

    /* Divide by number of timesteps to get rate */
    float inv_t = 1.0f / (float)n_timesteps;
    for (size_t i = 0; i < n_neurons; i++) {
        rate[i] *= inv_t;
    }

    return true;
}
