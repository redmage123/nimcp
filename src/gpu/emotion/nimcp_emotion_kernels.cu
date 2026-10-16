/**
 * @file nimcp_emotion_kernels.cu
 * @brief GPU Emotion Processing CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for limbic system and emotional processing
 * WHY:  GPU acceleration for biologically-inspired emotion computation
 * HOW:  Custom kernels for amygdala, OFC, NAcc, ACC
 *
 * ARCHITECTURE:
 * - Amygdala: Fast threat detection, fear conditioning, extinction
 * - OFC: Value computation, reversal learning, decision making
 * - NAcc: Reward prediction, motivation, effort evaluation
 * - ACC: Conflict monitoring, error detection, effort allocation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#include "gpu/emotion/nimcp_emotion_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "EMOTION_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE 32

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_amygdala_params_t nimcp_gpu_amygdala_params_default(void)
{
    nimcp_gpu_amygdala_params_t params;
    params.threat_threshold = 0.3f;
    params.fear_learning_rate = 0.1f;
    params.extinction_rate = 0.01f;
    params.generalization_sigma = 0.2f;
    params.habituation_tau = 1000.0f;
    params.sensitization_tau = 500.0f;
    params.context_weight = 0.3f;
    params.prefrontal_inhibition = 0.5f;
    params.lateral_inhibition = 0.2f;
    params.basal_threshold = 0.4f;
    return params;
}

nimcp_gpu_ofc_params_t nimcp_gpu_ofc_params_default(void)
{
    nimcp_gpu_ofc_params_t params;
    params.value_learning_rate = 0.1f;
    params.discount_factor = 0.95f;
    params.reversal_rate = 0.2f;
    params.comparison_gain = 2.0f;
    params.risk_sensitivity = 1.0f;
    params.satiety_decay = 0.01f;
    params.integration_tau = 100.0f;
    params.outcome_sensitivity = 1.0f;
    return params;
}

nimcp_gpu_nacc_params_t nimcp_gpu_nacc_params_default(void)
{
    nimcp_gpu_nacc_params_t params;
    params.reward_sensitivity = 1.0f;
    params.effort_cost = 0.5f;
    params.delay_discount = 0.1f;
    params.hedonic_baseline = 0.0f;
    params.motivation_decay = 0.05f;
    params.dopamine_gain = 2.0f;
    params.gaba_inhibition = 0.3f;
    params.glutamate_excitation = 1.0f;
    return params;
}

nimcp_gpu_acc_params_t nimcp_gpu_acc_params_default(void)
{
    nimcp_gpu_acc_params_t params;
    params.conflict_threshold = 0.5f;
    params.error_learning_rate = 0.1f;
    params.effort_sensitivity = 1.0f;
    params.prediction_weight = 0.8f;
    params.volatility_estimate = 0.1f;
    params.control_gain = 1.5f;
    return params;
}

//=============================================================================
// Amygdala Kernels
//=============================================================================

/**
 * @brief Kernel for fast threat detection (thalamic pathway)
 *
 * Implements the "low road" - fast, coarse threat detection
 * before cortical processing
 */
__global__ void kernel_amygdala_threat_detection(
    float* __restrict__ threat_out,
    float* __restrict__ lateral_activity,
    const float* __restrict__ sensory_input,
    const float* __restrict__ fear_memory,
    const float* __restrict__ context_gate,
    float threat_threshold,
    float generalization_sigma,
    float context_weight,
    size_t n_stimuli,
    size_t n_contexts)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_stimuli) return;

    float input = sensory_input[idx];
    float memory = fear_memory[idx];

    // Generalization: similar stimuli trigger fear
    // Using Gaussian similarity kernel
    float similarity = expf(-input * input / (2.0f * generalization_sigma * generalization_sigma));

    // Context modulation
    float context_mod = 1.0f;
    if (context_gate != NULL && n_contexts > 0) {
        // Average context gating
        float ctx_sum = 0.0f;
        for (size_t c = 0; c < n_contexts; c++) {
            ctx_sum += context_gate[c];
        }
        context_mod = 1.0f + context_weight * (ctx_sum / n_contexts - 0.5f);
    }

    // Lateral nucleus: initial threat assessment
    float lateral = memory * similarity * context_mod;
    lateral_activity[idx] = lateral;

    // Threshold for threat response
    float threat = (lateral > threat_threshold) ? lateral : 0.0f;
    threat_out[idx] = threat;
}

bool nimcp_gpu_amygdala_threat_detection(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* sensory_input,
    const nimcp_gpu_tensor_t* context,
    nimcp_gpu_tensor_t* threat_out,
    const nimcp_gpu_amygdala_params_t* params)
{
    if (!ctx || !state || !sensory_input || !threat_out || !params) {
        LOG_ERROR("Invalid parameters for threat detection");
        return false;
    }

    size_t n = state->n_stimuli;
    size_t n_ctx = context ? context->numel : 0;

    kernel_amygdala_threat_detection<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)threat_out->data,
        (float*)state->lateral_activity->data,
        (const float*)sensory_input->data,
        (const float*)state->fear_memory->data,
        context ? (const float*)context->data : NULL,
        params->threat_threshold,
        params->generalization_sigma,
        params->context_weight,
        n,
        n_ctx);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for fear conditioning (Rescorla-Wagner learning)
 *
 * Updates CS-US associations based on prediction error
 */
__global__ void kernel_fear_conditioning(
    float* __restrict__ cs_us_associations,
    float* __restrict__ fear_memory,
    const float* __restrict__ cs,
    const float* __restrict__ us,
    float learning_rate,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float cs_val = cs[idx];
    float us_val = us[idx];
    float assoc = cs_us_associations[idx];

    // Prediction: how much fear the CS predicts
    float prediction = assoc * cs_val;

    // Prediction error (US - predicted)
    float pe = us_val - prediction;

    // Rescorla-Wagner update
    float delta = learning_rate * cs_val * pe * dt;
    assoc += delta;

    // Clamp association strength
    assoc = fmaxf(0.0f, fminf(1.0f, assoc));

    cs_us_associations[idx] = assoc;

    // Update consolidated fear memory (slower)
    float memory = fear_memory[idx];
    memory = 0.99f * memory + 0.01f * assoc;
    fear_memory[idx] = memory;
}

bool nimcp_gpu_amygdala_fear_conditioning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* cs,
    const nimcp_gpu_tensor_t* us,
    float dt,
    const nimcp_gpu_amygdala_params_t* params)
{
    if (!ctx || !state || !cs || !us || !params) {
        LOG_ERROR("Invalid parameters for fear conditioning");
        return false;
    }

    size_t n = state->n_stimuli;

    kernel_fear_conditioning<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->cs_us_associations->data,
        (float*)state->fear_memory->data,
        (const float*)cs->data,
        (const float*)us->data,
        params->fear_learning_rate,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for fear extinction
 *
 * New inhibitory learning that suppresses (but doesn't erase) fear
 */
__global__ void kernel_fear_extinction(
    float* __restrict__ extinction_trace,
    float* __restrict__ cs_us_associations,
    const float* __restrict__ cs,
    float extinction_rate,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float cs_val = cs[idx];
    float extinction = extinction_trace[idx];
    float assoc = cs_us_associations[idx];

    // Extinction learning: CS without US
    // Builds inhibitory trace
    float extinction_delta = extinction_rate * cs_val * assoc * dt;
    extinction += extinction_delta;

    // Extinction trace decay (spontaneous recovery)
    extinction *= (1.0f - 0.001f * dt);

    extinction = fmaxf(0.0f, fminf(1.0f, extinction));
    extinction_trace[idx] = extinction;

    // Effective association is reduced by extinction
    // (original memory preserved, but expression suppressed)
}

bool nimcp_gpu_amygdala_extinction(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* cs,
    const nimcp_gpu_tensor_t* no_us,
    float dt,
    const nimcp_gpu_amygdala_params_t* params)
{
    if (!ctx || !state || !cs || !params) {
        LOG_ERROR("Invalid parameters for extinction");
        return false;
    }

    size_t n = state->n_stimuli;

    kernel_fear_extinction<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->extinction_trace->data,
        (float*)state->cs_us_associations->data,
        (const float*)cs->data,
        params->extinction_rate,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for prefrontal inhibition of amygdala
 */
__global__ void kernel_prefrontal_inhibition(
    float* __restrict__ central_output,
    const float* __restrict__ lateral_activity,
    const float* __restrict__ basal_activity,
    const float* __restrict__ pfc_signal,
    float prefrontal_inhibition,
    float basal_threshold,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float lateral = lateral_activity[idx];
    float basal = basal_activity[idx];
    float pfc = pfc_signal[idx];

    // PFC inhibits amygdala output
    float inhibition = prefrontal_inhibition * pfc;

    // Basal nucleus integrates and gates output
    float basal_gate = (basal > basal_threshold) ? 1.0f : basal / basal_threshold;

    // Central nucleus output (final fear response)
    float output = lateral * basal_gate * (1.0f - inhibition);
    output = fmaxf(0.0f, output);

    central_output[idx] = output;
}

bool nimcp_gpu_amygdala_prefrontal_inhibition(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* pfc_signal,
    const nimcp_gpu_amygdala_params_t* params)
{
    if (!ctx || !state || !pfc_signal || !params) {
        LOG_ERROR("Invalid parameters for PFC inhibition");
        return false;
    }

    size_t n = state->n_stimuli;

    kernel_prefrontal_inhibition<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->central_output->data,
        (const float*)state->lateral_activity->data,
        (const float*)state->basal_activity->data,
        (const float*)pfc_signal->data,
        params->prefrontal_inhibition,
        params->basal_threshold,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// OFC Kernels
//=============================================================================

/**
 * @brief Kernel to compute option values
 */
__global__ void kernel_ofc_compute_values(
    float* __restrict__ option_values,
    const float* __restrict__ stimulus_features,
    const float* __restrict__ expected_outcomes,
    const float* __restrict__ satiety_state,
    float discount_factor,
    float satiety_decay,
    size_t n_options,
    size_t n_features)
{
    size_t opt_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (opt_idx >= n_options) return;

    float value = 0.0f;

    // Integrate features weighted by expected outcomes
    for (size_t f = 0; f < n_features; f++) {
        size_t idx = opt_idx * n_features + f;
        float feature = stimulus_features[idx];
        float outcome = expected_outcomes[idx];

        // Satiety reduces value of specific outcomes
        float satiety_mod = 1.0f;
        if (satiety_state != NULL) {
            satiety_mod = expf(-satiety_decay * satiety_state[f]);
        }

        value += feature * outcome * satiety_mod;
    }

    // Temporal discounting for delayed rewards
    value *= discount_factor;

    option_values[opt_idx] = value;
}

bool nimcp_gpu_ofc_compute_values(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* stimulus_features,
    const nimcp_gpu_tensor_t* context,
    const nimcp_gpu_ofc_params_t* params)
{
    if (!ctx || !state || !stimulus_features || !params) {
        LOG_ERROR("Invalid parameters for OFC value computation");
        return false;
    }

    size_t n_options = state->n_options;
    size_t n_features = stimulus_features->numel / n_options;

    kernel_ofc_compute_values<<<GRID_SIZE(n_options), BLOCK_SIZE>>>(
        (float*)state->option_values->data,
        (const float*)stimulus_features->data,
        (const float*)state->expected_outcomes->data,
        state->satiety_state ? (const float*)state->satiety_state->data : NULL,
        params->discount_factor,
        params->satiety_decay,
        n_options,
        n_features);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for value update based on outcome
 */
__global__ void kernel_ofc_value_update(
    float* __restrict__ expected_outcomes,
    float* __restrict__ outcome_history,
    const float* __restrict__ chosen_option,
    const float* __restrict__ outcome,
    float learning_rate,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float choice = chosen_option[idx];
    float actual_outcome = outcome[idx];
    float expected = expected_outcomes[idx];

    // Only update for chosen option
    if (choice > 0.5f) {
        // Prediction error
        float pe = actual_outcome - expected;

        // Update expected outcome
        expected += learning_rate * pe * dt;
        expected = fmaxf(-1.0f, fminf(1.0f, expected));

        expected_outcomes[idx] = expected;

        // Update outcome history (exponential moving average)
        float history = outcome_history[idx];
        history = 0.9f * history + 0.1f * actual_outcome;
        outcome_history[idx] = history;
    }
}

bool nimcp_gpu_ofc_value_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* chosen_option,
    const nimcp_gpu_tensor_t* outcome,
    float dt,
    const nimcp_gpu_ofc_params_t* params)
{
    if (!ctx || !state || !chosen_option || !outcome || !params) {
        LOG_ERROR("Invalid parameters for OFC value update");
        return false;
    }

    size_t n = state->expected_outcomes->numel;

    kernel_ofc_value_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->expected_outcomes->data,
        (float*)state->outcome_history->data,
        (const float*)chosen_option->data,
        (const float*)outcome->data,
        params->value_learning_rate,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for softmax choice probabilities
 */
__global__ void kernel_ofc_softmax(
    float* __restrict__ choice_probs,
    const float* __restrict__ option_values,
    float temperature,
    size_t n_options)
{
    // This kernel computes softmax per batch
    // For simplicity, single block handles all options
    __shared__ float max_val;
    __shared__ float sum_exp;

    size_t idx = threadIdx.x;

    // Find max value for numerical stability
    if (idx == 0) {
        max_val = -FLT_MAX;
        for (size_t i = 0; i < n_options; i++) {
            if (option_values[i] > max_val) {
                max_val = option_values[i];
            }
        }
    }
    __syncthreads();

    // Compute sum of exp
    if (idx == 0) {
        sum_exp = 0.0f;
        for (size_t i = 0; i < n_options; i++) {
            sum_exp += expf((option_values[i] - max_val) / temperature);
        }
    }
    __syncthreads();

    // Compute probabilities
    if (idx < n_options) {
        float exp_val = expf((option_values[idx] - max_val) / temperature);
        choice_probs[idx] = exp_val / (sum_exp + 1e-8f);
    }
}

bool nimcp_gpu_ofc_choice_probabilities(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    float temperature,
    const nimcp_gpu_ofc_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for choice probabilities");
        return false;
    }

    size_t n = state->n_options;

    // Use single block for small number of options
    kernel_ofc_softmax<<<1, BLOCK_SIZE>>>(
        (float*)state->choice_probabilities->data,
        (const float*)state->option_values->data,
        temperature,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for reversal learning detection and update
 */
__global__ void kernel_ofc_reversal(
    float* __restrict__ reversal_signal,
    float* __restrict__ expected_outcomes,
    const float* __restrict__ prediction_error,
    float reversal_rate,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float pe = prediction_error[idx];
    float pe_abs = fabsf(pe);

    // Large, sustained prediction errors indicate reversal
    float reversal = reversal_signal[idx];

    // Accumulate reversal evidence
    if (pe_abs > 0.3f) {
        reversal += reversal_rate * pe_abs * dt;
    } else {
        reversal *= 0.95f; // Decay
    }

    reversal = fmaxf(0.0f, fminf(1.0f, reversal));
    reversal_signal[idx] = reversal;

    // If reversal detected, increase learning rate (reset expectations)
    if (reversal > 0.7f) {
        // Partial reset of expectations
        expected_outcomes[idx] *= 0.5f;
    }
}

bool nimcp_gpu_ofc_reversal_learning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error,
    float dt,
    const nimcp_gpu_ofc_params_t* params)
{
    if (!ctx || !state || !prediction_error || !params) {
        LOG_ERROR("Invalid parameters for reversal learning");
        return false;
    }

    size_t n = state->reversal_signal->numel;

    kernel_ofc_reversal<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->reversal_signal->data,
        (float*)state->expected_outcomes->data,
        (const float*)prediction_error->data,
        params->reversal_rate,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Nucleus Accumbens Kernels
//=============================================================================

/**
 * @brief Kernel for reward prediction
 */
__global__ void kernel_nacc_reward_prediction(
    float* __restrict__ reward_prediction,
    const float* __restrict__ state_features,
    const float* __restrict__ action,
    float reward_sensitivity,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float state = state_features[idx];
    float act = action[idx];

    // Simple prediction: state-action value
    float pred = state * act * reward_sensitivity;
    reward_prediction[idx] = pred;
}

bool nimcp_gpu_nacc_reward_prediction(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* state_features,
    const nimcp_gpu_tensor_t* action,
    const nimcp_gpu_nacc_params_t* params)
{
    if (!ctx || !state || !state_features || !action || !params) {
        LOG_ERROR("Invalid parameters for reward prediction");
        return false;
    }

    size_t n = state_features->numel;

    kernel_nacc_reward_prediction<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->reward_prediction->data,
        (const float*)state_features->data,
        (const float*)action->data,
        params->reward_sensitivity,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for motivation computation
 *
 * Motivation = DA-modulated reward expectation - effort cost
 */
__global__ void kernel_nacc_motivation(
    float* __restrict__ motivation_signal,
    float* __restrict__ hedonic_signal,
    const float* __restrict__ reward_prediction,
    const float* __restrict__ dopamine,
    const float* __restrict__ effort_required,
    float dopamine_gain,
    float effort_cost,
    float hedonic_baseline,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float pred = reward_prediction[idx];
    float da = dopamine[idx];
    float effort = effort_required[idx];

    // Wanting: DA-modulated motivation
    float da_mod = 1.0f + dopamine_gain * (da - 0.05f);
    da_mod = fmaxf(0.1f, da_mod);

    float wanting = pred * da_mod - effort_cost * effort;
    motivation_signal[idx] = fmaxf(0.0f, wanting);

    // Liking: hedonic response (less DA-dependent)
    float liking = pred - hedonic_baseline;
    hedonic_signal[idx] = tanhf(liking);
}

bool nimcp_gpu_nacc_compute_motivation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* dopamine,
    const nimcp_gpu_tensor_t* effort_required,
    const nimcp_gpu_nacc_params_t* params)
{
    if (!ctx || !state || !dopamine || !effort_required || !params) {
        LOG_ERROR("Invalid parameters for motivation");
        return false;
    }

    size_t n = state->reward_prediction->numel;

    kernel_nacc_motivation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->motivation_signal->data,
        (float*)state->hedonic_signal->data,
        (const float*)state->reward_prediction->data,
        (const float*)dopamine->data,
        (const float*)effort_required->data,
        params->dopamine_gain,
        params->effort_cost,
        params->hedonic_baseline,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for D1/D2 MSN activity update
 */
__global__ void kernel_nacc_msn_update(
    float* __restrict__ msn_d1_activity,
    float* __restrict__ msn_d2_activity,
    const float* __restrict__ cortical_input,
    const float* __restrict__ dopamine,
    float glutamate_excitation,
    float gaba_inhibition,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float cortex = cortical_input[idx];
    float da = dopamine[idx];

    float d1 = msn_d1_activity[idx];
    float d2 = msn_d2_activity[idx];

    // D1-MSNs: excited by DA (Go pathway)
    float d1_da_mod = 1.0f + 2.0f * da;
    float d1_input = cortex * glutamate_excitation * d1_da_mod;
    d1 += (d1_input - d1 - gaba_inhibition * d2) * dt * 0.1f;

    // D2-MSNs: inhibited by DA (NoGo pathway)
    float d2_da_mod = 1.0f / (1.0f + 2.0f * da);
    float d2_input = cortex * glutamate_excitation * d2_da_mod;
    d2 += (d2_input - d2 - gaba_inhibition * d1) * dt * 0.1f;

    // ReLU-like activation
    d1 = fmaxf(0.0f, d1);
    d2 = fmaxf(0.0f, d2);

    msn_d1_activity[idx] = d1;
    msn_d2_activity[idx] = d2;
}

bool nimcp_gpu_nacc_msn_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* cortical_input,
    const nimcp_gpu_tensor_t* dopamine,
    float dt,
    const nimcp_gpu_nacc_params_t* params)
{
    if (!ctx || !state || !cortical_input || !dopamine || !params) {
        LOG_ERROR("Invalid parameters for MSN update");
        return false;
    }

    size_t n = cortical_input->numel;

    kernel_nacc_msn_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->msn_d1_activity->data,
        (float*)state->msn_d2_activity->data,
        (const float*)cortical_input->data,
        (const float*)dopamine->data,
        params->glutamate_excitation,
        params->gaba_inhibition,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for Go/NoGo decision signal
 */
__global__ void kernel_nacc_go_nogo(
    float* __restrict__ go_signal,
    float* __restrict__ nogo_signal,
    const float* __restrict__ msn_d1_activity,
    const float* __restrict__ msn_d2_activity,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float d1 = msn_d1_activity[idx];
    float d2 = msn_d2_activity[idx];

    // Direct pathway: Go signal
    go_signal[idx] = d1;

    // Indirect pathway: NoGo signal
    nogo_signal[idx] = d2;
}

bool nimcp_gpu_nacc_go_nogo(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_nacc_state_t* state,
    nimcp_gpu_tensor_t* go_signal,
    nimcp_gpu_tensor_t* nogo_signal,
    const nimcp_gpu_nacc_params_t* params)
{
    if (!ctx || !state || !go_signal || !nogo_signal || !params) {
        LOG_ERROR("Invalid parameters for Go/NoGo");
        return false;
    }

    size_t n = state->msn_d1_activity->numel;

    kernel_nacc_go_nogo<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)go_signal->data,
        (float*)nogo_signal->data,
        (const float*)state->msn_d1_activity->data,
        (const float*)state->msn_d2_activity->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// ACC Kernels
//=============================================================================

/**
 * @brief Kernel for conflict detection
 *
 * Conflict = product of competing response activations
 */
__global__ void kernel_acc_conflict(
    float* __restrict__ conflict_signal,
    const float* __restrict__ response_activations,
    float conflict_threshold,
    size_t n_responses,
    size_t n_batches)
{
    // Each thread handles one sample (assumes batch processing)
    size_t batch_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (batch_idx >= n_batches) return;

    // Conflict is the product of top two responses
    // For simplicity, compute pairwise conflicts

    float max1 = 0.0f, max2 = 0.0f;

    for (size_t r = 0; r < n_responses; r++) {
        float resp = response_activations[batch_idx * n_responses + r];
        if (resp > max1) {
            max2 = max1;
            max1 = resp;
        } else if (resp > max2) {
            max2 = resp;
        }
    }

    // Conflict = coactivation of competing responses
    float conflict = max1 * max2;

    // Threshold
    conflict = (conflict > conflict_threshold) ? conflict : 0.0f;

    conflict_signal[batch_idx] = conflict;
}

bool nimcp_gpu_acc_conflict_detection(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* response_activations,
    const nimcp_gpu_acc_params_t* params)
{
    if (!ctx || !state || !response_activations || !params) {
        LOG_ERROR("Invalid parameters for conflict detection");
        return false;
    }

    size_t n_batches = response_activations->numel / state->n_responses;

    kernel_acc_conflict<<<GRID_SIZE(n_batches), BLOCK_SIZE>>>(
        (float*)state->conflict_signal->data,
        (const float*)response_activations->data,
        params->conflict_threshold,
        state->n_responses,
        n_batches);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for error signal computation
 */
__global__ void kernel_acc_error(
    float* __restrict__ error_signal,
    const float* __restrict__ expected,
    const float* __restrict__ actual,
    float prediction_weight,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float exp_val = expected[idx];
    float act_val = actual[idx];

    // Prediction error
    float pe = act_val - exp_val;

    // Weighted error signal
    error_signal[idx] = prediction_weight * fabsf(pe);
}

bool nimcp_gpu_acc_error_signal(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* expected,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_acc_params_t* params)
{
    if (!ctx || !state || !expected || !actual || !params) {
        LOG_ERROR("Invalid parameters for error signal");
        return false;
    }

    size_t n = expected->numel;

    kernel_acc_error<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->error_signal->data,
        (const float*)expected->data,
        (const float*)actual->data,
        params->prediction_weight,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for effort allocation
 *
 * Effort allocation based on Expected Value of Control (EVC)
 */
__global__ void kernel_acc_effort(
    float* __restrict__ effort_allocation,
    float* __restrict__ control_signal,
    const float* __restrict__ task_demand,
    const float* __restrict__ reward_expectation,
    float effort_sensitivity,
    float control_gain,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float demand = task_demand[idx];
    float reward = reward_expectation[idx];

    // EVC: expected value of exerting control
    float evc = reward - effort_sensitivity * demand;

    // Effort allocation scales with positive EVC
    float effort = (evc > 0.0f) ? control_gain * evc : 0.0f;
    effort = fmaxf(0.0f, fminf(1.0f, effort));

    effort_allocation[idx] = effort;

    // Control signal for prefrontal areas
    control_signal[idx] = demand * effort;
}

bool nimcp_gpu_acc_effort_allocation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* task_demand,
    const nimcp_gpu_tensor_t* reward_expectation,
    const nimcp_gpu_acc_params_t* params)
{
    if (!ctx || !state || !task_demand || !reward_expectation || !params) {
        LOG_ERROR("Invalid parameters for effort allocation");
        return false;
    }

    size_t n = task_demand->numel;

    kernel_acc_effort<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->effort_allocation->data,
        (float*)state->control_signal->data,
        (const float*)task_demand->data,
        (const float*)reward_expectation->data,
        params->effort_sensitivity,
        params->control_gain,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Integrated Emotion System Kernels
//=============================================================================

bool nimcp_gpu_emotion_system_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_emotion_system_t* system,
    const nimcp_gpu_tensor_t* sensory_input,
    const nimcp_gpu_tensor_t* reward_signal,
    const nimcp_gpu_tensor_t* context,
    float dt)
{
    if (!ctx || !system) {
        LOG_ERROR("Invalid emotion system update parameters");
        return false;
    }

    // This is a placeholder for orchestrating all emotion subsystems
    // In a full implementation, this would:
    // 1. Run amygdala threat detection
    // 2. Update OFC values
    // 3. Update NAcc motivation
    // 4. Monitor ACC conflict

    system->dt = dt;
    return true;
}

/**
 * @brief Kernel for computing emotion state (PAD model)
 */
__global__ void kernel_emotion_compute_state(
    float* __restrict__ valence_out,
    float* __restrict__ arousal_out,
    float* __restrict__ dominance_out,
    const float* __restrict__ amygdala_output,
    const float* __restrict__ ofc_value,
    const float* __restrict__ nacc_motivation,
    const float* __restrict__ acc_conflict,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float threat = amygdala_output[idx];
    float value = ofc_value[idx];
    float motivation = nacc_motivation[idx];
    float conflict = acc_conflict[idx];

    // Valence: positive/negative (-1 to +1)
    // High value = positive, high threat = negative
    float valence = value - 0.5f * threat;
    valence = tanhf(valence);

    // Arousal: activation level (0 to 1)
    // High threat, motivation, or conflict = high arousal
    float arousal = (threat + motivation + conflict) / 3.0f;
    arousal = fmaxf(0.0f, fminf(1.0f, arousal));

    // Dominance: control (-1 to +1)
    // High motivation, low conflict = high dominance
    float dominance = motivation - conflict;
    dominance = tanhf(dominance);

    valence_out[idx] = valence;
    arousal_out[idx] = arousal;
    dominance_out[idx] = dominance;
}

bool nimcp_gpu_emotion_compute_state(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_emotion_system_t* system,
    nimcp_gpu_tensor_t* valence_out,
    nimcp_gpu_tensor_t* arousal_out,
    nimcp_gpu_tensor_t* dominance_out)
{
    if (!ctx || !system || !valence_out || !arousal_out || !dominance_out) {
        LOG_ERROR("Invalid parameters for emotion state computation");
        return false;
    }

    if (!system->amygdala || !system->ofc || !system->nacc || !system->acc) {
        LOG_ERROR("Incomplete emotion system");
        return false;
    }

    size_t n = valence_out->numel;

    kernel_emotion_compute_state<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)valence_out->data,
        (float*)arousal_out->data,
        (float*)dominance_out->data,
        (const float*)system->amygdala->central_output->data,
        (const float*)system->ofc->option_values->data,
        (const float*)system->nacc->motivation_signal->data,
        (const float*)system->acc->conflict_signal->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to categorize emotions from PAD values
 */
__global__ void kernel_emotion_categorize(
    float* __restrict__ emotion_probs,
    const float* __restrict__ valence,
    const float* __restrict__ arousal,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float v = valence[idx];
    float a = arousal[idx];

    // Map to emotion categories (simplified)
    // Using 2D circumplex model

    float probs[NIMCP_EMOTION_COUNT] = {0};

    // Neutral: low arousal, neutral valence
    probs[NIMCP_EMOTION_NEUTRAL] = expf(-2.0f * (v * v + a * a));

    // Fear: negative valence, high arousal
    probs[NIMCP_EMOTION_FEAR] = expf(-2.0f * ((v + 0.8f) * (v + 0.8f) + (a - 0.8f) * (a - 0.8f)));

    // Anger: negative valence, high arousal
    probs[NIMCP_EMOTION_ANGER] = expf(-2.0f * ((v + 0.6f) * (v + 0.6f) + (a - 0.7f) * (a - 0.7f)));

    // Disgust: negative valence, medium arousal
    probs[NIMCP_EMOTION_DISGUST] = expf(-2.0f * ((v + 0.7f) * (v + 0.7f) + (a - 0.3f) * (a - 0.3f)));

    // Sadness: negative valence, low arousal
    probs[NIMCP_EMOTION_SADNESS] = expf(-2.0f * ((v + 0.6f) * (v + 0.6f) + (a + 0.2f) * (a + 0.2f)));

    // Happiness: positive valence, medium-high arousal
    probs[NIMCP_EMOTION_HAPPINESS] = expf(-2.0f * ((v - 0.7f) * (v - 0.7f) + (a - 0.5f) * (a - 0.5f)));

    // Surprise: neutral valence, high arousal
    probs[NIMCP_EMOTION_SURPRISE] = expf(-2.0f * (v * v + (a - 0.9f) * (a - 0.9f)));

    // Anticipation: positive valence, medium arousal
    probs[NIMCP_EMOTION_ANTICIPATION] = expf(-2.0f * ((v - 0.5f) * (v - 0.5f) + (a - 0.4f) * (a - 0.4f)));

    // Trust: positive valence, low arousal
    probs[NIMCP_EMOTION_TRUST] = expf(-2.0f * ((v - 0.6f) * (v - 0.6f) + (a + 0.1f) * (a + 0.1f)));

    // Normalize
    float sum = 0.0f;
    for (int i = 0; i < NIMCP_EMOTION_COUNT; i++) {
        sum += probs[i];
    }

    for (int i = 0; i < NIMCP_EMOTION_COUNT; i++) {
        emotion_probs[idx * NIMCP_EMOTION_COUNT + i] = probs[i] / (sum + 1e-8f);
    }
}

bool nimcp_gpu_emotion_categorize(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* valence,
    const nimcp_gpu_tensor_t* arousal,
    nimcp_gpu_tensor_t* emotion_probs)
{
    if (!ctx || !valence || !arousal || !emotion_probs) {
        LOG_ERROR("Invalid parameters for emotion categorization");
        return false;
    }

    size_t n = valence->numel;

    kernel_emotion_categorize<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)emotion_probs->data,
        (const float*)valence->data,
        (const float*)arousal->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for emotional modulation of cognition
 */
__global__ void kernel_emotion_cognitive_modulation(
    float* __restrict__ attention_bias,
    float* __restrict__ memory_enhancement,
    float* __restrict__ decision_bias,
    const float* __restrict__ threat_signal,
    const float* __restrict__ reward_signal,
    const float* __restrict__ arousal,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float threat = threat_signal[idx];
    float reward = reward_signal[idx];
    float ar = arousal[idx];

    // Attention: biased toward threat and reward
    attention_bias[idx] = 0.5f * threat + 0.3f * reward;

    // Memory: enhanced by arousal
    memory_enhancement[idx] = 1.0f + 0.5f * ar;

    // Decision: biased by emotion
    // Threat promotes avoidance, reward promotes approach
    decision_bias[idx] = reward - threat;
}

bool nimcp_gpu_emotion_cognitive_modulation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_emotion_system_t* system,
    nimcp_gpu_tensor_t* attention_bias,
    nimcp_gpu_tensor_t* memory_enhancement,
    nimcp_gpu_tensor_t* decision_bias)
{
    if (!ctx || !system || !attention_bias || !memory_enhancement || !decision_bias) {
        LOG_ERROR("Invalid parameters for cognitive modulation");
        return false;
    }

    if (!system->amygdala || !system->nacc) {
        LOG_ERROR("Incomplete emotion system");
        return false;
    }

    size_t n = attention_bias->numel;

    kernel_emotion_cognitive_modulation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)attention_bias->data,
        (float*)memory_enhancement->data,
        (float*)decision_bias->data,
        (const float*)system->amygdala->threat_signal->data,
        (const float*)system->nacc->reward_prediction->data,
        (const float*)system->arousal_level->data,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

// Stub implementations
#include "gpu/emotion/nimcp_emotion_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "EMOTION_GPU"

nimcp_gpu_amygdala_params_t nimcp_gpu_amygdala_params_default(void)
{
    nimcp_gpu_amygdala_params_t params = {0};
    params.threat_threshold = 0.3f;
    params.fear_learning_rate = 0.1f;
    params.extinction_rate = 0.01f;
    params.generalization_sigma = 0.2f;
    params.habituation_tau = 1000.0f;
    params.sensitization_tau = 500.0f;
    params.context_weight = 0.3f;
    params.prefrontal_inhibition = 0.5f;
    params.lateral_inhibition = 0.2f;
    params.basal_threshold = 0.4f;
    return params;
}

nimcp_gpu_ofc_params_t nimcp_gpu_ofc_params_default(void)
{
    nimcp_gpu_ofc_params_t params = {0};
    params.value_learning_rate = 0.1f;
    params.discount_factor = 0.95f;
    params.reversal_rate = 0.2f;
    params.comparison_gain = 2.0f;
    params.risk_sensitivity = 1.0f;
    params.satiety_decay = 0.01f;
    params.integration_tau = 100.0f;
    params.outcome_sensitivity = 1.0f;
    return params;
}

nimcp_gpu_nacc_params_t nimcp_gpu_nacc_params_default(void)
{
    nimcp_gpu_nacc_params_t params = {0};
    params.reward_sensitivity = 1.0f;
    params.effort_cost = 0.5f;
    params.delay_discount = 0.1f;
    params.hedonic_baseline = 0.0f;
    params.motivation_decay = 0.05f;
    params.dopamine_gain = 2.0f;
    params.gaba_inhibition = 0.3f;
    params.glutamate_excitation = 1.0f;
    return params;
}

nimcp_gpu_acc_params_t nimcp_gpu_acc_params_default(void)
{
    nimcp_gpu_acc_params_t params = {0};
    params.conflict_threshold = 0.5f;
    params.error_learning_rate = 0.1f;
    params.effort_sensitivity = 1.0f;
    params.prediction_weight = 0.8f;
    params.volatility_estimate = 0.1f;
    params.control_gain = 1.5f;
    return params;
}

bool nimcp_gpu_amygdala_threat_detection(nimcp_gpu_context_t* ctx, nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* sensory_input, const nimcp_gpu_tensor_t* context,
    nimcp_gpu_tensor_t* threat_out, const nimcp_gpu_amygdala_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_amygdala_fear_conditioning(nimcp_gpu_context_t* ctx, nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* cs, const nimcp_gpu_tensor_t* us, float dt,
    const nimcp_gpu_amygdala_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_amygdala_extinction(nimcp_gpu_context_t* ctx, nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* cs, const nimcp_gpu_tensor_t* no_us, float dt,
    const nimcp_gpu_amygdala_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_amygdala_prefrontal_inhibition(nimcp_gpu_context_t* ctx, nimcp_gpu_amygdala_state_t* state,
    const nimcp_gpu_tensor_t* pfc_signal, const nimcp_gpu_amygdala_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_ofc_compute_values(nimcp_gpu_context_t* ctx, nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* stimulus_features, const nimcp_gpu_tensor_t* context,
    const nimcp_gpu_ofc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_ofc_value_update(nimcp_gpu_context_t* ctx, nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* chosen_option, const nimcp_gpu_tensor_t* outcome, float dt,
    const nimcp_gpu_ofc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_ofc_choice_probabilities(nimcp_gpu_context_t* ctx, nimcp_gpu_ofc_state_t* state,
    float temperature, const nimcp_gpu_ofc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_ofc_reversal_learning(nimcp_gpu_context_t* ctx, nimcp_gpu_ofc_state_t* state,
    const nimcp_gpu_tensor_t* prediction_error, float dt, const nimcp_gpu_ofc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nacc_reward_prediction(nimcp_gpu_context_t* ctx, nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* state_features, const nimcp_gpu_tensor_t* action,
    const nimcp_gpu_nacc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nacc_compute_motivation(nimcp_gpu_context_t* ctx, nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* dopamine, const nimcp_gpu_tensor_t* effort_required,
    const nimcp_gpu_nacc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nacc_msn_update(nimcp_gpu_context_t* ctx, nimcp_gpu_nacc_state_t* state,
    const nimcp_gpu_tensor_t* cortical_input, const nimcp_gpu_tensor_t* dopamine, float dt,
    const nimcp_gpu_nacc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nacc_go_nogo(nimcp_gpu_context_t* ctx, const nimcp_gpu_nacc_state_t* state,
    nimcp_gpu_tensor_t* go_signal, nimcp_gpu_tensor_t* nogo_signal,
    const nimcp_gpu_nacc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_acc_conflict_detection(nimcp_gpu_context_t* ctx, nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* response_activations, const nimcp_gpu_acc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_acc_error_signal(nimcp_gpu_context_t* ctx, nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* expected, const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_acc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_acc_effort_allocation(nimcp_gpu_context_t* ctx, nimcp_gpu_acc_state_t* state,
    const nimcp_gpu_tensor_t* task_demand, const nimcp_gpu_tensor_t* reward_expectation,
    const nimcp_gpu_acc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_emotion_system_update(nimcp_gpu_context_t* ctx, nimcp_gpu_emotion_system_t* system,
    const nimcp_gpu_tensor_t* sensory_input, const nimcp_gpu_tensor_t* reward_signal,
    const nimcp_gpu_tensor_t* context, float dt)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_emotion_compute_state(nimcp_gpu_context_t* ctx, const nimcp_gpu_emotion_system_t* system,
    nimcp_gpu_tensor_t* valence_out, nimcp_gpu_tensor_t* arousal_out, nimcp_gpu_tensor_t* dominance_out)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_emotion_categorize(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* valence,
    const nimcp_gpu_tensor_t* arousal, nimcp_gpu_tensor_t* emotion_probs)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_emotion_cognitive_modulation(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_emotion_system_t* system, nimcp_gpu_tensor_t* attention_bias,
    nimcp_gpu_tensor_t* memory_enhancement, nimcp_gpu_tensor_t* decision_bias)
{ LOG_WARN("CUDA not enabled"); return false; }

#endif // NIMCP_ENABLE_CUDA
