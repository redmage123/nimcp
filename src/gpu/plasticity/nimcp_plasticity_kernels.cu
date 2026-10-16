/**
 * @file nimcp_plasticity_kernels.cu
 * @brief GPU Plasticity CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for synaptic plasticity computations
 * WHY:  GPU acceleration for performance-critical plasticity rules
 * HOW:  Custom kernels for STDP, BCM, Homeostatic, STP, Calcium dynamics
 *
 * ARCHITECTURE:
 * - STDP: Spike-Timing-Dependent Plasticity (pair and triplet)
 * - BCM: Bienenstock-Cooper-Munro sliding threshold
 * - Homeostatic: Synaptic scaling and intrinsic plasticity
 * - STP: Short-Term Plasticity (depression/facilitation)
 * - Calcium: Calcium-dependent learning rate dynamics
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
#include "gpu/plasticity/nimcp_plasticity_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "PLASTICITY_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_stdp_params_t nimcp_gpu_stdp_params_default(void)
{
    nimcp_gpu_stdp_params_t params;
    params.A_plus = 0.01f;       // LTP amplitude
    params.A_minus = 0.012f;     // LTD amplitude (slightly stronger for balance)
    params.tau_plus = 20.0f;     // LTP time constant (ms)
    params.tau_minus = 20.0f;    // LTD time constant (ms)
    params.w_max = 1.0f;         // Maximum weight
    params.w_min = 0.0f;         // Minimum weight
    params.da_mod_gain = 1.0f;   // Dopamine modulation gain
    params.burst_amp = 1.5f;     // Burst amplification factor
    params.soft_bounds = true;   // Use soft bounds
    return params;
}

nimcp_gpu_triplet_stdp_params_t nimcp_gpu_triplet_stdp_params_default(void)
{
    // Pfister & Gerstner 2006 visual cortex parameters
    nimcp_gpu_triplet_stdp_params_t params;
    params.A2_plus = 5e-10f;     // Pairwise LTP
    params.A3_plus = 6.2e-3f;    // Triplet LTP
    params.A2_minus = 7e-3f;     // Pairwise LTD
    params.A3_minus = 2.3e-4f;   // Triplet LTD
    params.tau_plus = 16.8f;     // Fast pre-trace (ms)
    params.tau_minus = 33.7f;    // Fast post-trace (ms)
    params.tau_x = 101.0f;       // Slow pre-trace (ms)
    params.tau_y = 125.0f;       // Slow post-trace (ms)
    params.w_max = 1.0f;
    params.w_min = 0.0f;
    return params;
}

nimcp_gpu_bcm_params_t nimcp_gpu_bcm_params_default(void)
{
    nimcp_gpu_bcm_params_t params;
    params.learning_rate = 0.001f;
    params.threshold_tau = 1000.0f;   // Slow threshold adaptation
    params.activity_tau = 100.0f;     // Activity averaging
    params.min_threshold = 0.01f;
    params.max_threshold = 10.0f;
    params.theta_power = 2.0f;        // Quadratic threshold
    return params;
}

nimcp_gpu_scaling_params_t nimcp_gpu_scaling_params_default(void)
{
    nimcp_gpu_scaling_params_t params;
    params.target_rate = 10.0f;       // Target 10 Hz
    params.scaling_tau = 10000.0f;    // Very slow scaling (ms)
    params.scaling_exponent = 1.0f;   // Linear scaling
    params.min_scale = 0.1f;
    params.max_scale = 10.0f;
    params.rate_tau = 1000.0f;        // Rate averaging
    return params;
}

nimcp_gpu_intrinsic_params_t nimcp_gpu_intrinsic_params_default(void)
{
    nimcp_gpu_intrinsic_params_t params;
    params.target_rate = 10.0f;
    params.threshold_tau = 5000.0f;
    params.gain_tau = 5000.0f;
    params.min_threshold = -100.0f;
    params.max_threshold = 100.0f;
    params.min_gain = 0.1f;
    params.max_gain = 10.0f;
    params.learning_rate = 0.0001f;
    return params;
}

nimcp_gpu_stp_params_t nimcp_gpu_stp_params_default(void)
{
    // Depression-dominated synapse (typical cortical)
    nimcp_gpu_stp_params_t params;
    params.U = 0.5f;          // Baseline release probability
    params.tau_D = 200.0f;    // Depression recovery (ms)
    params.tau_F = 50.0f;     // Facilitation decay (ms)
    return params;
}

nimcp_gpu_calcium_params_t nimcp_gpu_calcium_params_default(void)
{
    nimcp_gpu_calcium_params_t params;
    params.baseline = 0.1f;           // Resting [Ca] = 100 nM
    params.threshold_ltd = 0.3f;      // LTD threshold
    params.threshold_ltp = 0.6f;      // LTP threshold
    params.threshold_sat = 2.0f;      // Saturation threshold
    params.max_conc = 5.0f;           // Maximum [Ca]
    params.decay_tau = 50.0f;         // Decay time constant
    params.pump_rate = 0.02f;         // Pump extrusion rate
    params.buffer_capacity = 0.1f;    // Buffering capacity
    params.influx_alpha = 0.5f;       // Influx rate constant
    params.omega_max = 0.01f;         // Maximum learning rate
    params.omega_power = 2.0f;        // Omega function power
    return params;
}

//=============================================================================
// STDP Kernels
//=============================================================================

/**
 * @brief Kernel to update STDP eligibility traces
 *
 * trace_new = trace_old * exp(-dt/tau) + spike
 */
__global__ void kernel_stdp_update_traces(
    float* __restrict__ pre_trace,
    float* __restrict__ post_trace,
    const float* __restrict__ pre_spikes,
    const float* __restrict__ post_spikes,
    float dt,
    float tau_plus,
    float tau_minus,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Decay factors
    float decay_pre = expf(-dt / tau_plus);
    float decay_post = expf(-dt / tau_minus);

    // Update pre-synaptic trace
    pre_trace[idx] = pre_trace[idx] * decay_pre + pre_spikes[idx];

    // Update post-synaptic trace
    post_trace[idx] = post_trace[idx] * decay_post + post_spikes[idx];
}

bool nimcp_gpu_stdp_update_traces(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* pre_trace,
    nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    float dt,
    const nimcp_gpu_stdp_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !pre_trace || !post_trace || !pre_spikes || !post_spikes || !params) {
        LOG_ERROR("Invalid parameters for STDP trace update");
        return false;
    }

    size_t n = pre_trace->numel;

    kernel_stdp_update_traces<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)pre_trace->data,
        (float*)post_trace->data,
        (const float*)pre_spikes->data,
        (const float*)post_spikes->data,
        dt,
        params->tau_plus,
        params->tau_minus,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to apply STDP weight update
 *
 * For each synapse (i,j):
 * - If post_spike[j]: LTP: dw = A_plus * pre_trace[i]
 * - If pre_spike[i]: LTD: dw = -A_minus * post_trace[j]
 *
 * Weight matrix is [n_post x n_pre], row-major
 */
__global__ void kernel_stdp_apply(
    float* __restrict__ weights,
    const float* __restrict__ pre_spikes,
    const float* __restrict__ post_spikes,
    const float* __restrict__ pre_trace,
    const float* __restrict__ post_trace,
    float A_plus,
    float A_minus,
    float w_min,
    float w_max,
    bool soft_bounds,
    size_t n_pre,
    size_t n_post)
{
    // Each thread handles one synapse
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;  // Row (post-synaptic neuron)
    size_t pre_idx = idx % n_pre;   // Column (pre-synaptic neuron)

    float w = weights[idx];
    float dw = 0.0f;

    // LTP: post spike with pre trace (pre before post)
    if (post_spikes[post_idx] > 0.5f) {
        float ltp = A_plus * pre_trace[pre_idx];
        if (soft_bounds) {
            ltp *= (w_max - w);  // Weight-dependent scaling
        }
        dw += ltp;
    }

    // LTD: pre spike with post trace (post before pre)
    if (pre_spikes[pre_idx] > 0.5f) {
        float ltd = A_minus * post_trace[post_idx];
        if (soft_bounds) {
            ltd *= (w - w_min);  // Weight-dependent scaling
        }
        dw -= ltd;
    }

    // Apply update and clamp
    w += dw;
    w = fmaxf(w_min, fminf(w_max, w));
    weights[idx] = w;
}

bool nimcp_gpu_stdp_apply(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace,
    const nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_stdp_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !pre_spikes || !post_spikes || !pre_trace || !post_trace || !params) {
        LOG_ERROR("Invalid parameters for STDP apply");
        return false;
    }

    // Weights should be [n_post x n_pre]
    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_stdp_apply<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)pre_spikes->data,
        (const float*)post_spikes->data,
        (const float*)pre_trace->data,
        (const float*)post_trace->data,
        params->A_plus,
        params->A_minus,
        params->w_min,
        params->w_max,
        params->soft_bounds,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for dopamine-modulated STDP
 */
__global__ void kernel_stdp_apply_modulated(
    float* __restrict__ weights,
    const float* __restrict__ pre_spikes,
    const float* __restrict__ post_spikes,
    const float* __restrict__ pre_trace,
    const float* __restrict__ post_trace,
    const float* __restrict__ dopamine,
    float A_plus,
    float A_minus,
    float w_min,
    float w_max,
    float da_mod_gain,
    bool soft_bounds,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;
    size_t pre_idx = idx % n_pre;

    float w = weights[idx];
    float dw = 0.0f;

    // Get dopamine modulation (per synapse or broadcast)
    float da = dopamine[idx];
    float mod = 1.0f + da_mod_gain * da;

    // LTP with dopamine modulation
    if (post_spikes[post_idx] > 0.5f) {
        float ltp = A_plus * pre_trace[pre_idx] * mod;
        if (soft_bounds) {
            ltp *= (w_max - w);
        }
        dw += ltp;
    }

    // LTD with dopamine modulation
    if (pre_spikes[pre_idx] > 0.5f) {
        float ltd = A_minus * post_trace[post_idx] * mod;
        if (soft_bounds) {
            ltd *= (w - w_min);
        }
        dw -= ltd;
    }

    w += dw;
    w = fmaxf(w_min, fminf(w_max, w));
    weights[idx] = w;
}

bool nimcp_gpu_stdp_apply_modulated(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace,
    const nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_tensor_t* dopamine,
    const nimcp_gpu_stdp_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !pre_spikes || !post_spikes ||
        !pre_trace || !post_trace || !dopamine || !params) {
        LOG_ERROR("Invalid parameters for modulated STDP");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_stdp_apply_modulated<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)pre_spikes->data,
        (const float*)post_spikes->data,
        (const float*)pre_trace->data,
        (const float*)post_trace->data,
        (const float*)dopamine->data,
        params->A_plus,
        params->A_minus,
        params->w_min,
        params->w_max,
        params->da_mod_gain,
        params->soft_bounds,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Triplet STDP Kernels
//=============================================================================

/**
 * @brief Kernel to update all four triplet STDP traces
 */
__global__ void kernel_triplet_stdp_update_traces(
    float* __restrict__ r1_pre,      // Fast pre-trace
    float* __restrict__ r2_pre,      // Slow pre-trace
    float* __restrict__ o1_post,     // Fast post-trace
    float* __restrict__ o2_post,     // Slow post-trace
    const float* __restrict__ pre_spikes,
    const float* __restrict__ post_spikes,
    float dt,
    float tau_plus,
    float tau_minus,
    float tau_x,
    float tau_y,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Update pre-synaptic traces
    if (idx < n_pre) {
        float decay_fast = expf(-dt / tau_plus);
        float decay_slow = expf(-dt / tau_x);

        r1_pre[idx] = r1_pre[idx] * decay_fast + pre_spikes[idx];
        r2_pre[idx] = r2_pre[idx] * decay_slow + pre_spikes[idx];
    }

    // Update post-synaptic traces (using offset)
    if (idx < n_post) {
        float decay_fast = expf(-dt / tau_minus);
        float decay_slow = expf(-dt / tau_y);

        o1_post[idx] = o1_post[idx] * decay_fast + post_spikes[idx];
        o2_post[idx] = o2_post[idx] * decay_slow + post_spikes[idx];
    }
}

bool nimcp_gpu_triplet_stdp_update_traces(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* r1_pre,
    nimcp_gpu_tensor_t* r2_pre,
    nimcp_gpu_tensor_t* o1_post,
    nimcp_gpu_tensor_t* o2_post,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    float dt,
    const nimcp_gpu_triplet_stdp_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !r1_pre || !r2_pre || !o1_post || !o2_post ||
        !pre_spikes || !post_spikes || !params) {
        LOG_ERROR("Invalid parameters for triplet STDP trace update");
        return false;
    }

    size_t n_pre = pre_spikes->numel;
    size_t n_post = post_spikes->numel;
    size_t n_max = n_pre > n_post ? n_pre : n_post;

    kernel_triplet_stdp_update_traces<<<GRID_SIZE(n_max), BLOCK_SIZE>>>(
        (float*)r1_pre->data,
        (float*)r2_pre->data,
        (float*)o1_post->data,
        (float*)o2_post->data,
        (const float*)pre_spikes->data,
        (const float*)post_spikes->data,
        dt,
        params->tau_plus,
        params->tau_minus,
        params->tau_x,
        params->tau_y,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to apply triplet STDP weight update
 *
 * LTP = A2_plus * r1_pre + A3_plus * r2_pre * o1_post_before
 * LTD = A2_minus * o1_post + A3_minus * r1_pre_before * o2_post
 */
__global__ void kernel_triplet_stdp_apply(
    float* __restrict__ weights,
    const float* __restrict__ pre_spikes,
    const float* __restrict__ post_spikes,
    const float* __restrict__ r1_pre,
    const float* __restrict__ r2_pre,
    const float* __restrict__ o1_post,
    const float* __restrict__ o2_post,
    float A2_plus,
    float A3_plus,
    float A2_minus,
    float A3_minus,
    float w_min,
    float w_max,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;
    size_t pre_idx = idx % n_pre;

    float w = weights[idx];
    float dw = 0.0f;

    // LTP: on post spike
    if (post_spikes[post_idx] > 0.5f) {
        // Pairwise + triplet contribution
        float ltp = A2_plus * r1_pre[pre_idx] +
                    A3_plus * r2_pre[pre_idx] * o1_post[post_idx];
        dw += ltp;
    }

    // LTD: on pre spike
    if (pre_spikes[pre_idx] > 0.5f) {
        // Pairwise + triplet contribution
        float ltd = A2_minus * o1_post[post_idx] +
                    A3_minus * r1_pre[pre_idx] * o2_post[post_idx];
        dw -= ltd;
    }

    w += dw;
    w = fmaxf(w_min, fminf(w_max, w));
    weights[idx] = w;
}

bool nimcp_gpu_triplet_stdp_apply(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* r1_pre,
    const nimcp_gpu_tensor_t* r2_pre,
    const nimcp_gpu_tensor_t* o1_post,
    const nimcp_gpu_tensor_t* o2_post,
    const nimcp_gpu_triplet_stdp_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !pre_spikes || !post_spikes ||
        !r1_pre || !r2_pre || !o1_post || !o2_post || !params) {
        LOG_ERROR("Invalid parameters for triplet STDP apply");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_triplet_stdp_apply<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)pre_spikes->data,
        (const float*)post_spikes->data,
        (const float*)r1_pre->data,
        (const float*)r2_pre->data,
        (const float*)o1_post->data,
        (const float*)o2_post->data,
        params->A2_plus,
        params->A3_plus,
        params->A2_minus,
        params->A3_minus,
        params->w_min,
        params->w_max,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// BCM State Management and Kernels
//=============================================================================

nimcp_gpu_bcm_state_t* nimcp_gpu_bcm_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_pre,
    size_t n_post,
    const nimcp_gpu_bcm_params_t* params)
{
    if (!ctx || !params || n_pre == 0 || n_post == 0) {
        LOG_ERROR("Invalid parameters for BCM state creation");
        return NULL;
    }

    nimcp_gpu_bcm_state_t* state = (nimcp_gpu_bcm_state_t*)nimcp_calloc(1, sizeof(nimcp_gpu_bcm_state_t));
    if (!state) return NULL;

    size_t syn_dims[2] = {n_post, n_pre};
    size_t post_dims[1] = {n_post};

    state->weights = nimcp_gpu_tensor_create(ctx, syn_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->thresholds = nimcp_gpu_tensor_create(ctx, post_dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->avg_activity = nimcp_gpu_tensor_create(ctx, post_dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->eligibility = nimcp_gpu_tensor_create(ctx, syn_dims, 2, NIMCP_GPU_PRECISION_FP32);
    state->n_synapses = n_pre * n_post;

    if (!state->weights || !state->thresholds || !state->avg_activity || !state->eligibility) {
        nimcp_gpu_bcm_state_destroy(state);
        return NULL;
    }

    // Initialize thresholds to min_threshold
    nimcp_gpu_fill(ctx, state->thresholds, params->min_threshold);
    nimcp_gpu_zeros(ctx, state->avg_activity);
    nimcp_gpu_zeros(ctx, state->eligibility);

    // Initialize weights randomly would be done by caller

    LOG_DEBUG("Created BCM state for %zu x %zu synapses", n_post, n_pre);
    return state;
}

void nimcp_gpu_bcm_state_destroy(nimcp_gpu_bcm_state_t* state)
{
    if (!state) return;

    if (state->weights) nimcp_gpu_tensor_destroy(state->weights);
    if (state->thresholds) nimcp_gpu_tensor_destroy(state->thresholds);
    if (state->avg_activity) nimcp_gpu_tensor_destroy(state->avg_activity);
    if (state->eligibility) nimcp_gpu_tensor_destroy(state->eligibility);
    nimcp_free(state);
}

/**
 * @brief Kernel to update BCM sliding thresholds
 *
 * theta = theta + (post^theta_power - theta) * (1 - exp(-dt/tau))
 */
__global__ void kernel_bcm_update_threshold(
    float* __restrict__ thresholds,
    const float* __restrict__ post_activity,
    float dt,
    float threshold_tau,
    float theta_power,
    float min_threshold,
    float max_threshold,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float theta = thresholds[idx];
    float activity = post_activity[idx];

    // Target: activity^theta_power
    float target = powf(activity, theta_power);

    // Exponential moving average
    float alpha = 1.0f - expf(-dt / threshold_tau);
    theta = theta + alpha * (target - theta);

    // Clamp
    theta = fmaxf(min_threshold, fminf(max_threshold, theta));
    thresholds[idx] = theta;
}

bool nimcp_gpu_bcm_update_threshold(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* thresholds,
    const nimcp_gpu_tensor_t* post_activity,
    float dt,
    const nimcp_gpu_bcm_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !thresholds || !post_activity || !params) {
        LOG_ERROR("Invalid parameters for BCM threshold update");
        return false;
    }

    size_t n = thresholds->numel;

    kernel_bcm_update_threshold<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)thresholds->data,
        (const float*)post_activity->data,
        dt,
        params->threshold_tau,
        params->theta_power,
        params->min_threshold,
        params->max_threshold,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to apply BCM learning rule
 *
 * dw = eta * post * (post - theta) * pre
 */
__global__ void kernel_bcm_apply(
    float* __restrict__ weights,
    const float* __restrict__ pre_activity,
    const float* __restrict__ post_activity,
    const float* __restrict__ thresholds,
    float learning_rate,
    float dt,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;
    size_t pre_idx = idx % n_pre;

    float post = post_activity[post_idx];
    float pre = pre_activity[pre_idx];
    float theta = thresholds[post_idx];

    // BCM rule: dw = eta * post * (post - theta) * pre
    float dw = learning_rate * dt * post * (post - theta) * pre;

    weights[idx] += dw;
}

bool nimcp_gpu_bcm_apply(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_activity,
    const nimcp_gpu_tensor_t* post_activity,
    const nimcp_gpu_tensor_t* thresholds,
    float dt,
    const nimcp_gpu_bcm_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !pre_activity || !post_activity || !thresholds || !params) {
        LOG_ERROR("Invalid parameters for BCM apply");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_bcm_apply<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)pre_activity->data,
        (const float*)post_activity->data,
        (const float*)thresholds->data,
        params->learning_rate,
        dt,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for neuromodulator-gated BCM
 */
__global__ void kernel_bcm_apply_modulated(
    float* __restrict__ weights,
    const float* __restrict__ pre_activity,
    const float* __restrict__ post_activity,
    const float* __restrict__ thresholds,
    float neuromodulator,
    float learning_rate,
    float dt,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;
    size_t pre_idx = idx % n_pre;

    float post = post_activity[post_idx];
    float pre = pre_activity[pre_idx];
    float theta = thresholds[post_idx];

    // Modulated BCM rule
    float dw = neuromodulator * learning_rate * dt * post * (post - theta) * pre;

    weights[idx] += dw;
}

bool nimcp_gpu_bcm_apply_modulated(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_activity,
    const nimcp_gpu_tensor_t* post_activity,
    const nimcp_gpu_tensor_t* thresholds,
    float neuromodulator,
    float dt,
    const nimcp_gpu_bcm_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !pre_activity || !post_activity || !thresholds || !params) {
        LOG_ERROR("Invalid parameters for modulated BCM apply");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_bcm_apply_modulated<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)pre_activity->data,
        (const float*)post_activity->data,
        (const float*)thresholds->data,
        neuromodulator,
        params->learning_rate,
        dt,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Homeostatic Plasticity Kernels
//=============================================================================

nimcp_gpu_homeostatic_state_t* nimcp_gpu_homeostatic_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons)
{
    if (!ctx || n_neurons == 0) {
        LOG_ERROR("Invalid parameters for homeostatic state creation");
        return NULL;
    }

    nimcp_gpu_homeostatic_state_t* state =
        (nimcp_gpu_homeostatic_state_t*)nimcp_calloc(1, sizeof(nimcp_gpu_homeostatic_state_t));
    if (!state) return NULL;

    size_t dims[1] = {n_neurons};

    state->scaling_factors = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->avg_rates = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->thresholds = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->gains = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->n_neurons = n_neurons;

    if (!state->scaling_factors || !state->avg_rates ||
        !state->thresholds || !state->gains) {
        nimcp_gpu_homeostatic_state_destroy(state);
        return NULL;
    }

    // Initialize scaling factors to 1.0, rates to 0, thresholds and gains to defaults
    nimcp_gpu_fill(ctx, state->scaling_factors, 1.0f);
    nimcp_gpu_zeros(ctx, state->avg_rates);
    nimcp_gpu_fill(ctx, state->thresholds, 1.0f);  // Default threshold
    nimcp_gpu_fill(ctx, state->gains, 1.0f);       // Default gain

    LOG_DEBUG("Created homeostatic state for %zu neurons", n_neurons);
    return state;
}

void nimcp_gpu_homeostatic_state_destroy(nimcp_gpu_homeostatic_state_t* state)
{
    if (!state) return;

    if (state->scaling_factors) nimcp_gpu_tensor_destroy(state->scaling_factors);
    if (state->avg_rates) nimcp_gpu_tensor_destroy(state->avg_rates);
    if (state->thresholds) nimcp_gpu_tensor_destroy(state->thresholds);
    if (state->gains) nimcp_gpu_tensor_destroy(state->gains);
    nimcp_free(state);
}

/**
 * @brief Kernel to update firing rate estimates
 *
 * rate = rate + (spike - rate) * (1 - exp(-dt/tau))
 */
__global__ void kernel_homeostatic_update_rates(
    float* __restrict__ avg_rates,
    const float* __restrict__ spikes,
    float dt,
    float rate_tau,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float alpha = 1.0f - expf(-dt / rate_tau);
    float rate = avg_rates[idx];
    float spike = spikes[idx];

    // Exponential moving average of spike rate
    rate = rate + alpha * (spike - rate);
    avg_rates[idx] = rate;
}

bool nimcp_gpu_homeostatic_update_rates(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* avg_rates,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_scaling_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !avg_rates || !spikes || !params) {
        LOG_ERROR("Invalid parameters for homeostatic rate update");
        return false;
    }

    size_t n = avg_rates->numel;

    kernel_homeostatic_update_rates<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)avg_rates->data,
        (const float*)spikes->data,
        dt,
        params->rate_tau,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute synaptic scaling factors
 *
 * factor = (target_rate / actual_rate)^alpha
 */
__global__ void kernel_homeostatic_compute_scaling(
    float* __restrict__ scaling_factors,
    const float* __restrict__ avg_rates,
    float target_rate,
    float scaling_exponent,
    float min_scale,
    float max_scale,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float rate = avg_rates[idx];
    float factor;

    if (rate < 1e-6f) {
        factor = max_scale;  // Very low rate -> max scaling up
    } else {
        factor = powf(target_rate / rate, scaling_exponent);
    }

    // Clamp scaling factor
    factor = fmaxf(min_scale, fminf(max_scale, factor));
    scaling_factors[idx] = factor;
}

bool nimcp_gpu_homeostatic_compute_scaling(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* scaling_factors,
    const nimcp_gpu_tensor_t* avg_rates,
    const nimcp_gpu_scaling_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !scaling_factors || !avg_rates || !params) {
        LOG_ERROR("Invalid parameters for scaling factor computation");
        return false;
    }

    size_t n = scaling_factors->numel;

    kernel_homeostatic_compute_scaling<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)scaling_factors->data,
        (const float*)avg_rates->data,
        params->target_rate,
        params->scaling_exponent,
        params->min_scale,
        params->max_scale,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to apply synaptic scaling to weights
 *
 * w_new = clamp(w_old * scaling_factor, w_min, w_max)
 */
__global__ void kernel_homeostatic_apply_scaling(
    float* __restrict__ weights,
    const float* __restrict__ scaling_factors,
    float w_min,
    float w_max,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;  // Row (post-synaptic)

    float w = weights[idx];
    float scale = scaling_factors[post_idx];

    w = w * scale;
    w = fmaxf(w_min, fminf(w_max, w));
    weights[idx] = w;
}

bool nimcp_gpu_homeostatic_apply_scaling(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* scaling_factors,
    float w_min,
    float w_max)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !scaling_factors) {
        LOG_ERROR("Invalid parameters for scaling application");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_homeostatic_apply_scaling<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)scaling_factors->data,
        w_min,
        w_max,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for intrinsic plasticity (threshold adaptation)
 *
 * threshold += eta * (actual_rate - target_rate)
 */
__global__ void kernel_intrinsic_plasticity_update(
    float* __restrict__ thresholds,
    const float* __restrict__ avg_rates,
    float dt,
    float target_rate,
    float learning_rate,
    float min_threshold,
    float max_threshold,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float theta = thresholds[idx];
    float rate = avg_rates[idx];

    // Increase threshold if rate > target, decrease if rate < target
    float dtheta = learning_rate * dt * (rate - target_rate);
    theta += dtheta;

    theta = fmaxf(min_threshold, fminf(max_threshold, theta));
    thresholds[idx] = theta;
}

bool nimcp_gpu_intrinsic_plasticity_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* thresholds,
    const nimcp_gpu_tensor_t* avg_rates,
    float dt,
    const nimcp_gpu_intrinsic_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !thresholds || !avg_rates || !params) {
        LOG_ERROR("Invalid parameters for intrinsic plasticity update");
        return false;
    }

    size_t n = thresholds->numel;

    kernel_intrinsic_plasticity_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)thresholds->data,
        (const float*)avg_rates->data,
        dt,
        params->target_rate,
        params->learning_rate,
        params->min_threshold,
        params->max_threshold,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// STP (Short-Term Plasticity) Kernels
//=============================================================================

nimcp_gpu_stp_state_t* nimcp_gpu_stp_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_gpu_stp_params_t* params)
{
    if (!ctx || !params || n_synapses == 0) {
        LOG_ERROR("Invalid parameters for STP state creation");
        return NULL;
    }

    nimcp_gpu_stp_state_t* state = (nimcp_gpu_stp_state_t*)nimcp_calloc(1, sizeof(nimcp_gpu_stp_state_t));
    if (!state) return NULL;

    size_t dims[1] = {n_synapses};

    state->x = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->u = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->last_spike = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->n_synapses = n_synapses;
    state->params = *params;

    if (!state->x || !state->u || !state->last_spike) {
        nimcp_gpu_stp_state_destroy(state);
        return NULL;
    }

    // Initialize: x = 1 (full resources), u = U (baseline utilization)
    nimcp_gpu_fill(ctx, state->x, 1.0f);
    nimcp_gpu_fill(ctx, state->u, params->U);
    nimcp_gpu_fill(ctx, state->last_spike, -1000.0f);  // No recent spikes

    LOG_DEBUG("Created STP state for %zu synapses", n_synapses);
    return state;
}

void nimcp_gpu_stp_state_destroy(nimcp_gpu_stp_state_t* state)
{
    if (!state) return;

    if (state->x) nimcp_gpu_tensor_destroy(state->x);
    if (state->u) nimcp_gpu_tensor_destroy(state->u);
    if (state->last_spike) nimcp_gpu_tensor_destroy(state->last_spike);
    nimcp_free(state);
}

/**
 * @brief Kernel for STP continuous decay
 *
 * x += (1 - x) * (1 - exp(-dt/tau_D))  (resource recovery)
 * u += (U - u) * (1 - exp(-dt/tau_F))  (facilitation decay)
 */
__global__ void kernel_stp_update(
    float* __restrict__ x,
    float* __restrict__ u,
    float dt,
    float U,
    float tau_D,
    float tau_F,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float alpha_D = 1.0f - expf(-dt / tau_D);
    float alpha_F = 1.0f - expf(-dt / tau_F);

    // Resource recovery
    x[idx] = x[idx] + (1.0f - x[idx]) * alpha_D;

    // Facilitation decay back to baseline
    u[idx] = u[idx] + (U - u[idx]) * alpha_F;
}

bool nimcp_gpu_stp_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for STP update");
        return false;
    }

    size_t n = state->n_synapses;
    const nimcp_gpu_stp_params_t* p = &state->params;

    kernel_stp_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->x->data,
        (float*)state->u->data,
        dt,
        p->U,
        p->tau_D,
        p->tau_F,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to process presynaptic spikes for STP
 *
 * On spike:
 * u = u + U * (1 - u)  (facilitation)
 * x = x - u * x        (depression)
 */
__global__ void kernel_stp_process_spikes(
    float* __restrict__ x,
    float* __restrict__ u,
    const float* __restrict__ spikes,
    float U,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    if (spikes[idx] > 0.5f) {
        // Facilitation (before depression)
        float u_new = u[idx] + U * (1.0f - u[idx]);

        // Depression
        float x_new = x[idx] * (1.0f - u_new);

        x[idx] = x_new;
        u[idx] = u_new;
    }
}

bool nimcp_gpu_stp_process_spikes(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state,
    const nimcp_gpu_tensor_t* spikes)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !spikes) {
        LOG_ERROR("Invalid parameters for STP spike processing");
        return false;
    }

    size_t n = state->n_synapses;

    kernel_stp_process_spikes<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->x->data,
        (float*)state->u->data,
        (const float*)spikes->data,
        state->params.U,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute STP modulation factors
 *
 * modulation = u * x
 */
__global__ void kernel_stp_get_modulation(
    const float* __restrict__ x,
    const float* __restrict__ u,
    float* __restrict__ modulation,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    modulation[idx] = u[idx] * x[idx];
}

bool nimcp_gpu_stp_get_modulation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_stp_state_t* state,
    nimcp_gpu_tensor_t* modulation)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !modulation) {
        LOG_ERROR("Invalid parameters for STP modulation");
        return false;
    }

    size_t n = state->n_synapses;

    kernel_stp_get_modulation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)state->x->data,
        (const float*)state->u->data,
        (float*)modulation->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to apply STP modulation to weights
 *
 * effective_weight = base_weight * u * x
 */
__global__ void kernel_stp_apply(
    const float* __restrict__ base_weights,
    const float* __restrict__ x,
    const float* __restrict__ u,
    float* __restrict__ effective_weights,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    effective_weights[idx] = base_weights[idx] * u[idx] * x[idx];
}

bool nimcp_gpu_stp_apply(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* base_weights,
    const nimcp_gpu_stp_state_t* state,
    nimcp_gpu_tensor_t* effective_weights)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !base_weights || !state || !effective_weights) {
        LOG_ERROR("Invalid parameters for STP apply");
        return false;
    }

    size_t n = base_weights->numel;

    kernel_stp_apply<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)base_weights->data,
        (const float*)state->x->data,
        (const float*)state->u->data,
        (float*)effective_weights->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_stp_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for STP reset");
        return false;
    }

    // Reset to initial state
    nimcp_gpu_fill(ctx, state->x, 1.0f);
    nimcp_gpu_fill(ctx, state->u, state->params.U);

    return true;
}

//=============================================================================
// Calcium Dynamics Kernels
//=============================================================================

nimcp_gpu_calcium_state_t* nimcp_gpu_calcium_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_gpu_calcium_params_t* params)
{
    if (!ctx || !params || n_synapses == 0) {
        LOG_ERROR("Invalid parameters for calcium state creation");
        return NULL;
    }

    nimcp_gpu_calcium_state_t* state =
        (nimcp_gpu_calcium_state_t*)nimcp_calloc(1, sizeof(nimcp_gpu_calcium_state_t));
    if (!state) return NULL;

    size_t dims[1] = {n_synapses};

    state->concentration = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->learning_rate = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->nmda_activation = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->n_synapses = n_synapses;
    state->params = *params;

    if (!state->concentration || !state->learning_rate || !state->nmda_activation) {
        nimcp_gpu_calcium_state_destroy(state);
        return NULL;
    }

    // Initialize to baseline calcium
    nimcp_gpu_fill(ctx, state->concentration, params->baseline);
    nimcp_gpu_zeros(ctx, state->learning_rate);
    nimcp_gpu_zeros(ctx, state->nmda_activation);

    LOG_DEBUG("Created calcium state for %zu synapses", n_synapses);
    return state;
}

void nimcp_gpu_calcium_state_destroy(nimcp_gpu_calcium_state_t* state)
{
    if (!state) return;

    if (state->concentration) nimcp_gpu_tensor_destroy(state->concentration);
    if (state->learning_rate) nimcp_gpu_tensor_destroy(state->learning_rate);
    if (state->nmda_activation) nimcp_gpu_tensor_destroy(state->nmda_activation);
    nimcp_free(state);
}

/**
 * @brief Kernel for calcium decay/extrusion
 *
 * d[Ca]/dt = -pump_rate * ([Ca] - baseline) - buffer_cap * ([Ca] - baseline)
 */
__global__ void kernel_calcium_update(
    float* __restrict__ concentration,
    float dt,
    float baseline,
    float decay_tau,
    float pump_rate,
    float buffer_capacity,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ca = concentration[idx];

    // Combined decay from pump and buffering
    float decay_rate = pump_rate + buffer_capacity;
    float alpha = expf(-decay_rate * dt / decay_tau);

    // Exponential decay toward baseline
    ca = baseline + (ca - baseline) * alpha;

    concentration[idx] = fmaxf(0.0f, ca);  // Calcium can't go negative
}

bool nimcp_gpu_calcium_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for calcium update");
        return false;
    }

    size_t n = state->n_synapses;
    const nimcp_gpu_calcium_params_t* p = &state->params;

    kernel_calcium_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        dt,
        p->baseline,
        p->decay_tau,
        p->pump_rate,
        p->buffer_capacity,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Device function for Mg2+ block
 *
 * block = 1 / (1 + [Mg] * exp(-0.062 * V) / 3.57)
 * Assuming [Mg] = 1 mM
 */
__device__ inline float device_mg_block(float voltage)
{
    const float mg_conc = 1.0f;  // mM
    return 1.0f / (1.0f + mg_conc * expf(-0.062f * voltage) / 3.57f);
}

/**
 * @brief Kernel for NMDA-mediated calcium influx
 *
 * [Ca] += alpha * nmda_activation * mg_block
 */
__global__ void kernel_calcium_nmda_influx(
    float* __restrict__ concentration,
    float* __restrict__ nmda_state,
    const float* __restrict__ nmda_activation,
    const float* __restrict__ voltage,
    float influx_alpha,
    float max_conc,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float nmda = nmda_activation[idx];
    float v = voltage[idx];
    float mg = device_mg_block(v);

    // Update stored NMDA activation
    nmda_state[idx] = nmda;

    // Calcium influx
    float influx = influx_alpha * nmda * mg;
    float ca = concentration[idx] + influx;

    concentration[idx] = fminf(ca, max_conc);
}

bool nimcp_gpu_calcium_nmda_influx(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state,
    const nimcp_gpu_tensor_t* nmda_activation,
    const nimcp_gpu_tensor_t* postsynaptic_voltage)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !nmda_activation || !postsynaptic_voltage) {
        LOG_ERROR("Invalid parameters for calcium NMDA influx");
        return false;
    }

    size_t n = state->n_synapses;
    const nimcp_gpu_calcium_params_t* p = &state->params;

    kernel_calcium_nmda_influx<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        (float*)state->nmda_activation->data,
        (const float*)nmda_activation->data,
        (const float*)postsynaptic_voltage->data,
        p->influx_alpha,
        p->max_conc,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute Omega learning rate function
 *
 * omega([Ca]) = omega_max * (([Ca] - theta_LTD) / (theta_LTP - theta_LTD))^p
 * Clamped to [0, omega_max] and sign depends on regime
 *
 * If [Ca] < theta_LTD: omega = 0 (no plasticity)
 * If theta_LTD <= [Ca] < theta_LTP: omega < 0 (LTD)
 * If [Ca] >= theta_LTP: omega > 0 (LTP)
 */
__global__ void kernel_calcium_compute_learning_rate(
    const float* __restrict__ concentration,
    float* __restrict__ learning_rate,
    float threshold_ltd,
    float threshold_ltp,
    float omega_max,
    float omega_power,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ca = concentration[idx];
    float omega = 0.0f;

    if (ca >= threshold_ltp) {
        // LTP regime
        float x = (ca - threshold_ltp) / (threshold_ltp - threshold_ltd);
        omega = omega_max * powf(x, omega_power);
        omega = fminf(omega, omega_max);
    } else if (ca >= threshold_ltd) {
        // LTD regime
        float x = (ca - threshold_ltd) / (threshold_ltp - threshold_ltd);
        omega = -omega_max * powf(1.0f - x, omega_power) * x;
    }
    // else: below LTD threshold, no plasticity

    learning_rate[idx] = omega;
}

bool nimcp_gpu_calcium_compute_learning_rate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for calcium learning rate computation");
        return false;
    }

    size_t n = state->n_synapses;
    const nimcp_gpu_calcium_params_t* p = &state->params;

    kernel_calcium_compute_learning_rate<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)state->concentration->data,
        (float*)state->learning_rate->data,
        p->threshold_ltd,
        p->threshold_ltp,
        p->omega_max,
        p->omega_power,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for calcium-dependent weight update
 *
 * dw = omega * pre * post
 */
__global__ void kernel_calcium_apply_plasticity(
    float* __restrict__ weights,
    const float* __restrict__ learning_rate,
    const float* __restrict__ pre_activity,
    const float* __restrict__ post_activity,
    size_t n_pre,
    size_t n_post)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = n_pre * n_post;
    if (idx >= total) return;

    size_t post_idx = idx / n_pre;
    size_t pre_idx = idx % n_pre;

    float omega = learning_rate[idx];  // Per-synapse learning rate
    float pre = pre_activity[pre_idx];
    float post = post_activity[post_idx];

    weights[idx] += omega * pre * post;
}

bool nimcp_gpu_calcium_apply_plasticity(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_calcium_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity,
    const nimcp_gpu_tensor_t* post_activity)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights || !state || !pre_activity || !post_activity) {
        LOG_ERROR("Invalid parameters for calcium plasticity");
        return false;
    }

    size_t n_post = weights->dims[0];
    size_t n_pre = weights->ndim > 1 ? weights->dims[1] : 1;
    size_t total = n_pre * n_post;

    kernel_calcium_apply_plasticity<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)state->learning_rate->data,
        (const float*)pre_activity->data,
        (const float*)post_activity->data,
        n_pre,
        n_post);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_calcium_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for calcium reset");
        return false;
    }

    // Reset to baseline
    nimcp_gpu_fill(ctx, state->concentration, state->params.baseline);
    nimcp_gpu_zeros(ctx, state->learning_rate);
    nimcp_gpu_zeros(ctx, state->nmda_activation);

    return true;
}

/**
 * @brief Kernel to compute Mg2+ block factor
 *
 * block = 1 / (1 + [Mg] * exp(-0.062 * V) / 3.57)
 */
__global__ void kernel_calcium_mg_block(
    const float* __restrict__ voltage,
    float* __restrict__ mg_block,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    mg_block[idx] = device_mg_block(voltage[idx]);
}

bool nimcp_gpu_calcium_mg_block(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* voltage,
    nimcp_gpu_tensor_t* mg_block)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !voltage || !mg_block) {
        LOG_ERROR("Invalid parameters for Mg block computation");
        return false;
    }

    size_t n = voltage->numel;

    kernel_calcium_mg_block<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)voltage->data,
        (float*)mg_block->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to determine plasticity regime from calcium
 *
 * 0 = no plasticity ([Ca] < theta_LTD)
 * 1 = LTD (theta_LTD <= [Ca] < transition)
 * 2 = transition zone
 * 3 = LTP ([Ca] >= theta_LTP)
 * 4 = saturated ([Ca] >= theta_sat)
 */
__global__ void kernel_calcium_get_regime(
    const float* __restrict__ concentration,
    int* __restrict__ regime,
    float threshold_ltd,
    float threshold_ltp,
    float threshold_sat,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ca = concentration[idx];
    int r;

    if (ca < threshold_ltd) {
        r = 0;  // No plasticity
    } else if (ca < (threshold_ltd + threshold_ltp) * 0.5f) {
        r = 1;  // LTD
    } else if (ca < threshold_ltp) {
        r = 2;  // Transition
    } else if (ca < threshold_sat) {
        r = 3;  // LTP
    } else {
        r = 4;  // Saturated
    }

    regime[idx] = r;
}

bool nimcp_gpu_calcium_get_regime(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* concentration,
    nimcp_gpu_tensor_t* regime,
    const nimcp_gpu_calcium_params_t* params)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !concentration || !regime || !params) {
        LOG_ERROR("Invalid parameters for calcium regime detection");
        return false;
    }

    size_t n = concentration->numel;

    kernel_calcium_get_regime<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (const float*)concentration->data,
        (int*)regime->data,
        params->threshold_ltd,
        params->threshold_ltp,
        params->threshold_sat,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/*=============================================================================
 * CSR Weight Update Kernels for GPU-Side Plasticity
 *
 * These operate directly on the GPU weight cache's CSR sparse tensors,
 * eliminating the GPU→CPU→GPU round-trip for plasticity updates.
 *=============================================================================*/

/* STDP weight update on CSR */
__global__ void kernel_stdp_weight_update_csr(
    float* __restrict__ csr_values,
    const int* __restrict__ row_ptrs,
    const int* __restrict__ col_indices,
    const float* __restrict__ pre_traces,
    const float* __restrict__ post_traces,
    int nnz, int rows,
    float a_plus, float a_minus, float lr,
    float min_trace, float w_min, float w_max)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    /* Binary search for row */
    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (row_ptrs[mid + 1] <= idx) lo = mid + 1;
        else if (row_ptrs[mid] > idx) hi = mid - 1;
        else { row = mid; break; }
    }
    if (lo > hi) row = lo;

    int col = col_indices[idx];
    float pre_t = pre_traces[col];
    float post_t = post_traces[row];
    float dw = 0.0f;

    /* LTP: pre trace significant → potentiate */
    if (pre_t > min_trace) {
        dw += a_plus * lr * pre_t;
    }
    /* LTD: post trace significant → depress */
    if (post_t > min_trace) {
        dw -= a_minus * lr * post_t;
    }

    if (dw != 0.0f) {
        float new_w = csr_values[idx] + dw;
        if (new_w < w_min) new_w = w_min;
        if (new_w > w_max) new_w = w_max;
        csr_values[idx] = new_w;
    }
}

extern "C" int nimcp_gpu_stdp_weight_update_csr(
    nimcp_gpu_context_t* ctx,
    float* csr_values, const int* row_ptrs, const int* col_indices,
    const float* pre_traces, const float* post_traces,
    int nnz, int rows,
    float a_plus, float a_minus, float lr,
    float min_trace, float w_min, float w_max)
{
    if (!ctx || !csr_values || nnz == 0) return -1;
    cudaStream_t stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_stdp_weight_update_csr<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
        csr_values, row_ptrs, col_indices, pre_traces, post_traces,
        nnz, rows, a_plus, a_minus, lr, min_trace, w_min, w_max);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return 0;
}

/* BCM weight update on CSR */
__global__ void kernel_bcm_weight_update_csr(
    float* __restrict__ csr_values,
    const int* __restrict__ row_ptrs,
    const int* __restrict__ col_indices,
    const float* __restrict__ post_activity,
    const float* __restrict__ pre_activity,
    const float* __restrict__ thresholds,
    int nnz, int rows,
    float lr, float w_min, float w_max)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (row_ptrs[mid + 1] <= idx) lo = mid + 1;
        else if (row_ptrs[mid] > idx) hi = mid - 1;
        else { row = mid; break; }
    }
    if (lo > hi) row = lo;

    int col = col_indices[idx];
    float post = post_activity[row];
    float theta = thresholds[row];
    float phi = post * (post - theta);  /* BCM plasticity factor */
    float pre = pre_activity[col];

    float dw = lr * phi * pre;

    /* Sparse skip: negligible updates */
    if (fabsf(dw) < 1e-7f) return;

    float new_w = csr_values[idx] + dw;
    if (new_w < w_min) new_w = w_min;
    if (new_w > w_max) new_w = w_max;
    csr_values[idx] = new_w;
}

extern "C" int nimcp_gpu_bcm_weight_update_csr(
    nimcp_gpu_context_t* ctx,
    float* csr_values, const int* row_ptrs, const int* col_indices,
    const float* post_activity, const float* pre_activity,
    const float* thresholds,
    int nnz, int rows,
    float lr, float w_min, float w_max)
{
    if (!ctx || !csr_values || nnz == 0) return -1;
    cudaStream_t stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_bcm_weight_update_csr<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
        csr_values, row_ptrs, col_indices, post_activity, pre_activity,
        thresholds, nnz, rows, lr, w_min, w_max);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return 0;
}

/* Homeostatic synaptic scaling on CSR */
__global__ void kernel_homeostatic_scale_csr(
    float* __restrict__ csr_values,
    const int* __restrict__ row_ptrs,
    const float* __restrict__ avg_rates,
    int nnz, int rows,
    float target_rate, float min_scale, float max_scale)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= nnz) return;

    int lo = 0, hi = rows - 1, row = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (row_ptrs[mid + 1] <= idx) lo = mid + 1;
        else if (row_ptrs[mid] > idx) hi = mid - 1;
        else { row = mid; break; }
    }
    if (lo > hi) row = lo;

    float rate = avg_rates[row];
    if (rate < 1e-6f) rate = 1e-6f;  /* prevent division by zero */
    float scale = target_rate / rate;

    /* Clamp scaling factor */
    if (scale < min_scale) scale = min_scale;
    if (scale > max_scale) scale = max_scale;

    /* Only scale if significantly different from 1.0 */
    if (fabsf(scale - 1.0f) > 0.001f) {
        csr_values[idx] *= scale;
    }
}

extern "C" int nimcp_gpu_homeostatic_scale_csr(
    nimcp_gpu_context_t* ctx,
    float* csr_values, const int* row_ptrs,
    const float* avg_rates,
    int nnz, int rows,
    float target_rate, float min_scale, float max_scale)
{
    if (!ctx || !csr_values || nnz == 0) return -1;
    cudaStream_t stream = nimcp_gpu_get_pool_stream(ctx);
    kernel_homeostatic_scale_csr<<<GRID_SIZE(nnz), BLOCK_SIZE, 0, stream>>>(
        csr_values, row_ptrs, avg_rates, nnz, rows,
        target_rate, min_scale, max_scale);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return 0;
}

#else  // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/plasticity/nimcp_plasticity_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>

#define LOG_MODULE "PLASTICITY_GPU"

// Default parameter functions (work without CUDA)
nimcp_gpu_stdp_params_t nimcp_gpu_stdp_params_default(void)
{
    nimcp_gpu_stdp_params_t params;
    params.A_plus = 0.01f;
    params.A_minus = 0.012f;
    params.tau_plus = 20.0f;
    params.tau_minus = 20.0f;
    params.w_max = 1.0f;
    params.w_min = 0.0f;
    params.da_mod_gain = 1.0f;
    params.burst_amp = 1.5f;
    params.soft_bounds = true;
    return params;
}

nimcp_gpu_triplet_stdp_params_t nimcp_gpu_triplet_stdp_params_default(void)
{
    nimcp_gpu_triplet_stdp_params_t params;
    params.A2_plus = 5e-10f;
    params.A3_plus = 6.2e-3f;
    params.A2_minus = 7e-3f;
    params.A3_minus = 2.3e-4f;
    params.tau_plus = 16.8f;
    params.tau_minus = 33.7f;
    params.tau_x = 101.0f;
    params.tau_y = 125.0f;
    params.w_max = 1.0f;
    params.w_min = 0.0f;
    return params;
}

nimcp_gpu_bcm_params_t nimcp_gpu_bcm_params_default(void)
{
    nimcp_gpu_bcm_params_t params;
    params.learning_rate = 0.001f;
    params.threshold_tau = 1000.0f;
    params.activity_tau = 100.0f;
    params.min_threshold = 0.01f;
    params.max_threshold = 10.0f;
    params.theta_power = 2.0f;
    return params;
}

nimcp_gpu_scaling_params_t nimcp_gpu_scaling_params_default(void)
{
    nimcp_gpu_scaling_params_t params;
    params.target_rate = 10.0f;
    params.scaling_tau = 10000.0f;
    params.scaling_exponent = 1.0f;
    params.min_scale = 0.1f;
    params.max_scale = 10.0f;
    params.rate_tau = 1000.0f;
    return params;
}

nimcp_gpu_intrinsic_params_t nimcp_gpu_intrinsic_params_default(void)
{
    nimcp_gpu_intrinsic_params_t params;
    params.target_rate = 10.0f;
    params.threshold_tau = 5000.0f;
    params.gain_tau = 5000.0f;
    params.min_threshold = -100.0f;
    params.max_threshold = 100.0f;
    params.min_gain = 0.1f;
    params.max_gain = 10.0f;
    params.learning_rate = 0.0001f;
    return params;
}

nimcp_gpu_stp_params_t nimcp_gpu_stp_params_default(void)
{
    nimcp_gpu_stp_params_t params;
    params.U = 0.5f;
    params.tau_D = 200.0f;
    params.tau_F = 50.0f;
    return params;
}

nimcp_gpu_calcium_params_t nimcp_gpu_calcium_params_default(void)
{
    nimcp_gpu_calcium_params_t params;
    params.baseline = 0.1f;
    params.threshold_ltd = 0.3f;
    params.threshold_ltp = 0.6f;
    params.threshold_sat = 2.0f;
    params.max_conc = 5.0f;
    params.decay_tau = 50.0f;
    params.pump_rate = 0.02f;
    params.buffer_capacity = 0.1f;
    params.influx_alpha = 0.5f;
    params.omega_max = 0.01f;
    params.omega_power = 2.0f;
    return params;
}

// Stub functions for CPU fallback
#define CUDA_NOT_ENABLED_MSG "CUDA not enabled, GPU plasticity operations unavailable"

bool nimcp_gpu_stdp_update_traces(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* pre_trace,
    nimcp_gpu_tensor_t* post_trace, const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes, float dt, const nimcp_gpu_stdp_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_stdp_apply(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace, const nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_stdp_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_stdp_apply_modulated(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace, const nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_tensor_t* dopamine, const nimcp_gpu_stdp_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_triplet_stdp_update_traces(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* r1_pre, nimcp_gpu_tensor_t* r2_pre,
    nimcp_gpu_tensor_t* o1_post, nimcp_gpu_tensor_t* o2_post,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    float dt, const nimcp_gpu_triplet_stdp_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_triplet_stdp_apply(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes, const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* r1_pre, const nimcp_gpu_tensor_t* r2_pre,
    const nimcp_gpu_tensor_t* o1_post, const nimcp_gpu_tensor_t* o2_post,
    const nimcp_gpu_triplet_stdp_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

nimcp_gpu_bcm_state_t* nimcp_gpu_bcm_state_create(nimcp_gpu_context_t* ctx,
    size_t n_pre, size_t n_post, const nimcp_gpu_bcm_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return NULL;
}

void nimcp_gpu_bcm_state_destroy(nimcp_gpu_bcm_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_bcm_update_threshold(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* thresholds,
    const nimcp_gpu_tensor_t* post_activity, float dt, const nimcp_gpu_bcm_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_bcm_apply(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_activity, const nimcp_gpu_tensor_t* post_activity,
    const nimcp_gpu_tensor_t* thresholds, float dt, const nimcp_gpu_bcm_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_bcm_apply_modulated(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_activity, const nimcp_gpu_tensor_t* post_activity,
    const nimcp_gpu_tensor_t* thresholds, float neuromodulator, float dt,
    const nimcp_gpu_bcm_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

nimcp_gpu_homeostatic_state_t* nimcp_gpu_homeostatic_state_create(
    nimcp_gpu_context_t* ctx, size_t n_neurons)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return NULL;
}

void nimcp_gpu_homeostatic_state_destroy(nimcp_gpu_homeostatic_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_homeostatic_update_rates(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* avg_rates, const nimcp_gpu_tensor_t* spikes,
    float dt, const nimcp_gpu_scaling_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_homeostatic_compute_scaling(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* scaling_factors, const nimcp_gpu_tensor_t* avg_rates,
    const nimcp_gpu_scaling_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_homeostatic_apply_scaling(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights, const nimcp_gpu_tensor_t* scaling_factors,
    float w_min, float w_max)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_intrinsic_plasticity_update(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* thresholds, const nimcp_gpu_tensor_t* avg_rates,
    float dt, const nimcp_gpu_intrinsic_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

nimcp_gpu_stp_state_t* nimcp_gpu_stp_state_create(nimcp_gpu_context_t* ctx,
    size_t n_synapses, const nimcp_gpu_stp_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return NULL;
}

void nimcp_gpu_stp_state_destroy(nimcp_gpu_stp_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_stp_update(nimcp_gpu_context_t* ctx, nimcp_gpu_stp_state_t* state, float dt)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_stp_process_spikes(nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state, const nimcp_gpu_tensor_t* spikes)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_stp_get_modulation(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_stp_state_t* state, nimcp_gpu_tensor_t* modulation)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_stp_apply(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* base_weights,
    const nimcp_gpu_stp_state_t* state, nimcp_gpu_tensor_t* effective_weights)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_stp_reset(nimcp_gpu_context_t* ctx, nimcp_gpu_stp_state_t* state)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

nimcp_gpu_calcium_state_t* nimcp_gpu_calcium_state_create(nimcp_gpu_context_t* ctx,
    size_t n_synapses, const nimcp_gpu_calcium_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return NULL;
}

void nimcp_gpu_calcium_state_destroy(nimcp_gpu_calcium_state_t* state)
{
    if (state) nimcp_free(state);
}

bool nimcp_gpu_calcium_update(nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state, float dt)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_calcium_nmda_influx(nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state, const nimcp_gpu_tensor_t* nmda_activation,
    const nimcp_gpu_tensor_t* postsynaptic_voltage)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_calcium_compute_learning_rate(nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_calcium_apply_plasticity(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights, const nimcp_gpu_calcium_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity, const nimcp_gpu_tensor_t* post_activity)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_calcium_reset(nimcp_gpu_context_t* ctx, nimcp_gpu_calcium_state_t* state)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_calcium_mg_block(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* voltage, nimcp_gpu_tensor_t* mg_block)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

bool nimcp_gpu_calcium_get_regime(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* concentration, nimcp_gpu_tensor_t* regime,
    const nimcp_gpu_calcium_params_t* params)
{
    LOG_ERROR(CUDA_NOT_ENABLED_MSG);
    return false;
}

/* CSR weight update stubs (no CUDA) */
int nimcp_gpu_stdp_weight_update_csr(void* ctx, float* v, const int* rp, const int* ci,
    const float* pt, const float* po, int nnz, int r, float ap, float am,
    float lr, float mt, float wn, float wx) { (void)ctx;(void)v;(void)rp;(void)ci;(void)pt;(void)po;(void)nnz;(void)r;(void)ap;(void)am;(void)lr;(void)mt;(void)wn;(void)wx; return -1; }
int nimcp_gpu_bcm_weight_update_csr(void* ctx, float* v, const int* rp, const int* ci,
    const float* pa, const float* pr, const float* th, int nnz, int r,
    float lr, float wn, float wx) { (void)ctx;(void)v;(void)rp;(void)ci;(void)pa;(void)pr;(void)th;(void)nnz;(void)r;(void)lr;(void)wn;(void)wx; return -1; }
int nimcp_gpu_homeostatic_scale_csr(void* ctx, float* v, const int* rp,
    const float* ar, int nnz, int r, float tr, float ms, float mx) { (void)ctx;(void)v;(void)rp;(void)ar;(void)nnz;(void)r;(void)tr;(void)ms;(void)mx; return -1; }

#endif // NIMCP_ENABLE_CUDA
