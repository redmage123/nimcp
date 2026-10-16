/**
 * @file nimcp_neuromodulator_kernels.cu
 * @brief GPU Neuromodulator CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for neuromodulator system computations
 * WHY:  GPU acceleration for biologically-accurate neuromodulator dynamics
 * HOW:  Custom kernels for dopamine, serotonin, acetylcholine, norepinephrine
 *
 * ARCHITECTURE:
 * - Dopamine: Reward prediction, VTA/SNc dynamics, D1/D2 receptors
 * - Serotonin: Raphe nucleus, 5-HT1A/2A receptors, mood modulation
 * - Acetylcholine: Basal forebrain, muscarinic/nicotinic, attention
 * - Norepinephrine: Locus coeruleus, adrenergic receptors, arousal
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/neuromodulators/nimcp_neuromodulator_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#define LOG_MODULE "NEUROMODULATOR_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_dopamine_params_t nimcp_gpu_dopamine_params_default(void)
{
    nimcp_gpu_dopamine_params_t params;
    params.baseline = 0.05f;        // ~50 nM baseline
    params.release_rate = 0.1f;     // Release per spike
    params.reuptake_tau = 50.0f;    // DAT reuptake ~50ms
    params.decay_tau = 200.0f;      // Diffusion decay
    params.d1_affinity = 0.5f;      // D1 Kd ~500 nM
    params.d2_affinity = 0.02f;     // D2 Kd ~20 nM (higher affinity)
    params.max_conc = 2.0f;         // Max ~2 uM during burst
    params.burst_factor = 3.0f;     // Burst amplification
    params.tonic_rate = 4.0f;       // ~4 Hz tonic firing
    params.phasic_amplitude = 0.5f; // Phasic burst amplitude
    return params;
}

nimcp_gpu_serotonin_params_t nimcp_gpu_serotonin_params_default(void)
{
    nimcp_gpu_serotonin_params_t params;
    params.baseline = 0.01f;        // ~10 nM baseline
    params.release_rate = 0.05f;    // Lower release than DA
    params.reuptake_tau = 100.0f;   // SERT slower than DAT
    params.decay_tau = 300.0f;      // Slower decay
    params.ht1a_affinity = 0.001f;  // 5-HT1A high affinity
    params.ht2a_affinity = 0.01f;   // 5-HT2A moderate affinity
    params.max_conc = 0.5f;         // Max concentration
    params.autoreceptor_gain = 0.5f;// Autoreceptor feedback
    params.synthesis_rate = 0.001f; // Tryptophan hydroxylase
    return params;
}

nimcp_gpu_acetylcholine_params_t nimcp_gpu_acetylcholine_params_default(void)
{
    nimcp_gpu_acetylcholine_params_t params;
    params.baseline = 0.02f;        // ~20 nM baseline
    params.release_rate = 0.2f;     // Higher release rate
    params.ache_rate = 0.5f;        // Fast AChE hydrolysis
    params.decay_tau = 20.0f;       // Very fast decay
    params.m1_affinity = 0.01f;     // M1 muscarinic affinity
    params.m2_affinity = 0.005f;    // M2 muscarinic (presynaptic)
    params.nicotinic_affinity = 0.1f;// Nicotinic receptor
    params.max_conc = 1.0f;         // Max concentration
    params.desensitization_tau = 500.0f; // Receptor desensitization
    params.choline_uptake_km = 0.1f;// CHT Km
    return params;
}

nimcp_gpu_norepinephrine_params_t nimcp_gpu_norepinephrine_params_default(void)
{
    nimcp_gpu_norepinephrine_params_t params;
    params.baseline = 0.02f;        // ~20 nM baseline
    params.release_rate = 0.08f;    // Release per spike
    params.reuptake_tau = 80.0f;    // NET reuptake
    params.decay_tau = 250.0f;      // Decay time
    params.alpha1_affinity = 0.1f;  // Alpha-1 affinity
    params.alpha2_affinity = 0.01f; // Alpha-2 high affinity (autoreceptor)
    params.beta_affinity = 0.05f;   // Beta receptor affinity
    params.max_conc = 1.0f;         // Max concentration
    params.lc_baseline_rate = 2.0f; // LC baseline ~2 Hz
    params.stress_gain = 2.0f;      // Stress amplification
    return params;
}

//=============================================================================
// Dopamine Kernels
//=============================================================================

/**
 * @brief Kernel to update dopamine concentration
 *
 * dDA/dt = release * spikes - (DA - baseline) / reuptake_tau - DA / decay_tau
 */
__global__ void kernel_dopamine_update(
    float* __restrict__ concentration,
    float* __restrict__ vesicle_pool,
    const float* __restrict__ spikes,
    float baseline,
    float release_rate,
    float reuptake_tau,
    float decay_tau,
    float max_conc,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float da = concentration[idx];
    float vesicles = vesicle_pool[idx];
    float spike = spikes[idx];

    // Release from vesicle pool
    float released = release_rate * spike * vesicles;
    vesicles -= released;

    // Vesicle replenishment (first-order)
    float replenish = (1.0f - vesicles) * 0.01f * dt;
    vesicles = fminf(1.0f, vesicles + replenish);

    // Update concentration
    float reuptake = (da - baseline) / reuptake_tau;
    float decay = da / decay_tau;

    da += (released - reuptake - decay) * dt;
    da = fmaxf(0.0f, fminf(max_conc, da));

    concentration[idx] = da;
    vesicle_pool[idx] = vesicles;
}

bool nimcp_gpu_dopamine_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_dopamine_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_dopamine_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !spikes || !params) {
        LOG_ERROR("Invalid parameters for dopamine update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_dopamine_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        (float*)state->vesicle_pool->data,
        (const float*)spikes->data,
        params->baseline,
        params->release_rate,
        params->reuptake_tau,
        params->decay_tau,
        params->max_conc,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute reward prediction error
 *
 * RPE = reward - predicted + DA_baseline_deviation
 */
__global__ void kernel_dopamine_rpe(
    float* __restrict__ rpe,
    const float* __restrict__ reward,
    const float* __restrict__ predicted,
    const float* __restrict__ da_conc,
    float baseline,
    float phasic_amplitude,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float r = reward[idx];
    float p = predicted[idx];
    float da = da_conc[idx];

    // TD-like RPE computation
    float temporal_diff = r - p;

    // DA deviation from baseline indicates RPE encoding
    float da_deviation = (da - baseline) / baseline;

    // Combined RPE signal
    rpe[idx] = temporal_diff * phasic_amplitude + da_deviation;
}

bool nimcp_gpu_dopamine_compute_rpe(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_dopamine_state_t* state,
    const nimcp_gpu_tensor_t* reward,
    const nimcp_gpu_tensor_t* predicted,
    nimcp_gpu_tensor_t* rpe_out,
    const nimcp_gpu_dopamine_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !reward || !predicted || !rpe_out || !params) {
        LOG_ERROR("Invalid parameters for RPE computation");
        return false;
    }

    size_t n = reward->numel;

    kernel_dopamine_rpe<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)rpe_out->data,
        (const float*)reward->data,
        (const float*)predicted->data,
        (const float*)state->concentration->data,
        params->baseline,
        params->phasic_amplitude,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to update D1/D2 receptor occupancy
 *
 * Uses Hill equation: occupancy = [DA]^n / (Kd^n + [DA]^n)
 */
__global__ void kernel_dopamine_receptor_update(
    float* __restrict__ d1_occupancy,
    float* __restrict__ d2_occupancy,
    const float* __restrict__ da_conc,
    float d1_affinity,
    float d2_affinity,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float da = da_conc[idx];

    // Hill equation (n=1 for simple binding)
    float d1_eq = da / (d1_affinity + da);
    float d2_eq = da / (d2_affinity + da);

    // Receptor kinetics with time constant
    float tau_receptor = 10.0f; // 10ms receptor kinetics
    float alpha = dt / (tau_receptor + dt);

    d1_occupancy[idx] = d1_occupancy[idx] * (1.0f - alpha) + d1_eq * alpha;
    d2_occupancy[idx] = d2_occupancy[idx] * (1.0f - alpha) + d2_eq * alpha;
}

bool nimcp_gpu_dopamine_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_dopamine_state_t* state,
    float dt,
    const nimcp_gpu_dopamine_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for receptor update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_dopamine_receptor_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->d1_occupancy->data,
        (float*)state->d2_occupancy->data,
        (const float*)state->concentration->data,
        params->d1_affinity,
        params->d2_affinity,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to modulate plasticity via dopamine
 *
 * D1: enhances LTP (direct pathway)
 * D2: enhances LTD (indirect pathway)
 */
__global__ void kernel_dopamine_modulate_plasticity(
    float* __restrict__ weights,
    const float* __restrict__ d1_effect,
    const float* __restrict__ d2_effect,
    const float* __restrict__ eligibility,
    float learning_rate,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float w = weights[idx];
    float d1 = d1_effect[idx];
    float d2 = d2_effect[idx];
    float elig = eligibility[idx];

    // D1 enhances potentiation, D2 enhances depression
    // Net effect depends on receptor occupancy difference
    float d1_mod = (1.0f + d1);  // LTP enhancement
    float d2_mod = (1.0f - d2);  // LTD via reduced potentiation

    float dw = learning_rate * elig * d1_mod * d2_mod;

    w += dw;
    w = fmaxf(0.0f, fminf(1.0f, w));
    weights[idx] = w;
}

bool nimcp_gpu_dopamine_modulate_plasticity(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* d1_effect,
    const nimcp_gpu_tensor_t* d2_effect,
    const nimcp_gpu_tensor_t* eligibility,
    float learning_rate)
{
    if (!ctx || !weights || !d1_effect || !d2_effect || !eligibility) {
        LOG_ERROR("Invalid parameters for DA plasticity modulation");
        return false;
    }

    size_t n = weights->numel;

    kernel_dopamine_modulate_plasticity<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)weights->data,
        (const float*)d1_effect->data,
        (const float*)d2_effect->data,
        (const float*)eligibility->data,
        learning_rate,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Serotonin Kernels
//=============================================================================

/**
 * @brief Kernel to update serotonin concentration
 */
__global__ void kernel_serotonin_update(
    float* __restrict__ concentration,
    float* __restrict__ vesicle_pool,
    const float* __restrict__ spikes,
    float baseline,
    float release_rate,
    float reuptake_tau,
    float decay_tau,
    float autoreceptor_gain,
    float max_conc,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ht = concentration[idx];
    float vesicles = vesicle_pool[idx];
    float spike = spikes[idx];

    // Autoreceptor inhibition of release (5-HT1A)
    float autoreceptor_inhibition = 1.0f / (1.0f + autoreceptor_gain * ht);

    // Release with autoreceptor modulation
    float released = release_rate * spike * vesicles * autoreceptor_inhibition;
    vesicles -= released;

    // Vesicle replenishment
    float replenish = (1.0f - vesicles) * 0.005f * dt; // Slower than DA
    vesicles = fminf(1.0f, vesicles + replenish);

    // SERT reuptake and decay
    float reuptake = (ht - baseline) / reuptake_tau;
    float decay = ht / decay_tau;

    ht += (released - reuptake - decay) * dt;
    ht = fmaxf(0.0f, fminf(max_conc, ht));

    concentration[idx] = ht;
    vesicle_pool[idx] = vesicles;
}

bool nimcp_gpu_serotonin_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_serotonin_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_serotonin_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !spikes || !params) {
        LOG_ERROR("Invalid parameters for serotonin update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_serotonin_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        (float*)state->vesicle_pool->data,
        (const float*)spikes->data,
        params->baseline,
        params->release_rate,
        params->reuptake_tau,
        params->decay_tau,
        params->autoreceptor_gain,
        params->max_conc,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to update 5-HT receptor occupancy
 */
__global__ void kernel_serotonin_receptor_update(
    float* __restrict__ ht1a_occupancy,
    float* __restrict__ ht2a_occupancy,
    const float* __restrict__ ht_conc,
    float ht1a_affinity,
    float ht2a_affinity,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ht = ht_conc[idx];

    // Hill equation
    float ht1a_eq = ht / (ht1a_affinity + ht);
    float ht2a_eq = ht / (ht2a_affinity + ht);

    // Receptor kinetics
    float tau_receptor = 20.0f;
    float alpha = dt / (tau_receptor + dt);

    ht1a_occupancy[idx] = ht1a_occupancy[idx] * (1.0f - alpha) + ht1a_eq * alpha;
    ht2a_occupancy[idx] = ht2a_occupancy[idx] * (1.0f - alpha) + ht2a_eq * alpha;
}

bool nimcp_gpu_serotonin_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_serotonin_state_t* state,
    float dt,
    const nimcp_gpu_serotonin_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for serotonin receptor update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_serotonin_receptor_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->ht1a_occupancy->data,
        (float*)state->ht2a_occupancy->data,
        (const float*)state->concentration->data,
        params->ht1a_affinity,
        params->ht2a_affinity,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for serotonergic behavioral modulation
 *
 * 5-HT1A: inhibitory, reduces impulsivity
 * 5-HT2A: excitatory, affects mood/perception
 */
__global__ void kernel_serotonin_modulate_behavior(
    float* __restrict__ impulse_control,
    float* __restrict__ mood_signal,
    const float* __restrict__ ht1a_occupancy,
    const float* __restrict__ ht2a_occupancy,
    const float* __restrict__ ht_conc,
    float baseline,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ht1a = ht1a_occupancy[idx];
    float ht2a = ht2a_occupancy[idx];
    float ht = ht_conc[idx];

    // 5-HT1A activation enhances impulse control
    // Higher occupancy = better inhibitory control
    impulse_control[idx] = 0.5f + 0.5f * ht1a;

    // Mood signal from overall 5-HT tone and 5-HT2A
    // Low 5-HT = negative mood, high = positive (simplified)
    float ht_normalized = (ht - baseline) / (baseline + 0.01f);
    mood_signal[idx] = tanhf(ht_normalized + 0.5f * ht2a);
}

bool nimcp_gpu_serotonin_modulate_behavior(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_serotonin_state_t* state,
    nimcp_gpu_tensor_t* impulse_control,
    nimcp_gpu_tensor_t* mood_signal,
    const nimcp_gpu_serotonin_params_t* params)
{
    if (!ctx || !state || !impulse_control || !mood_signal || !params) {
        LOG_ERROR("Invalid parameters for behavior modulation");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_serotonin_modulate_behavior<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)impulse_control->data,
        (float*)mood_signal->data,
        (const float*)state->ht1a_occupancy->data,
        (const float*)state->ht2a_occupancy->data,
        (const float*)state->concentration->data,
        params->baseline,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Acetylcholine Kernels
//=============================================================================

/**
 * @brief Kernel to update acetylcholine concentration
 *
 * Very fast dynamics due to AChE hydrolysis
 */
__global__ void kernel_acetylcholine_update(
    float* __restrict__ concentration,
    float* __restrict__ vesicle_pool,
    const float* __restrict__ spikes,
    float baseline,
    float release_rate,
    float ache_rate,
    float decay_tau,
    float max_conc,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ach = concentration[idx];
    float vesicles = vesicle_pool[idx];
    float spike = spikes[idx];

    // Release from vesicle pool
    float released = release_rate * spike * vesicles;
    vesicles -= released;

    // Fast vesicle replenishment for ACh
    float replenish = (1.0f - vesicles) * 0.02f * dt;
    vesicles = fminf(1.0f, vesicles + replenish);

    // AChE hydrolysis (very fast, enzymatic)
    float hydrolysis = ache_rate * ach;

    // Diffusion decay
    float decay = ach / decay_tau;

    ach += (released - hydrolysis - decay) * dt;
    ach = fmaxf(0.0f, fminf(max_conc, ach));

    concentration[idx] = ach;
    vesicle_pool[idx] = vesicles;
}

bool nimcp_gpu_acetylcholine_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_acetylcholine_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !spikes || !params) {
        LOG_ERROR("Invalid parameters for ACh update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_acetylcholine_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        (float*)state->vesicle_pool->data,
        (const float*)spikes->data,
        params->baseline,
        params->release_rate,
        params->ache_rate,
        params->decay_tau,
        params->max_conc,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to update cholinergic receptor states
 */
__global__ void kernel_acetylcholine_receptor_update(
    float* __restrict__ m1_occupancy,
    float* __restrict__ m2_occupancy,
    float* __restrict__ nicotinic_state,
    const float* __restrict__ ach_conc,
    float m1_affinity,
    float m2_affinity,
    float nicotinic_affinity,
    float desensitization_tau,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ach = ach_conc[idx];

    // Muscarinic receptors (metabotropic, slower)
    float m1_eq = ach / (m1_affinity + ach);
    float m2_eq = ach / (m2_affinity + ach);

    float tau_musc = 50.0f;
    float alpha_musc = dt / (tau_musc + dt);

    m1_occupancy[idx] = m1_occupancy[idx] * (1.0f - alpha_musc) + m1_eq * alpha_musc;
    m2_occupancy[idx] = m2_occupancy[idx] * (1.0f - alpha_musc) + m2_eq * alpha_musc;

    // Nicotinic receptors (ionotropic, fast with desensitization)
    float nic_eq = ach / (nicotinic_affinity + ach);
    float nic_state = nicotinic_state[idx];

    // Fast activation, slow desensitization
    float tau_nic = 5.0f;
    float alpha_nic = dt / (tau_nic + dt);

    // Desensitization: high ACh reduces effective response
    float desens_factor = expf(-ach * dt / desensitization_tau);

    nic_state = nic_state * desens_factor * (1.0f - alpha_nic) + nic_eq * alpha_nic;
    nicotinic_state[idx] = nic_state;
}

bool nimcp_gpu_acetylcholine_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state,
    float dt,
    const nimcp_gpu_acetylcholine_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for ACh receptor update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_acetylcholine_receptor_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->m1_occupancy->data,
        (float*)state->m2_occupancy->data,
        (float*)state->nicotinic_state->data,
        (const float*)state->concentration->data,
        params->m1_affinity,
        params->m2_affinity,
        params->nicotinic_affinity,
        params->desensitization_tau,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute attention modulation from ACh
 *
 * ACh enhances signal-to-noise ratio in cortex
 * M1 activation enhances attention to salient stimuli
 */
__global__ void kernel_acetylcholine_attention(
    float* __restrict__ attention_out,
    const float* __restrict__ salience,
    const float* __restrict__ m1_occupancy,
    const float* __restrict__ ach_conc,
    float baseline,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float sal = salience[idx];
    float m1 = m1_occupancy[idx];
    float ach = ach_conc[idx];

    // ACh increases gain for salient stimuli
    float ach_gain = 1.0f + (ach - baseline) / (baseline + 0.01f);
    ach_gain = fmaxf(0.5f, fminf(2.0f, ach_gain));

    // M1 receptors modulate attention focus
    float m1_attention = 0.5f + 0.5f * m1;

    // Combined attention signal
    attention_out[idx] = sal * ach_gain * m1_attention;
}

bool nimcp_gpu_acetylcholine_compute_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state,
    const nimcp_gpu_tensor_t* salience,
    nimcp_gpu_tensor_t* attention_out,
    const nimcp_gpu_acetylcholine_params_t* params)
{
    if (!ctx || !state || !salience || !attention_out || !params) {
        LOG_ERROR("Invalid parameters for attention computation");
        return false;
    }

    size_t n = salience->numel;

    kernel_acetylcholine_attention<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)attention_out->data,
        (const float*)salience->data,
        (const float*)state->m1_occupancy->data,
        (const float*)state->concentration->data,
        params->baseline,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to modulate learning rate based on ACh
 */
__global__ void kernel_acetylcholine_modulate_learning(
    float* __restrict__ learning_rates,
    const float* __restrict__ ach_concentration,
    float baseline_rate,
    float max_modulation,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ach = ach_concentration[idx];

    // Higher ACh = enhanced plasticity
    // ACh promotes encoding of new information
    float ach_factor = 1.0f + max_modulation * ach;

    learning_rates[idx] = baseline_rate * ach_factor;
}

bool nimcp_gpu_acetylcholine_modulate_learning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* learning_rates,
    const nimcp_gpu_tensor_t* ach_concentration,
    float baseline_rate,
    float max_modulation)
{
    if (!ctx || !learning_rates || !ach_concentration) {
        LOG_ERROR("Invalid parameters for ACh learning modulation");
        return false;
    }

    size_t n = learning_rates->numel;

    kernel_acetylcholine_modulate_learning<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)learning_rates->data,
        (const float*)ach_concentration->data,
        baseline_rate,
        max_modulation,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Norepinephrine Kernels
//=============================================================================

/**
 * @brief Kernel to update norepinephrine concentration
 */
__global__ void kernel_norepinephrine_update(
    float* __restrict__ concentration,
    float* __restrict__ vesicle_pool,
    const float* __restrict__ spikes,
    const float* __restrict__ alpha2_occupancy,
    float baseline,
    float release_rate,
    float reuptake_tau,
    float decay_tau,
    float max_conc,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ne = concentration[idx];
    float vesicles = vesicle_pool[idx];
    float spike = spikes[idx];
    float alpha2 = alpha2_occupancy[idx];

    // Alpha-2 autoreceptor inhibition of release
    float autoreceptor_inhibition = 1.0f / (1.0f + alpha2);

    // Release with autoreceptor modulation
    float released = release_rate * spike * vesicles * autoreceptor_inhibition;
    vesicles -= released;

    // Vesicle replenishment
    float replenish = (1.0f - vesicles) * 0.008f * dt;
    vesicles = fminf(1.0f, vesicles + replenish);

    // NET reuptake and decay
    float reuptake = (ne - baseline) / reuptake_tau;
    float decay = ne / decay_tau;

    ne += (released - reuptake - decay) * dt;
    ne = fmaxf(0.0f, fminf(max_conc, ne));

    concentration[idx] = ne;
    vesicle_pool[idx] = vesicles;
}

bool nimcp_gpu_norepinephrine_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_norepinephrine_params_t* params)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !spikes || !params) {
        LOG_ERROR("Invalid parameters for NE update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_norepinephrine_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        (float*)state->vesicle_pool->data,
        (const float*)spikes->data,
        (const float*)state->alpha2_occupancy->data,
        params->baseline,
        params->release_rate,
        params->reuptake_tau,
        params->decay_tau,
        params->max_conc,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to update adrenergic receptor occupancy
 */
__global__ void kernel_norepinephrine_receptor_update(
    float* __restrict__ alpha1_occupancy,
    float* __restrict__ alpha2_occupancy,
    float* __restrict__ beta_occupancy,
    const float* __restrict__ ne_conc,
    float alpha1_affinity,
    float alpha2_affinity,
    float beta_affinity,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ne = ne_conc[idx];

    // Hill equation for each receptor type
    float a1_eq = ne / (alpha1_affinity + ne);
    float a2_eq = ne / (alpha2_affinity + ne);
    float b_eq = ne / (beta_affinity + ne);

    // Receptor kinetics
    float tau_receptor = 15.0f;
    float alpha = dt / (tau_receptor + dt);

    alpha1_occupancy[idx] = alpha1_occupancy[idx] * (1.0f - alpha) + a1_eq * alpha;
    alpha2_occupancy[idx] = alpha2_occupancy[idx] * (1.0f - alpha) + a2_eq * alpha;
    beta_occupancy[idx] = beta_occupancy[idx] * (1.0f - alpha) + b_eq * alpha;
}

bool nimcp_gpu_norepinephrine_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state,
    float dt,
    const nimcp_gpu_norepinephrine_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for NE receptor update");
        return false;
    }

    size_t n = state->concentration->numel;

    kernel_norepinephrine_receptor_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->alpha1_occupancy->data,
        (float*)state->alpha2_occupancy->data,
        (float*)state->beta_occupancy->data,
        (const float*)state->concentration->data,
        params->alpha1_affinity,
        params->alpha2_affinity,
        params->beta_affinity,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to compute arousal/vigilance signal
 *
 * NE modulates arousal via LC projections
 * Beta receptors increase arousal
 * Alpha-1 enhances vigilance
 */
__global__ void kernel_norepinephrine_arousal(
    float* __restrict__ arousal_out,
    const float* __restrict__ alpha1_occupancy,
    const float* __restrict__ beta_occupancy,
    const float* __restrict__ stress_input,
    const float* __restrict__ ne_conc,
    float baseline,
    float stress_gain,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float a1 = alpha1_occupancy[idx];
    float beta = beta_occupancy[idx];
    float stress = stress_input[idx];
    float ne = ne_conc[idx];

    // Base arousal from NE tone
    float ne_normalized = (ne - baseline) / (baseline + 0.01f);

    // Beta receptor contribution (general arousal)
    float beta_arousal = beta * 0.6f;

    // Alpha-1 contribution (vigilance/alertness)
    float a1_vigilance = a1 * 0.4f;

    // Stress amplification
    float stress_factor = 1.0f + stress_gain * stress;

    // Combined arousal signal
    float arousal = (ne_normalized + beta_arousal + a1_vigilance) * stress_factor;
    arousal = fmaxf(0.0f, fminf(2.0f, arousal));

    arousal_out[idx] = arousal;
}

bool nimcp_gpu_norepinephrine_compute_arousal(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state,
    const nimcp_gpu_tensor_t* stress_input,
    nimcp_gpu_tensor_t* arousal_out,
    const nimcp_gpu_norepinephrine_params_t* params)
{
    if (!ctx || !state || !stress_input || !arousal_out || !params) {
        LOG_ERROR("Invalid parameters for arousal computation");
        return false;
    }

    size_t n = stress_input->numel;

    kernel_norepinephrine_arousal<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)arousal_out->data,
        (const float*)state->alpha1_occupancy->data,
        (const float*)state->beta_occupancy->data,
        (const float*)stress_input->data,
        (const float*)state->concentration->data,
        params->baseline,
        params->stress_gain,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for NE gain modulation (Yerkes-Dodson)
 *
 * Optimal arousal = peak performance
 * Too low or too high = impaired function
 */
__global__ void kernel_norepinephrine_modulate_gain(
    float* __restrict__ neural_gains,
    const float* __restrict__ ne_concentration,
    float optimal_arousal,
    float gain_sensitivity,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float ne = ne_concentration[idx];

    // Yerkes-Dodson inverted U curve
    // Peak performance at optimal_arousal level
    float deviation = fabsf(ne - optimal_arousal);
    float gain = expf(-gain_sensitivity * deviation * deviation);

    // Base gain of 1.0, modulated by NE
    neural_gains[idx] = 0.5f + 0.5f * gain;
}

bool nimcp_gpu_norepinephrine_modulate_gain(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* neural_gains,
    const nimcp_gpu_tensor_t* ne_concentration,
    float optimal_arousal,
    float gain_sensitivity)
{
    if (!ctx || !neural_gains || !ne_concentration) {
        LOG_ERROR("Invalid parameters for NE gain modulation");
        return false;
    }

    size_t n = neural_gains->numel;

    kernel_norepinephrine_modulate_gain<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)neural_gains->data,
        (const float*)ne_concentration->data,
        optimal_arousal,
        gain_sensitivity,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Integrated System Kernels
//=============================================================================

/**
 * @brief Kernel for cross-modulator interactions
 *
 * Models reciprocal influences between neuromodulator systems
 */
__global__ void kernel_neuromod_interactions(
    float* __restrict__ da_mod,
    float* __restrict__ ht_mod,
    float* __restrict__ ach_mod,
    float* __restrict__ ne_mod,
    const float* __restrict__ da_conc,
    const float* __restrict__ ht_conc,
    const float* __restrict__ ach_conc,
    const float* __restrict__ ne_conc,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float da = da_conc[idx];
    float ht = ht_conc[idx];
    float ach = ach_conc[idx];
    float ne = ne_conc[idx];

    // DA-5HT interactions (mutual inhibition in some circuits)
    float da_ht_interaction = -0.2f * ht;  // 5-HT inhibits DA
    float ht_da_interaction = -0.1f * da;  // DA inhibits 5-HT

    // ACh-DA interactions (VTA cholinergic modulation)
    float da_ach_interaction = 0.3f * ach; // ACh enhances DA

    // NE-ACh interactions (LC-BF coupling)
    float ach_ne_interaction = 0.2f * ne;  // NE enhances ACh release

    // NE-DA interactions (stress-reward coupling)
    float da_ne_interaction = 0.15f * ne;  // NE modulates DA
    float ne_da_interaction = 0.1f * da;   // DA modulates NE

    // Apply modulation factors
    da_mod[idx] = 1.0f + da_ht_interaction + da_ach_interaction + da_ne_interaction;
    ht_mod[idx] = 1.0f + ht_da_interaction;
    ach_mod[idx] = 1.0f + ach_ne_interaction;
    ne_mod[idx] = 1.0f + ne_da_interaction;

    // Clamp modulation factors
    da_mod[idx] = fmaxf(0.1f, fminf(3.0f, da_mod[idx]));
    ht_mod[idx] = fmaxf(0.1f, fminf(3.0f, ht_mod[idx]));
    ach_mod[idx] = fmaxf(0.1f, fminf(3.0f, ach_mod[idx]));
    ne_mod[idx] = fmaxf(0.1f, fminf(3.0f, ne_mod[idx]));
}

bool nimcp_gpu_neuromod_interactions(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_neuromod_system_t* system)
{
    if (!ctx || !system) {
        LOG_ERROR("Invalid parameters for neuromod interactions");
        return false;
    }

    // Check all subsystems exist
    if (!system->dopamine || !system->serotonin ||
        !system->acetylcholine || !system->norepinephrine) {
        LOG_ERROR("Incomplete neuromodulator system");
        return false;
    }

    size_t n = system->dopamine->concentration->numel;

    // Allocate interaction matrix if needed
    if (!system->interaction_matrix) {
        LOG_WARN("Interaction matrix not allocated, skipping interactions");
        return true;
    }

    // For now, apply interactions in-place to concentration effects
    // In a full implementation, this would update the interaction matrix

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_neuromod_system_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_neuromod_system_t* system,
    const nimcp_gpu_tensor_t* da_spikes,
    const nimcp_gpu_tensor_t* ht_spikes,
    const nimcp_gpu_tensor_t* ach_spikes,
    const nimcp_gpu_tensor_t* ne_spikes,
    float dt)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !system) {
        LOG_ERROR("Invalid system update parameters");
        return false;
    }

    bool success = true;

    // Update individual systems
    if (system->dopamine && da_spikes) {
        nimcp_gpu_dopamine_params_t da_params = nimcp_gpu_dopamine_params_default();
        success &= nimcp_gpu_dopamine_update(ctx, system->dopamine, da_spikes, dt, &da_params);
        success &= nimcp_gpu_dopamine_receptor_update(ctx, system->dopamine, dt, &da_params);
    }

    if (system->serotonin && ht_spikes) {
        nimcp_gpu_serotonin_params_t ht_params = nimcp_gpu_serotonin_params_default();
        success &= nimcp_gpu_serotonin_update(ctx, system->serotonin, ht_spikes, dt, &ht_params);
        success &= nimcp_gpu_serotonin_receptor_update(ctx, system->serotonin, dt, &ht_params);
    }

    if (system->acetylcholine && ach_spikes) {
        nimcp_gpu_acetylcholine_params_t ach_params = nimcp_gpu_acetylcholine_params_default();
        success &= nimcp_gpu_acetylcholine_update(ctx, system->acetylcholine, ach_spikes, dt, &ach_params);
        success &= nimcp_gpu_acetylcholine_receptor_update(ctx, system->acetylcholine, dt, &ach_params);
    }

    if (system->norepinephrine && ne_spikes) {
        nimcp_gpu_norepinephrine_params_t ne_params = nimcp_gpu_norepinephrine_params_default();
        success &= nimcp_gpu_norepinephrine_update(ctx, system->norepinephrine, ne_spikes, dt, &ne_params);
        success &= nimcp_gpu_norepinephrine_receptor_update(ctx, system->norepinephrine, dt, &ne_params);
    }

    // Update cross-modulator interactions
    success &= nimcp_gpu_neuromod_interactions(ctx, system);

    return success;
}

/**
 * @brief Kernel to apply combined neuromodulator effects
 */
__global__ void kernel_neuromod_apply_combined(
    float* __restrict__ output,
    const float* __restrict__ input,
    const float* __restrict__ da_conc,
    const float* __restrict__ ht_conc,
    const float* __restrict__ ach_conc,
    const float* __restrict__ ne_conc,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float x = input[idx];
    float da = da_conc[idx];
    float ht = ht_conc[idx];
    float ach = ach_conc[idx];
    float ne = ne_conc[idx];

    // DA: increases gain for rewarded actions
    float da_factor = 1.0f + 0.5f * da;

    // 5-HT: normalizing/calming effect
    float ht_factor = 1.0f / (1.0f + 0.5f * ht);

    // ACh: signal enhancement (attention)
    float ach_factor = 1.0f + 0.3f * ach;

    // NE: overall gain modulation
    float ne_factor = 0.8f + 0.4f * ne;

    // Combined effect
    output[idx] = x * da_factor * ht_factor * ach_factor * ne_factor;
}

bool nimcp_gpu_neuromod_apply_combined(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_neuromod_system_t* system,
    const nimcp_gpu_tensor_t* target_activity,
    nimcp_gpu_tensor_t* modulated_output)
{
    if (!ctx || !system || !target_activity || !modulated_output) {
        LOG_ERROR("Invalid parameters for combined neuromod application");
        return false;
    }

    if (!system->dopamine || !system->serotonin ||
        !system->acetylcholine || !system->norepinephrine) {
        LOG_ERROR("Incomplete neuromodulator system");
        return false;
    }

    size_t n = target_activity->numel;

    kernel_neuromod_apply_combined<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)modulated_output->data,
        (const float*)target_activity->data,
        (const float*)system->dopamine->concentration->data,
        (const float*)system->serotonin->concentration->data,
        (const float*)system->acetylcholine->concentration->data,
        (const float*)system->norepinephrine->concentration->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Vesicle and Receptor Dynamics (Generic)
//=============================================================================

/**
 * @brief Kernel for generic vesicle dynamics
 */
__global__ void kernel_vesicle_dynamics(
    float* __restrict__ vesicle_pool,
    const float* __restrict__ spikes,
    float release_prob,
    float replenish_rate,
    float max_pool,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float pool = vesicle_pool[idx];
    float spike = spikes[idx];

    // Release based on spike and probability
    float release = release_prob * spike * pool;
    pool -= release;

    // Replenishment toward max_pool
    float replenish = replenish_rate * (max_pool - pool) * dt;
    pool += replenish;

    // Clamp
    pool = fmaxf(0.0f, fminf(max_pool, pool));

    vesicle_pool[idx] = pool;
}

bool nimcp_gpu_vesicle_dynamics(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* vesicle_pool,
    const nimcp_gpu_tensor_t* spikes,
    float release_prob,
    float replenish_rate,
    float max_pool,
    float dt)
{
    if (!ctx || !vesicle_pool || !spikes) {
        LOG_ERROR("Invalid parameters for vesicle dynamics");
        return false;
    }

    size_t n = vesicle_pool->numel;

    kernel_vesicle_dynamics<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)vesicle_pool->data,
        (const float*)spikes->data,
        release_prob,
        replenish_rate,
        max_pool,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for generic receptor binding kinetics
 */
__global__ void kernel_receptor_kinetics(
    float* __restrict__ occupancy,
    const float* __restrict__ concentration,
    float affinity,
    float on_rate,
    float off_rate,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float occ = occupancy[idx];
    float conc = concentration[idx];

    // Binding: kon * [L] * (1 - occupancy)
    float binding = on_rate * conc * (1.0f - occ);

    // Unbinding: koff * occupancy
    float unbinding = off_rate * occ;

    // Update occupancy
    occ += (binding - unbinding) * dt;
    occ = fmaxf(0.0f, fminf(1.0f, occ));

    occupancy[idx] = occ;
}

bool nimcp_gpu_receptor_kinetics(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* occupancy,
    const nimcp_gpu_tensor_t* concentration,
    float affinity,
    float on_rate,
    float off_rate,
    float dt)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !occupancy || !concentration) {
        LOG_ERROR("Invalid parameters for receptor kinetics");
        return false;
    }

    size_t n = occupancy->numel;

    kernel_receptor_kinetics<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)occupancy->data,
        (const float*)concentration->data,
        affinity,
        on_rate,
        off_rate,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

// Stub implementations when CUDA is not available
#include "gpu/neuromodulators/nimcp_neuromodulator_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "NEUROMODULATOR_GPU"

nimcp_gpu_dopamine_params_t nimcp_gpu_dopamine_params_default(void)
{
    nimcp_gpu_dopamine_params_t params = {0};
    params.baseline = 0.05f;
    params.release_rate = 0.1f;
    params.reuptake_tau = 50.0f;
    params.decay_tau = 200.0f;
    params.d1_affinity = 0.5f;
    params.d2_affinity = 0.02f;
    params.max_conc = 2.0f;
    params.burst_factor = 3.0f;
    params.tonic_rate = 4.0f;
    params.phasic_amplitude = 0.5f;
    return params;
}

nimcp_gpu_serotonin_params_t nimcp_gpu_serotonin_params_default(void)
{
    nimcp_gpu_serotonin_params_t params = {0};
    params.baseline = 0.01f;
    params.release_rate = 0.05f;
    params.reuptake_tau = 100.0f;
    params.decay_tau = 300.0f;
    params.ht1a_affinity = 0.001f;
    params.ht2a_affinity = 0.01f;
    params.max_conc = 0.5f;
    params.autoreceptor_gain = 0.5f;
    params.synthesis_rate = 0.001f;
    return params;
}

nimcp_gpu_acetylcholine_params_t nimcp_gpu_acetylcholine_params_default(void)
{
    nimcp_gpu_acetylcholine_params_t params = {0};
    params.baseline = 0.02f;
    params.release_rate = 0.2f;
    params.ache_rate = 0.5f;
    params.decay_tau = 20.0f;
    params.m1_affinity = 0.01f;
    params.m2_affinity = 0.005f;
    params.nicotinic_affinity = 0.1f;
    params.max_conc = 1.0f;
    params.desensitization_tau = 500.0f;
    params.choline_uptake_km = 0.1f;
    return params;
}

nimcp_gpu_norepinephrine_params_t nimcp_gpu_norepinephrine_params_default(void)
{
    nimcp_gpu_norepinephrine_params_t params = {0};
    params.baseline = 0.02f;
    params.release_rate = 0.08f;
    params.reuptake_tau = 80.0f;
    params.decay_tau = 250.0f;
    params.alpha1_affinity = 0.1f;
    params.alpha2_affinity = 0.01f;
    params.beta_affinity = 0.05f;
    params.max_conc = 1.0f;
    params.lc_baseline_rate = 2.0f;
    params.stress_gain = 2.0f;
    return params;
}

bool nimcp_gpu_dopamine_update(nimcp_gpu_context_t* ctx, nimcp_gpu_dopamine_state_t* state,
    const nimcp_gpu_tensor_t* spikes, float dt, const nimcp_gpu_dopamine_params_t* params)
{
    LOG_WARN("CUDA not enabled - dopamine update unavailable");
    return false;
}

bool nimcp_gpu_dopamine_compute_rpe(nimcp_gpu_context_t* ctx, nimcp_gpu_dopamine_state_t* state,
    const nimcp_gpu_tensor_t* reward, const nimcp_gpu_tensor_t* predicted,
    nimcp_gpu_tensor_t* rpe_out, const nimcp_gpu_dopamine_params_t* params)
{
    LOG_WARN("CUDA not enabled - RPE computation unavailable");
    return false;
}

bool nimcp_gpu_dopamine_receptor_update(nimcp_gpu_context_t* ctx, nimcp_gpu_dopamine_state_t* state,
    float dt, const nimcp_gpu_dopamine_params_t* params)
{
    LOG_WARN("CUDA not enabled - receptor update unavailable");
    return false;
}

bool nimcp_gpu_dopamine_modulate_plasticity(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* d1_effect, const nimcp_gpu_tensor_t* d2_effect,
    const nimcp_gpu_tensor_t* eligibility, float learning_rate)
{
    LOG_WARN("CUDA not enabled - plasticity modulation unavailable");
    return false;
}

bool nimcp_gpu_serotonin_update(nimcp_gpu_context_t* ctx, nimcp_gpu_serotonin_state_t* state,
    const nimcp_gpu_tensor_t* spikes, float dt, const nimcp_gpu_serotonin_params_t* params)
{
    LOG_WARN("CUDA not enabled - serotonin update unavailable");
    return false;
}

bool nimcp_gpu_serotonin_receptor_update(nimcp_gpu_context_t* ctx, nimcp_gpu_serotonin_state_t* state,
    float dt, const nimcp_gpu_serotonin_params_t* params)
{
    LOG_WARN("CUDA not enabled - serotonin receptor update unavailable");
    return false;
}

bool nimcp_gpu_serotonin_modulate_behavior(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_serotonin_state_t* state, nimcp_gpu_tensor_t* impulse_control,
    nimcp_gpu_tensor_t* mood_signal, const nimcp_gpu_serotonin_params_t* params)
{
    LOG_WARN("CUDA not enabled - behavior modulation unavailable");
    return false;
}

bool nimcp_gpu_acetylcholine_update(nimcp_gpu_context_t* ctx, nimcp_gpu_acetylcholine_state_t* state,
    const nimcp_gpu_tensor_t* spikes, float dt, const nimcp_gpu_acetylcholine_params_t* params)
{
    LOG_WARN("CUDA not enabled - ACh update unavailable");
    return false;
}

bool nimcp_gpu_acetylcholine_receptor_update(nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state, float dt, const nimcp_gpu_acetylcholine_params_t* params)
{
    LOG_WARN("CUDA not enabled - ACh receptor update unavailable");
    return false;
}

bool nimcp_gpu_acetylcholine_compute_attention(nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state, const nimcp_gpu_tensor_t* salience,
    nimcp_gpu_tensor_t* attention_out, const nimcp_gpu_acetylcholine_params_t* params)
{
    LOG_WARN("CUDA not enabled - attention computation unavailable");
    return false;
}

bool nimcp_gpu_acetylcholine_modulate_learning(nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* learning_rates, const nimcp_gpu_tensor_t* ach_concentration,
    float baseline_rate, float max_modulation)
{
    LOG_WARN("CUDA not enabled - ACh learning modulation unavailable");
    return false;
}

bool nimcp_gpu_norepinephrine_update(nimcp_gpu_context_t* ctx, nimcp_gpu_norepinephrine_state_t* state,
    const nimcp_gpu_tensor_t* spikes, float dt, const nimcp_gpu_norepinephrine_params_t* params)
{
    LOG_WARN("CUDA not enabled - NE update unavailable");
    return false;
}

bool nimcp_gpu_norepinephrine_receptor_update(nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state, float dt, const nimcp_gpu_norepinephrine_params_t* params)
{
    LOG_WARN("CUDA not enabled - NE receptor update unavailable");
    return false;
}

bool nimcp_gpu_norepinephrine_compute_arousal(nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state, const nimcp_gpu_tensor_t* stress_input,
    nimcp_gpu_tensor_t* arousal_out, const nimcp_gpu_norepinephrine_params_t* params)
{
    LOG_WARN("CUDA not enabled - arousal computation unavailable");
    return false;
}

bool nimcp_gpu_norepinephrine_modulate_gain(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* neural_gains,
    const nimcp_gpu_tensor_t* ne_concentration, float optimal_arousal, float gain_sensitivity)
{
    LOG_WARN("CUDA not enabled - NE gain modulation unavailable");
    return false;
}

bool nimcp_gpu_neuromod_system_update(nimcp_gpu_context_t* ctx, nimcp_gpu_neuromod_system_t* system,
    const nimcp_gpu_tensor_t* da_spikes, const nimcp_gpu_tensor_t* ht_spikes,
    const nimcp_gpu_tensor_t* ach_spikes, const nimcp_gpu_tensor_t* ne_spikes, float dt)
{
    LOG_WARN("CUDA not enabled - neuromod system update unavailable");
    return false;
}

bool nimcp_gpu_neuromod_interactions(nimcp_gpu_context_t* ctx, nimcp_gpu_neuromod_system_t* system)
{
    LOG_WARN("CUDA not enabled - neuromod interactions unavailable");
    return false;
}

bool nimcp_gpu_neuromod_apply_combined(nimcp_gpu_context_t* ctx, const nimcp_gpu_neuromod_system_t* system,
    const nimcp_gpu_tensor_t* target_activity, nimcp_gpu_tensor_t* modulated_output)
{
    LOG_WARN("CUDA not enabled - combined neuromod unavailable");
    return false;
}

bool nimcp_gpu_vesicle_dynamics(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* vesicle_pool,
    const nimcp_gpu_tensor_t* spikes, float release_prob, float replenish_rate,
    float max_pool, float dt)
{
    LOG_WARN("CUDA not enabled - vesicle dynamics unavailable");
    return false;
}

bool nimcp_gpu_receptor_kinetics(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* occupancy,
    const nimcp_gpu_tensor_t* concentration, float affinity, float on_rate, float off_rate, float dt)
{
    LOG_WARN("CUDA not enabled - receptor kinetics unavailable");
    return false;
}

#endif // NIMCP_ENABLE_CUDA
