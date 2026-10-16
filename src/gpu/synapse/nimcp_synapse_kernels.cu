/**
 * @file nimcp_synapse_kernels.cu
 * @brief GPU Synapse CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for synaptic transmission, vesicle dynamics, receptor kinetics
 * WHY:  GPU acceleration for large-scale synaptic computation (100K+ synapses)
 * HOW:  Custom kernels for biologically realistic synapse models
 *
 * ARCHITECTURE:
 * - Synaptic transmission: pre->post signal propagation with weight modulation
 * - Vesicle dynamics: Tsodyks-Markram model with three-pool extension
 * - Receptor kinetics: Hill equation binding, desensitization
 * - Short-term plasticity: Facilitation and depression
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
#include <float.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/synapse/nimcp_synapse_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#define LOG_MODULE "SYNAPSE_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Default Parameter Functions
//=============================================================================

nimcp_vesicle_params_t nimcp_gpu_vesicle_default_params(void)
{
    nimcp_vesicle_params_t params;
    params.U = 0.2f;              // Initial release probability
    params.tau_rec = 800.0f;      // Recovery time constant (ms)
    params.tau_facil = 0.0f;      // Facilitation time constant (ms) - 0 = no facilitation
    params.tau_inact = 3.0f;      // Inactivation time constant (ms)
    params.quantal_size = 5000.0f; // Molecules per vesicle
    params.rrp_capacity = 10;     // Readily releasable pool capacity
    params.dt = 0.1f;             // Default timestep (ms)
    return params;
}

nimcp_receptor_params_t nimcp_gpu_receptor_ampa_params(void)
{
    nimcp_receptor_params_t params;
    params.kd = 0.5f;             // Dissociation constant (uM)
    params.hill_coef = 1.0f;      // Hill coefficient (no cooperativity)
    params.tau_rise = 0.2f;       // Rise time constant (ms)
    params.tau_decay = 2.0f;      // Decay time constant (ms)
    params.tau_desens = 15.0f;    // Desensitization time constant (ms)
    params.max_conductance = 0.5f; // Maximum conductance (nS)
    params.reversal = 0.0f;       // Reversal potential (mV)
    return params;
}

nimcp_receptor_params_t nimcp_gpu_receptor_nmda_params(void)
{
    nimcp_receptor_params_t params;
    params.kd = 1.0f;             // Dissociation constant (uM)
    params.hill_coef = 1.5f;      // Hill coefficient (some cooperativity)
    params.tau_rise = 5.0f;       // Rise time constant (ms) - slower than AMPA
    params.tau_decay = 100.0f;    // Decay time constant (ms) - much slower than AMPA
    params.tau_desens = 500.0f;   // Desensitization time constant (ms)
    params.max_conductance = 1.0f; // Maximum conductance (nS)
    params.reversal = 0.0f;       // Reversal potential (mV)
    return params;
}

nimcp_receptor_params_t nimcp_gpu_receptor_gabaa_params(void)
{
    nimcp_receptor_params_t params;
    params.kd = 0.3f;             // Dissociation constant (uM)
    params.hill_coef = 2.0f;      // Hill coefficient (cooperativity)
    params.tau_rise = 0.3f;       // Rise time constant (ms)
    params.tau_decay = 6.0f;      // Decay time constant (ms)
    params.tau_desens = 100.0f;   // Desensitization time constant (ms)
    params.max_conductance = 0.8f; // Maximum conductance (nS)
    params.reversal = -70.0f;     // Reversal potential (mV) - inhibitory
    return params;
}

//=============================================================================
// Vesicle State Lifecycle
//=============================================================================

nimcp_gpu_vesicle_state_t* nimcp_gpu_vesicle_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_vesicle_params_t* params)
{
    if (!ctx || n_synapses == 0) {
        LOG_ERROR("Invalid parameters: ctx=%p, n_synapses=%zu", (void*)ctx, n_synapses);
        return NULL;
    }

    nimcp_gpu_vesicle_state_t* state = (nimcp_gpu_vesicle_state_t*)nimcp_malloc(sizeof(nimcp_gpu_vesicle_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate vesicle state");
        return NULL;
    }

    // Store parameters
    if (params) {
        state->params = *params;
    } else {
        state->params = nimcp_gpu_vesicle_default_params();
    }

    // Create GPU tensors for state variables
    size_t dims[] = {n_synapses};

    state->u = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->x = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->y = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->z = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->u || !state->x || !state->y || !state->z) {
        LOG_ERROR("Failed to create vesicle state tensors");
        nimcp_gpu_vesicle_state_destroy(state);
        return NULL;
    }

    // Initialize state: u=U, x=1, y=0, z=0
    nimcp_gpu_fill(ctx, state->u, state->params.U);
    nimcp_gpu_fill(ctx, state->x, 1.0f);
    nimcp_gpu_fill(ctx, state->y, 0.0f);
    nimcp_gpu_fill(ctx, state->z, 0.0f);

    LOG_DEBUG("Created vesicle state for %zu synapses", n_synapses);
    return state;
}

void nimcp_gpu_vesicle_state_destroy(nimcp_gpu_vesicle_state_t* state)
{
    if (!state) return;

    nimcp_gpu_tensor_destroy(state->u);
    nimcp_gpu_tensor_destroy(state->x);
    nimcp_gpu_tensor_destroy(state->y);
    nimcp_gpu_tensor_destroy(state->z);
    nimcp_free(state);
}

//=============================================================================
// Receptor State Lifecycle
//=============================================================================

nimcp_gpu_receptor_state_t* nimcp_gpu_receptor_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_receptor_params_t* params)
{
    if (!ctx || n_synapses == 0) {
        LOG_ERROR("Invalid parameters: ctx=%p, n_synapses=%zu", (void*)ctx, n_synapses);
        return NULL;
    }

    nimcp_gpu_receptor_state_t* state = (nimcp_gpu_receptor_state_t*)nimcp_malloc(sizeof(nimcp_gpu_receptor_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate receptor state");
        return NULL;
    }

    // Store parameters
    if (params) {
        state->params = *params;
    } else {
        state->params = nimcp_gpu_receptor_ampa_params();
    }

    // Create GPU tensors
    size_t dims[] = {n_synapses};

    state->occupancy = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->desensitization = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->conductance = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->occupancy || !state->desensitization || !state->conductance) {
        LOG_ERROR("Failed to create receptor state tensors");
        nimcp_gpu_receptor_state_destroy(state);
        return NULL;
    }

    // Initialize to zero
    nimcp_gpu_fill(ctx, state->occupancy, 0.0f);
    nimcp_gpu_fill(ctx, state->desensitization, 0.0f);
    nimcp_gpu_fill(ctx, state->conductance, 0.0f);

    LOG_DEBUG("Created receptor state for %zu synapses", n_synapses);
    return state;
}

void nimcp_gpu_receptor_state_destroy(nimcp_gpu_receptor_state_t* state)
{
    if (!state) return;

    nimcp_gpu_tensor_destroy(state->occupancy);
    nimcp_gpu_tensor_destroy(state->desensitization);
    nimcp_gpu_tensor_destroy(state->conductance);
    nimcp_free(state);
}

//=============================================================================
// Synapse State Lifecycle
//=============================================================================

nimcp_gpu_synapse_state_t* nimcp_gpu_synapse_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    size_t n_pre,
    size_t n_post,
    nimcp_synapse_model_t model)
{
    if (!ctx || n_synapses == 0 || n_pre == 0 || n_post == 0) {
        LOG_ERROR("Invalid parameters: ctx=%p, n_syn=%zu, n_pre=%zu, n_post=%zu",
                  (void*)ctx, n_synapses, n_pre, n_post);
        return NULL;
    }

    nimcp_gpu_synapse_state_t* state = (nimcp_gpu_synapse_state_t*)nimcp_malloc(sizeof(nimcp_gpu_synapse_state_t));
    if (!state) {
        LOG_ERROR("Failed to allocate synapse state");
        return NULL;
    }

    // Initialize all pointers to NULL for safe cleanup
    state->pre_ids = NULL;
    state->post_ids = NULL;
    state->weights = NULL;
    state->delays = NULL;
    state->vesicle_state = NULL;
    state->receptor_state = NULL;
    state->psc = NULL;
    state->transmission = NULL;

    // Store metadata
    state->n_synapses = n_synapses;
    state->n_pre = n_pre;
    state->n_post = n_post;
    state->model = model;

    // Create connectivity tensors
    size_t syn_dims[] = {n_synapses};
    size_t post_dims[] = {n_post};

    state->pre_ids = nimcp_gpu_tensor_create(ctx, syn_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    state->post_ids = nimcp_gpu_tensor_create(ctx, syn_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    state->weights = nimcp_gpu_tensor_create(ctx, syn_dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->transmission = nimcp_gpu_tensor_create(ctx, syn_dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->psc = nimcp_gpu_tensor_create(ctx, post_dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->pre_ids || !state->post_ids || !state->weights ||
        !state->transmission || !state->psc) {
        LOG_ERROR("Failed to create synapse connectivity tensors");
        nimcp_gpu_synapse_state_destroy(state);
        return NULL;
    }

    // Initialize weights to small random values (will be set by user)
    nimcp_gpu_fill(ctx, state->weights, 0.1f);
    nimcp_gpu_fill(ctx, state->transmission, 0.0f);
    nimcp_gpu_fill(ctx, state->psc, 0.0f);

    // Create STP state for models that use it
    if (model == NIMCP_SYNAPSE_STP || model == NIMCP_SYNAPSE_NMDA) {
        state->vesicle_state = nimcp_gpu_vesicle_state_create(ctx, n_synapses, NULL);
        if (!state->vesicle_state) {
            LOG_ERROR("Failed to create vesicle state");
            nimcp_gpu_synapse_state_destroy(state);
            return NULL;
        }
    }

    // Create receptor state for conductance-based models
    if (model == NIMCP_SYNAPSE_CONDUCTANCE || model == NIMCP_SYNAPSE_NMDA ||
        model == NIMCP_SYNAPSE_AMPA || model == NIMCP_SYNAPSE_GABA_A ||
        model == NIMCP_SYNAPSE_GABA_B) {

        nimcp_receptor_params_t params;
        switch (model) {
            case NIMCP_SYNAPSE_NMDA:
                params = nimcp_gpu_receptor_nmda_params();
                break;
            case NIMCP_SYNAPSE_GABA_A:
            case NIMCP_SYNAPSE_GABA_B:
                params = nimcp_gpu_receptor_gabaa_params();
                break;
            default:
                params = nimcp_gpu_receptor_ampa_params();
                break;
        }

        state->receptor_state = nimcp_gpu_receptor_state_create(ctx, n_synapses, &params);
        if (!state->receptor_state) {
            LOG_ERROR("Failed to create receptor state");
            nimcp_gpu_synapse_state_destroy(state);
            return NULL;
        }
    }

    LOG_DEBUG("Created synapse state: %zu synapses, model=%d", n_synapses, model);
    return state;
}

void nimcp_gpu_synapse_state_destroy(nimcp_gpu_synapse_state_t* state)
{
    if (!state) return;

    nimcp_gpu_tensor_destroy(state->pre_ids);
    nimcp_gpu_tensor_destroy(state->post_ids);
    nimcp_gpu_tensor_destroy(state->weights);
    nimcp_gpu_tensor_destroy(state->delays);
    nimcp_gpu_tensor_destroy(state->psc);
    nimcp_gpu_tensor_destroy(state->transmission);

    nimcp_gpu_vesicle_state_destroy(state->vesicle_state);
    nimcp_gpu_receptor_state_destroy(state->receptor_state);

    nimcp_free(state);
}

//=============================================================================
// Synaptic Transmission Kernels
//=============================================================================

/**
 * @brief Kernel for simple synaptic transmission
 *
 * transmission[i] = weight[i] * pre_activity[pre_id[i]]
 */
__global__ void kernel_synapse_transmit_simple(
    const float* weights,
    const uint32_t* pre_ids,
    const float* pre_activity,
    float* transmission,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    uint32_t pre_id = pre_ids[idx];
    transmission[idx] = weights[idx] * pre_activity[pre_id];
}

/**
 * @brief Kernel for STP-modulated synaptic transmission
 *
 * transmission[i] = weight[i] * pre_activity[pre_id[i]] * u[i] * x[i]
 */
__global__ void kernel_synapse_transmit_stp(
    const float* weights,
    const uint32_t* pre_ids,
    const float* pre_activity,
    const float* u,
    const float* x,
    float* transmission,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    uint32_t pre_id = pre_ids[idx];
    float stp_factor = u[idx] * x[idx];
    transmission[idx] = weights[idx] * pre_activity[pre_id] * stp_factor;
}

bool nimcp_gpu_synapse_transmit(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !pre_activity) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    // Guard: Validate internal tensor fields
    if (!state->weights || !state->pre_ids || !state->transmission) {
        LOG_ERROR("Synapse state not properly initialized");
        return false;
    }

    // Guard: Validate pre_activity has elements
    if (pre_activity->numel == 0) {
        LOG_ERROR("pre_activity tensor has zero elements");
        return false;
    }

    // Guard: Nothing to do if no synapses
    if (state->n_synapses == 0) {
        return true;
    }

    // NOTE: pre_ids values must be < pre_activity->numel for safe indexing
    // Caller must ensure synapse connectivity is valid for given pre_activity size

    if (state->model == NIMCP_SYNAPSE_STP && state->vesicle_state) {
        // STP-modulated transmission
        kernel_synapse_transmit_stp<<<GRID_SIZE(state->n_synapses), BLOCK_SIZE>>>(
            (const float*)state->weights->data,
            (const uint32_t*)state->pre_ids->data,
            (const float*)pre_activity->data,
            (const float*)state->vesicle_state->u->data,
            (const float*)state->vesicle_state->x->data,
            (float*)state->transmission->data,
            state->n_synapses);
    } else {
        // Simple transmission
        kernel_synapse_transmit_simple<<<GRID_SIZE(state->n_synapses), BLOCK_SIZE>>>(
            (const float*)state->weights->data,
            (const uint32_t*)state->pre_ids->data,
            (const float*)pre_activity->data,
            (float*)state->transmission->data,
            state->n_synapses);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to accumulate synaptic transmission to post-synaptic currents
 *
 * Uses atomicAdd for scatter-add operation
 */
__global__ void kernel_synapse_accumulate_psc(
    const float* transmission,
    const uint32_t* post_ids,
    float* psc,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    uint32_t post_id = post_ids[idx];
    atomicAdd(&psc[post_id], transmission[idx]);
}

bool nimcp_gpu_synapse_accumulate_psc(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    // Guard: Validate internal tensor fields
    if (!state->transmission || !state->post_ids || !state->psc) {
        LOG_ERROR("Synapse state not properly initialized");
        return false;
    }

    // Zero out PSC before accumulation
    nimcp_gpu_fill(ctx, state->psc, 0.0f);

    kernel_synapse_accumulate_psc<<<GRID_SIZE(state->n_synapses), BLOCK_SIZE>>>(
        (const float*)state->transmission->data,
        (const uint32_t*)state->post_ids->data,
        (float*)state->psc->data,
        state->n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_synapse_forward(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !pre_activity) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    // Transmit
    if (!nimcp_gpu_synapse_transmit(ctx, state, pre_activity)) {
        return false;
    }

    // Accumulate
    if (!nimcp_gpu_synapse_accumulate_psc(ctx, state)) {
        return false;
    }

    return true;
}

//=============================================================================
// Vesicle Dynamics Kernels (Tsodyks-Markram Model)
//=============================================================================

/**
 * @brief Kernel for vesicle release probability update with facilitation
 *
 * On spike: u(t+dt) = U + u(t)*(1-U)
 * Between spikes: u decays toward U with tau_facil
 */
__global__ void kernel_vesicle_update_release_prob(
    float* u,
    const float* pre_spikes,
    float U,
    float tau_facil,
    float dt,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float spike = pre_spikes[idx];
    float u_val = u[idx];

    if (tau_facil > 0.0f) {
        // Facilitation: u increases on spike, decays between spikes
        float decay = expf(-dt / tau_facil);
        u_val = U + (u_val - U) * decay;

        // On spike, facilitation
        if (spike > 0.5f) {
            u_val = u_val + U * (1.0f - u_val);
        }
    } else {
        // No facilitation: u = U on spike
        if (spike > 0.5f) {
            u_val = U;
        }
    }

    u[idx] = u_val;
}

bool nimcp_gpu_vesicle_update_release_prob(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state,
    const nimcp_gpu_tensor_t* pre_spikes)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !pre_spikes) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n_synapses = pre_spikes->numel;

    kernel_vesicle_update_release_prob<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (float*)state->u->data,
        (const float*)pre_spikes->data,
        state->params.U,
        state->params.tau_facil,
        state->params.dt,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for vesicle pool dynamics (Tsodyks-Markram)
 *
 * dx/dt = z/tau_rec - u*x*spike
 * dy/dt = -y/tau_inact + u*x*spike
 * dz/dt = y/tau_inact - z/tau_rec
 */
__global__ void kernel_vesicle_release(
    float* x,
    float* y,
    float* z,
    const float* u,
    const float* pre_spikes,
    float tau_rec,
    float tau_inact,
    float dt,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float x_val = x[idx];
    float y_val = y[idx];
    float z_val = z[idx];
    float u_val = u[idx];
    float spike = pre_spikes[idx];

    // Compute derivatives
    float dx = z_val / tau_rec;
    float dy = -y_val / tau_inact;
    float dz = y_val / tau_inact - z_val / tau_rec;

    // Apply spike-triggered release
    if (spike > 0.5f) {
        float release = u_val * x_val;
        dx -= release / dt;  // Instantaneous release
        dy += release / dt;
    }

    // Forward Euler integration
    x_val += dx * dt;
    y_val += dy * dt;
    z_val += dz * dt;

    // Clamp to valid range [0, 1]
    x_val = fmaxf(0.0f, fminf(1.0f, x_val));
    y_val = fmaxf(0.0f, fminf(1.0f, y_val));
    z_val = fmaxf(0.0f, fminf(1.0f, z_val));

    x[idx] = x_val;
    y[idx] = y_val;
    z[idx] = z_val;
}

bool nimcp_gpu_vesicle_release(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state,
    const nimcp_gpu_tensor_t* pre_spikes,
    float dt)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !pre_spikes) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n_synapses = pre_spikes->numel;

    // First update release probability
    if (!nimcp_gpu_vesicle_update_release_prob(ctx, state, pre_spikes)) {
        return false;
    }

    // Then update pool dynamics
    kernel_vesicle_release<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (float*)state->x->data,
        (float*)state->y->data,
        (float*)state->z->data,
        (const float*)state->u->data,
        (const float*)pre_spikes->data,
        state->params.tau_rec,
        state->params.tau_inact,
        dt,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute STP efficacy
 *
 * efficacy = u * x
 */
__global__ void kernel_vesicle_get_efficacy(
    const float* u,
    const float* x,
    float* efficacy,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    efficacy[idx] = u[idx] * x[idx];
}

bool nimcp_gpu_vesicle_get_efficacy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_vesicle_state_t* state,
    nimcp_gpu_tensor_t* efficacy)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !efficacy) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n_synapses = state->u->numel;

    kernel_vesicle_get_efficacy<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (const float*)state->u->data,
        (const float*)state->x->data,
        (float*)efficacy->data,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Receptor Kinetics Kernels
//=============================================================================

/**
 * @brief Device function for Hill equation
 *
 * occupancy = [NT]^n / (Kd^n + [NT]^n)
 */
__device__ inline float hill_equation(float concentration, float kd, float hill_coef)
{
    float cn = powf(concentration, hill_coef);
    float kn = powf(kd, hill_coef);
    return cn / (kn + cn + 1e-10f);  // Small epsilon to avoid division by zero
}

/**
 * @brief Kernel for receptor binding update
 *
 * Uses Hill equation with temporal filtering
 */
__global__ void kernel_receptor_update_binding(
    float* occupancy,
    const float* concentration,
    float kd,
    float hill_coef,
    float tau_rise,
    float tau_decay,
    float dt,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float conc = concentration[idx];
    float occ = occupancy[idx];

    // Target occupancy from Hill equation
    float target = hill_equation(conc, kd, hill_coef);

    // Two-exponential dynamics: fast rise, slow decay
    float tau = (target > occ) ? tau_rise : tau_decay;
    float alpha = 1.0f - expf(-dt / tau);

    // Update occupancy toward target
    occupancy[idx] = occ + alpha * (target - occ);
}

bool nimcp_gpu_receptor_update_binding(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state,
    const nimcp_gpu_tensor_t* concentration,
    float dt)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !concentration) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n_synapses = concentration->numel;

    kernel_receptor_update_binding<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (float*)state->occupancy->data,
        (const float*)concentration->data,
        state->params.kd,
        state->params.hill_coef,
        state->params.tau_rise,
        state->params.tau_decay,
        dt,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for receptor desensitization update
 *
 * d_desens/dt = (occupancy - desens) / tau_desens
 */
__global__ void kernel_receptor_update_desensitization(
    float* desensitization,
    const float* occupancy,
    float tau_desens,
    float dt,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float desens = desensitization[idx];
    float occ = occupancy[idx];

    // Exponential approach toward occupancy
    float alpha = 1.0f - expf(-dt / tau_desens);
    desensitization[idx] = desens + alpha * (occ - desens);
}

bool nimcp_gpu_receptor_update_desensitization(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state,
    float dt)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n_synapses = state->occupancy->numel;

    kernel_receptor_update_desensitization<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (float*)state->desensitization->data,
        (const float*)state->occupancy->data,
        state->params.tau_desens,
        dt,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for receptor conductance computation
 *
 * g = g_max * occupancy * (1 - desensitization)
 */
__global__ void kernel_receptor_compute_conductance(
    float* conductance,
    const float* occupancy,
    const float* desensitization,
    float max_conductance,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float occ = occupancy[idx];
    float desens = desensitization[idx];

    conductance[idx] = max_conductance * occ * (1.0f - desens);
}

bool nimcp_gpu_receptor_compute_conductance(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n_synapses = state->occupancy->numel;

    kernel_receptor_compute_conductance<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (float*)state->conductance->data,
        (const float*)state->occupancy->data,
        (const float*)state->desensitization->data,
        state->params.max_conductance,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Simple kernel for synaptic current computation without post_ids mapping
 *
 * I = g * (V - E_rev)
 *
 * Uses direct indexing: synapse i uses voltage at index i % n_post
 */
__global__ void kernel_receptor_compute_current_simple(
    const float* conductance,
    const float* voltage,
    float reversal,
    float* current,
    size_t n_synapses,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float g = conductance[idx];
    float v = voltage[idx % n_post];

    current[idx] = g * (v - reversal);
}

/**
 * @brief Kernel for synaptic current computation
 *
 * I = g * (V_post - E_rev)
 *
 * Note: This kernel needs post_ids to map synapses to post-synaptic neurons
 */
__global__ void kernel_receptor_compute_current(
    const float* conductance,
    const float* post_voltage,
    const uint32_t* post_ids,
    float reversal,
    float* current,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    uint32_t post_id = post_ids[idx];
    float g = conductance[idx];
    float v = post_voltage[post_id];

    current[idx] = g * (v - reversal);
}

bool nimcp_gpu_receptor_compute_current(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_receptor_state_t* state,
    const nimcp_gpu_tensor_t* post_voltage,
    nimcp_gpu_tensor_t* current)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !post_voltage || !current) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    if (!state->conductance) {
        LOG_ERROR("Receptor state has no conductance tensor");
        return false;
    }

    size_t n_synapses = state->conductance->numel;
    size_t n_post = post_voltage->numel;

    // Guard: Avoid division by zero in kernel (idx % n_post)
    if (n_post == 0) {
        LOG_ERROR("post_voltage tensor has zero elements");
        return false;
    }

    // Guard: Nothing to compute if no synapses
    if (n_synapses == 0) {
        return true;
    }

    // Use simple kernel with direct indexing (synapse i uses voltage[i % n_post])
    kernel_receptor_compute_current_simple<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (const float*)state->conductance->data,
        (const float*)post_voltage->data,
        state->params.reversal,
        (float*)current->data,
        n_synapses,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// NMDA Voltage-Dependent Block
//=============================================================================

/**
 * @brief Kernel for NMDA Mg2+ block computation
 *
 * block = 1 / (1 + [Mg2+]/3.57 * exp(-0.062 * V))
 *
 * Based on Jahr & Stevens (1990) model
 */
__global__ void kernel_nmda_mg_block(
    const float* post_voltage,
    float* mg_block,
    float mg_concentration,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float v = post_voltage[idx];

    // Jahr-Stevens model for Mg2+ block
    // block factor = 1 / (1 + [Mg]/3.57 * exp(-0.062 * V))
    float mg_factor = mg_concentration / 3.57f;
    float voltage_factor = expf(-0.062f * v);

    mg_block[idx] = 1.0f / (1.0f + mg_factor * voltage_factor);
}

bool nimcp_gpu_nmda_mg_block(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* post_voltage,
    nimcp_gpu_tensor_t* mg_block,
    float mg_concentration)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !post_voltage || !mg_block) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    if (mg_concentration <= 0.0f) {
        mg_concentration = 1.0f;  // Default extracellular Mg2+ (mM)
    }

    size_t n = post_voltage->numel;

    kernel_nmda_mg_block<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)post_voltage->data,
        (float*)mg_block->data,
        mg_concentration,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Neurotransmitter Diffusion
//=============================================================================

/**
 * @brief Kernel for neurotransmitter diffusion
 *
 * Bi-exponential model: c(t) = A * (exp(-t/tau_d) - exp(-t/tau_r))
 *
 * Implemented as two ODE variables for numerical stability:
 * dc_rise/dt = -c_rise/tau_r + release
 * dc_decay/dt = -c_decay/tau_d + release
 * concentration = c_decay - c_rise (with proper normalization)
 */
__global__ void kernel_neurotransmitter_diffusion(
    const float* release,
    float* concentration,
    float tau_rise,
    float tau_decay,
    float dt,
    size_t n_synapses)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_synapses) return;

    float rel = release[idx];
    float conc = concentration[idx];

    // Simplified bi-exponential: update concentration
    // This is a single-variable approximation
    float decay_factor = expf(-dt / tau_decay);
    float rise_factor = expf(-dt / tau_rise);

    // Decay existing concentration
    conc *= decay_factor;

    // Add new release with rise dynamics
    // Peak amplitude normalization for bi-exponential
    float norm = tau_decay / (tau_decay - tau_rise + 1e-6f);
    conc += rel * norm * (1.0f - rise_factor);

    concentration[idx] = conc;
}

bool nimcp_gpu_neurotransmitter_diffusion(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* release,
    nimcp_gpu_tensor_t* concentration,
    float tau_rise,
    float tau_decay,
    float dt)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !release || !concentration) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    if (tau_rise <= 0.0f) tau_rise = 0.2f;
    if (tau_decay <= 0.0f) tau_decay = 2.0f;
    if (dt <= 0.0f) dt = 0.1f;

    size_t n_synapses = release->numel;

    kernel_neurotransmitter_diffusion<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        (const float*)release->data,
        (float*)concentration->data,
        tau_rise,
        tau_decay,
        dt,
        n_synapses);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

bool nimcp_gpu_synapse_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state)
{
    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    // Reset transmission and PSC
    nimcp_gpu_fill(ctx, state->transmission, 0.0f);
    nimcp_gpu_fill(ctx, state->psc, 0.0f);

    // Reset vesicle state if present
    if (state->vesicle_state) {
        if (!nimcp_gpu_vesicle_reset(ctx, state->vesicle_state)) {
            return false;
        }
    }

    // Reset receptor state if present
    if (state->receptor_state) {
        if (!nimcp_gpu_receptor_reset(ctx, state->receptor_state)) {
            return false;
        }
    }

    return true;
}

bool nimcp_gpu_vesicle_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state)
{
    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    nimcp_gpu_fill(ctx, state->u, state->params.U);
    nimcp_gpu_fill(ctx, state->x, 1.0f);
    nimcp_gpu_fill(ctx, state->y, 0.0f);
    nimcp_gpu_fill(ctx, state->z, 0.0f);

    return true;
}

bool nimcp_gpu_receptor_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state)
{
    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    nimcp_gpu_fill(ctx, state->occupancy, 0.0f);
    nimcp_gpu_fill(ctx, state->desensitization, 0.0f);
    nimcp_gpu_fill(ctx, state->conductance, 0.0f);

    return true;
}

/**
 * @brief Kernel for computing sum using parallel reduction
 */
static __global__ void kernel_synapse_reduce_sum(const float* data, float* result, size_t n)
{
    extern __shared__ float sdata[];

    size_t tid = threadIdx.x;
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Load data into shared memory
    sdata[tid] = (idx < n) ? data[idx] : 0.0f;
    __syncthreads();

    // Parallel reduction for sum
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    // Atomic add from first thread of each block
    if (tid == 0) {
        atomicAdd(result, sdata[0]);
    }
}

bool nimcp_gpu_synapse_get_stats(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_synapse_state_t* state,
    float* mean_weight,
    float* mean_transmission,
    float* mean_efficacy)
{
    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters");
        return false;
    }

    size_t n = state->n_synapses;

    // Guard: Avoid division by zero
    if (n == 0) {
        if (mean_weight) *mean_weight = 0.0f;
        if (mean_transmission) *mean_transmission = 0.0f;
        if (mean_efficacy) *mean_efficacy = 1.0f;
        return true;
    }

    float* d_sum = NULL;
    float h_sum;
    bool success = false;
    cudaError_t err;

    err = cudaMalloc(&d_sum, sizeof(float));
    if (err != cudaSuccess) {
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err));
        return false;
    }

    // Compute mean weight
    if (mean_weight) {
        err = cudaMemset(d_sum, 0, sizeof(float));
        if (err != cudaSuccess) goto cleanup;
        kernel_synapse_reduce_sum<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
            (const float*)state->weights->data, d_sum, n);
        err = cudaGetLastError();
        if (err != cudaSuccess) goto cleanup;
        err = cudaMemcpy(&h_sum, d_sum, sizeof(float), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) goto cleanup;
        *mean_weight = h_sum / (float)n;
    }

    // Compute mean transmission
    if (mean_transmission) {
        err = cudaMemset(d_sum, 0, sizeof(float));
        if (err != cudaSuccess) goto cleanup;
        kernel_synapse_reduce_sum<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
            (const float*)state->transmission->data, d_sum, n);
        err = cudaGetLastError();
        if (err != cudaSuccess) goto cleanup;
        err = cudaMemcpy(&h_sum, d_sum, sizeof(float), cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) goto cleanup;
        *mean_transmission = h_sum / (float)n;
    }

    // Compute mean efficacy if vesicle state exists
    if (mean_efficacy && state->vesicle_state) {
        // Need temporary buffer for efficacy
        size_t dims[] = {n};
        nimcp_gpu_tensor_t* efficacy = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (efficacy) {
            nimcp_gpu_vesicle_get_efficacy(ctx, state->vesicle_state, efficacy);

            err = cudaMemset(d_sum, 0, sizeof(float));
            if (err != cudaSuccess) {
                nimcp_gpu_tensor_destroy(efficacy);
                goto cleanup;
            }
            kernel_synapse_reduce_sum<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
                (const float*)efficacy->data, d_sum, n);
            err = cudaGetLastError();
            if (err != cudaSuccess) {
                nimcp_gpu_tensor_destroy(efficacy);
                goto cleanup;
            }
            err = cudaMemcpy(&h_sum, d_sum, sizeof(float), cudaMemcpyDeviceToHost);
            if (err != cudaSuccess) {
                nimcp_gpu_tensor_destroy(efficacy);
                goto cleanup;
            }
            *mean_efficacy = h_sum / (float)n;

            nimcp_gpu_tensor_destroy(efficacy);
        } else {
            *mean_efficacy = 0.0f;
        }
    } else if (mean_efficacy) {
        *mean_efficacy = 1.0f;  // Default efficacy when no STP
    }

    success = true;

cleanup:
    if (d_sum) cudaFree(d_sum);
    if (!success) {
        LOG_ERROR("CUDA error: %s", cudaGetErrorString(err));
    }
    return success;
}

#else // !NIMCP_ENABLE_CUDA

// Stub implementations when CUDA is not available

#include "gpu/synapse/nimcp_synapse_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>

#define LOG_MODULE "SYNAPSE_GPU"

nimcp_vesicle_params_t nimcp_gpu_vesicle_default_params(void)
{
    nimcp_vesicle_params_t params = {0};
    params.U = 0.2f;
    params.tau_rec = 800.0f;
    params.tau_inact = 3.0f;
    params.quantal_size = 5000.0f;
    params.rrp_capacity = 10;
    params.dt = 0.1f;
    return params;
}

nimcp_receptor_params_t nimcp_gpu_receptor_ampa_params(void)
{
    nimcp_receptor_params_t params = {0};
    params.kd = 0.5f;
    params.hill_coef = 1.0f;
    params.tau_rise = 0.2f;
    params.tau_decay = 2.0f;
    params.tau_desens = 15.0f;
    params.max_conductance = 0.5f;
    params.reversal = 0.0f;
    return params;
}

nimcp_receptor_params_t nimcp_gpu_receptor_nmda_params(void)
{
    nimcp_receptor_params_t params = {0};
    params.kd = 1.0f;
    params.hill_coef = 1.5f;
    params.tau_rise = 5.0f;
    params.tau_decay = 100.0f;
    params.tau_desens = 500.0f;
    params.max_conductance = 1.0f;
    params.reversal = 0.0f;
    return params;
}

nimcp_receptor_params_t nimcp_gpu_receptor_gabaa_params(void)
{
    nimcp_receptor_params_t params = {0};
    params.kd = 0.3f;
    params.hill_coef = 2.0f;
    params.tau_rise = 0.3f;
    params.tau_decay = 6.0f;
    params.tau_desens = 100.0f;
    params.max_conductance = 0.8f;
    params.reversal = -70.0f;
    return params;
}

nimcp_gpu_vesicle_state_t* nimcp_gpu_vesicle_state_create(
    nimcp_gpu_context_t* ctx, size_t n_synapses, const nimcp_vesicle_params_t* params)
{
    LOG_WARN("CUDA not enabled - vesicle state creation unavailable");
    return NULL;
}

void nimcp_gpu_vesicle_state_destroy(nimcp_gpu_vesicle_state_t* state) {}

nimcp_gpu_receptor_state_t* nimcp_gpu_receptor_state_create(
    nimcp_gpu_context_t* ctx, size_t n_synapses, const nimcp_receptor_params_t* params)
{
    LOG_WARN("CUDA not enabled - receptor state creation unavailable");
    return NULL;
}

void nimcp_gpu_receptor_state_destroy(nimcp_gpu_receptor_state_t* state) {}

nimcp_gpu_synapse_state_t* nimcp_gpu_synapse_state_create(
    nimcp_gpu_context_t* ctx, size_t n_synapses, size_t n_pre, size_t n_post,
    nimcp_synapse_model_t model)
{
    LOG_WARN("CUDA not enabled - synapse state creation unavailable");
    return NULL;
}

void nimcp_gpu_synapse_state_destroy(nimcp_gpu_synapse_state_t* state) {}

bool nimcp_gpu_synapse_transmit(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state, const nimcp_gpu_tensor_t* pre_activity)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_synapse_accumulate_psc(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_synapse_forward(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state, const nimcp_gpu_tensor_t* pre_activity)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_vesicle_update_release_prob(nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state, const nimcp_gpu_tensor_t* pre_spikes)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_vesicle_release(nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state, const nimcp_gpu_tensor_t* pre_spikes, float dt)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_vesicle_get_efficacy(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_vesicle_state_t* state, nimcp_gpu_tensor_t* efficacy)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_receptor_update_binding(nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state, const nimcp_gpu_tensor_t* concentration, float dt)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_receptor_update_desensitization(nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state, float dt)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_receptor_compute_conductance(nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_receptor_compute_current(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_receptor_state_t* state, const nimcp_gpu_tensor_t* post_voltage,
    nimcp_gpu_tensor_t* current)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_nmda_mg_block(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* post_voltage, nimcp_gpu_tensor_t* mg_block,
    float mg_concentration)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_neurotransmitter_diffusion(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* release, nimcp_gpu_tensor_t* concentration,
    float tau_rise, float tau_decay, float dt)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_synapse_reset(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synapse_state_t* state)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_vesicle_reset(nimcp_gpu_context_t* ctx,
    nimcp_gpu_vesicle_state_t* state)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_receptor_reset(nimcp_gpu_context_t* ctx,
    nimcp_gpu_receptor_state_t* state)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

bool nimcp_gpu_synapse_get_stats(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_synapse_state_t* state, float* mean_weight,
    float* mean_transmission, float* mean_efficacy)
{
    LOG_WARN("CUDA not enabled");
    return false;
}

#endif // NIMCP_ENABLE_CUDA
