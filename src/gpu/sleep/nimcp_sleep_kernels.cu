/**
 * @file nimcp_sleep_kernels.cu
 * @brief GPU Sleep and Memory Consolidation CUDA Kernels
 *
 * WHAT: CUDA kernels for sleep-dependent memory consolidation
 * WHY:  GPU acceleration for offline learning and memory transfer
 * HOW:  Custom kernels for replay, consolidation, synaptic homeostasis
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <vector>
#include <cstring>

#include "gpu/sleep/nimcp_sleep_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "SLEEP_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_nrem_params_t nimcp_gpu_nrem_params_default(void)
{
    nimcp_gpu_nrem_params_t params;
    params.slow_wave_freq = 0.8f;    // ~0.8 Hz slow oscillation
    params.spindle_freq = 12.0f;     // 12-15 Hz spindles
    params.sharp_wave_rate = 1.0f;   // 1 Hz SWR rate
    params.consolidation_rate = 0.01f;
    params.replay_speed = 10.0f;     // 10x compression
    params.cortical_gain = 0.5f;
    params.replay_iterations = 100;
    params.noise_level = 0.01f;
    return params;
}

nimcp_gpu_rem_params_t nimcp_gpu_rem_params_default(void)
{
    nimcp_gpu_rem_params_t params;
    params.theta_freq = 6.0f;        // 6-8 Hz theta
    params.pgo_rate = 0.5f;
    params.emotional_bias = 0.3f;
    params.integration_rate = 0.05f;
    params.dream_generation_rate = 0.1f;
    params.creativity_factor = 0.2f;
    params.acetylcholine_level = 0.8f;
    return params;
}

nimcp_gpu_homeostasis_params_t nimcp_gpu_homeostasis_params_default(void)
{
    nimcp_gpu_homeostasis_params_t params;
    params.downscaling_rate = 0.1f;
    params.threshold = 0.5f;
    params.preservation_factor = 0.9f;
    params.global_factor = 0.8f;
    params.selective_pruning = true;
    params.min_weight = 0.01f;
    return params;
}

nimcp_gpu_replay_params_t nimcp_gpu_replay_params_default(void)
{
    nimcp_gpu_replay_params_t params;
    params.compression_ratio = 10.0f;
    params.sequence_fidelity = 0.9f;
    params.reverse_replay_prob = 0.3f;
    params.priority_weight = 0.6f;
    params.buffer_size = 1000;
    params.decay_rate = 0.01f;
    return params;
}

//=============================================================================
// Sleep Stage Kernels
//=============================================================================

bool nimcp_gpu_sleep_stage_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_sleep_stage_state_t* state,
    float circadian_phase,
    float arousal_signal,
    float dt)
{
    if (!ctx || !state) {
        LOG_ERROR("Invalid parameters for sleep stage update");
        return false;
    }

    state->stage_duration += dt;
    state->total_sleep_time += dt;

    // Simple sleep stage state machine based on duration and pressure
    // Transitions: WAKE -> N1 -> N2 -> N3 -> N2 -> REM -> N2 -> ...
    float threshold = 30.0f;  // Transition every ~30 time units

    if (state->stage_duration > threshold) {
        state->stage_duration = 0.0f;

        switch (state->current_stage) {
            case NIMCP_SLEEP_WAKE:
                if (state->sleep_pressure > 0.5f && arousal_signal < 0.5f) {
                    state->current_stage = NIMCP_SLEEP_N1;
                }
                break;
            case NIMCP_SLEEP_N1:
                state->current_stage = NIMCP_SLEEP_N2;
                break;
            case NIMCP_SLEEP_N2:
                state->current_stage = NIMCP_SLEEP_N3;
                break;
            case NIMCP_SLEEP_N3:
                state->current_stage = NIMCP_SLEEP_N2;
                break;
            case NIMCP_SLEEP_REM:
                state->current_stage = NIMCP_SLEEP_N2;
                break;
            default:
                break;
        }
    }

    return true;
}

bool nimcp_gpu_sleep_transitions(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_sleep_stage_state_t* state,
    nimcp_gpu_tensor_t* transition_probs)
{
    if (!ctx || !state || !transition_probs) {
        LOG_ERROR("Invalid parameters for sleep transitions");
        return false;
    }

    // Compute transition probabilities based on stage duration and pressure
    return true;
}

//=============================================================================
// NREM Consolidation Kernels
//=============================================================================

/**
 * @brief Kernel for slow oscillation generation
 */
__global__ void kernel_slow_oscillation(
    float* __restrict__ oscillation,
    float time,
    float slow_wave_freq,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    // Slow oscillation: alternating up and down states
    float phase = NIMCP_TWO_PI_F * slow_wave_freq * time;
    float so = 0.5f * (1.0f + sinf(phase));

    // Nested faster oscillations (spindles during up state)
    float spindle = 0.0f;
    if (so > 0.5f) {
        spindle = 0.3f * sinf(12.0f * NIMCP_TWO_PI_F * time);
    }

    oscillation[idx] = so + spindle;
}

bool nimcp_gpu_nrem_slow_oscillation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    float time,
    const nimcp_gpu_nrem_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for slow oscillation");
        return false;
    }

    size_t n = state->buffer_size;

    kernel_slow_oscillation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->slow_oscillation->data,
        time,
        params->slow_wave_freq,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for memory replay
 */
__global__ void kernel_nrem_replay(
    float* __restrict__ cortical_weights,
    const float* __restrict__ hippocampal_buffer,
    const float* __restrict__ replay_buffer,
    const float* __restrict__ slow_oscillation,
    const float* __restrict__ priority_scores,
    float consolidation_rate,
    float replay_speed,
    size_t buffer_size,
    size_t memory_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= buffer_size * memory_dim) return;

    size_t mem_idx = idx / memory_dim;
    size_t dim_idx = idx % memory_dim;

    // Replay during slow oscillation up state
    float so = slow_oscillation[mem_idx % buffer_size];
    if (so < 0.5f) return;  // Only replay during up state

    float hippocampal = hippocampal_buffer[idx];
    float priority = priority_scores[mem_idx];

    // Transfer to cortex weighted by priority
    float transfer = consolidation_rate * hippocampal * priority * so;

    // Update cortical weights
    atomicAdd(&cortical_weights[idx], transfer);
}

/**
 * @brief Kernel to copy hippocampal content to replay buffer for replay
 */
__global__ void kernel_nrem_replay_buffer_update(
    float* __restrict__ replay_buffer,
    const float* __restrict__ hippocampal_buffer,
    const float* __restrict__ priority_scores,
    float replay_speed,
    size_t buffer_size,
    size_t memory_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = buffer_size * memory_dim;
    if (idx >= total) return;

    size_t mem_idx = idx / memory_dim;
    float priority = priority_scores[mem_idx];

    // Replay buffer gets hippocampal content weighted by priority and speed
    replay_buffer[idx] = hippocampal_buffer[idx] * priority * (1.0f / replay_speed);
}

bool nimcp_gpu_nrem_replay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_nrem_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for NREM replay");
        return false;
    }

    size_t n = state->buffer_size * state->memory_dim;

    // First update replay buffer from hippocampal content
    kernel_nrem_replay_buffer_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->replay_buffer->data,
        (const float*)state->hippocampal_buffer->data,
        (const float*)state->priority_scores->data,
        params->replay_speed,
        state->buffer_size,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Then transfer to cortical weights during slow oscillation up states
    kernel_nrem_replay<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->cortical_weights->data,
        (const float*)state->hippocampal_buffer->data,
        (const float*)state->replay_buffer->data,
        (const float*)state->slow_oscillation->data,
        (const float*)state->priority_scores->data,
        params->consolidation_rate,
        params->replay_speed,
        state->buffer_size,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for direct hippocampal-to-cortical transfer
 */
__global__ void kernel_systems_consolidation(
    float* __restrict__ cortical_weights,
    const float* __restrict__ hippocampal_buffer,
    const float* __restrict__ consolidation_mask,
    float consolidation_rate,
    float cortical_gain,
    float dt,
    size_t buffer_size,
    size_t memory_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = buffer_size * memory_dim;
    if (idx >= total) return;

    size_t mem_idx = idx / memory_dim;
    size_t dim_idx = idx % memory_dim;

    float mask = consolidation_mask[mem_idx];
    float hippocampal = hippocampal_buffer[idx];

    // Cortical weights indexed by memory dimension
    size_t cortical_idx = dim_idx * memory_dim + (dim_idx + mem_idx) % memory_dim;
    if (cortical_idx < memory_dim * memory_dim) {
        float transfer = consolidation_rate * cortical_gain * hippocampal * mask * dt;
        atomicAdd(&cortical_weights[cortical_idx], transfer);
    }
}

bool nimcp_gpu_nrem_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    float dt,
    const nimcp_gpu_nrem_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for systems consolidation");
        return false;
    }

    size_t n = state->buffer_size * state->memory_dim;

    kernel_systems_consolidation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->cortical_weights->data,
        (const float*)state->hippocampal_buffer->data,
        (const float*)state->consolidation_mask->data,
        params->consolidation_rate,
        params->cortical_gain,
        dt,
        state->buffer_size,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for sharp-wave ripple content generation
 *
 * Generates compressed content from hippocampal buffer by averaging
 * across memory entries weighted by priority.
 */
__global__ void kernel_sharp_wave_ripple(
    float* __restrict__ ripple_content,
    const float* __restrict__ hippocampal_buffer,
    const float* __restrict__ priority_scores,
    float sharp_wave_rate,
    size_t buffer_size,
    size_t memory_dim)
{
    size_t dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= memory_dim) return;

    // Average hippocampal content weighted by priority (compressed replay)
    float sum = 0.0f;
    float weight_sum = 0.0f;
    for (size_t m = 0; m < buffer_size; m++) {
        float priority = priority_scores[m];
        sum += hippocampal_buffer[m * memory_dim + dim_idx] * priority;
        weight_sum += priority;
    }

    if (weight_sum > 0.0f) {
        ripple_content[dim_idx] = sharp_wave_rate * sum / weight_sum;
    }
}

bool nimcp_gpu_nrem_sharp_wave_ripple(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    nimcp_gpu_tensor_t* ripple_content,
    const nimcp_gpu_nrem_params_t* params)
{
    if (!ctx || !state || !ripple_content || !params) {
        LOG_ERROR("Invalid parameters for SWR");
        return false;
    }

    size_t n = state->memory_dim;

    kernel_sharp_wave_ripple<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)ripple_content->data,
        (const float*)state->hippocampal_buffer->data,
        (const float*)state->priority_scores->data,
        params->sharp_wave_rate,
        state->buffer_size,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// REM Processing Kernels
//=============================================================================

bool nimcp_gpu_rem_processing(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    float dt,
    const nimcp_gpu_rem_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for REM processing");
        return false;
    }

    // REM: theta rhythm, ACh-dependent processing
    return true;
}

/**
 * @brief Kernel for memory integration
 */
__global__ void kernel_rem_integration(
    float* __restrict__ hippocampal_buffer,
    const float* __restrict__ semantic_memory,
    float integration_rate,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float episodic = hippocampal_buffer[idx];
    float semantic = semantic_memory[idx];

    // Integrate episodic with semantic knowledge
    float integrated = episodic + integration_rate * (semantic - episodic);

    hippocampal_buffer[idx] = integrated;
}

/**
 * @brief Kernel to transfer integrated memories to cortical weights
 */
__global__ void kernel_rem_cortical_update(
    float* __restrict__ cortical_weights,
    const float* __restrict__ hippocampal_buffer,
    const float* __restrict__ semantic_memory,
    float integration_rate,
    size_t memory_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= memory_dim * memory_dim) return;

    // Cortical update from integrated episodic-semantic blend
    float hip = hippocampal_buffer[idx % (memory_dim * memory_dim)];
    float sem = semantic_memory[idx];

    float blend = integration_rate * (hip + sem) * 0.5f;
    cortical_weights[idx] += blend;
}

bool nimcp_gpu_rem_memory_integration(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* semantic_memory,
    const nimcp_gpu_rem_params_t* params)
{
    if (!ctx || !state || !semantic_memory || !params) {
        LOG_ERROR("Invalid parameters for memory integration");
        return false;
    }

    // Update hippocampal buffer with semantic integration
    size_t n = state->hippocampal_buffer->numel;
    size_t sem_n = semantic_memory->numel < n ? semantic_memory->numel : n;

    kernel_rem_integration<<<GRID_SIZE(sem_n), BLOCK_SIZE>>>(
        (float*)state->hippocampal_buffer->data,
        (const float*)semantic_memory->data,
        params->integration_rate,
        sem_n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Also update cortical weights with integrated memories
    size_t cortical_n = state->cortical_weights->numel;

    kernel_rem_cortical_update<<<GRID_SIZE(cortical_n), BLOCK_SIZE>>>(
        (float*)state->cortical_weights->data,
        (const float*)state->hippocampal_buffer->data,
        (const float*)semantic_memory->data,
        params->integration_rate,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_rem_emotional_processing(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* emotional_tags,
    const nimcp_gpu_rem_params_t* params)
{
    if (!ctx || !state || !emotional_tags || !params) {
        LOG_ERROR("Invalid parameters for emotional processing");
        return false;
    }

    // REM emotional memory processing
    return true;
}

/**
 * @brief Kernel for dream content generation
 *
 * Creative recombination: combines random pairs of hippocampal memories
 * with modulation by creativity factor and ACh level.
 */
__global__ void kernel_dream_generation(
    float* __restrict__ dream_content,
    const float* __restrict__ hippocampal_buffer,
    float creativity_factor,
    float dream_generation_rate,
    float ach_level,
    size_t buffer_size,
    size_t memory_dim)
{
    size_t dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= memory_dim) return;

    // Combine memories with creative recombination
    float dream = 0.0f;
    for (size_t m = 0; m < buffer_size; m++) {
        float val = hippocampal_buffer[m * memory_dim + dim_idx];
        // Modulate by a position-dependent mixing weight for creative combinations
        float mix = sinf((float)m * 0.7f + (float)dim_idx * 0.3f) * creativity_factor;
        dream += val * (1.0f + mix);
    }

    dream_content[dim_idx] = dream_generation_rate * ach_level * dream / (float)buffer_size;
}

bool nimcp_gpu_rem_dream_generation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_consolidation_state_t* state,
    nimcp_gpu_tensor_t* dream_content,
    const nimcp_gpu_rem_params_t* params)
{
    if (!ctx || !state || !dream_content || !params) {
        LOG_ERROR("Invalid parameters for dream generation");
        return false;
    }

    size_t n = dream_content->numel;

    kernel_dream_generation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)dream_content->data,
        (const float*)state->hippocampal_buffer->data,
        params->creativity_factor,
        params->dream_generation_rate,
        params->acetylcholine_level,
        state->buffer_size,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Synaptic Homeostasis Kernels
//=============================================================================

/**
 * @brief Kernel for synaptic downscaling
 */
__global__ void kernel_synaptic_downscaling(
    float* __restrict__ weights,
    const float* __restrict__ potentiation_tags,
    float downscaling_rate,
    float preservation_factor,
    float min_weight,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float w = weights[idx];
    float tag = potentiation_tags[idx];

    // Preserve tagged (recently potentiated) synapses
    float protection = tag * preservation_factor;

    // Global downscaling
    float scale = 1.0f - downscaling_rate * (1.0f - protection);

    w *= scale;
    w = fmaxf(min_weight, w);

    weights[idx] = w;
}

bool nimcp_gpu_synaptic_downscaling(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_homeostasis_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for synaptic downscaling");
        return false;
    }

    size_t n = state->n_synapses;

    kernel_synaptic_downscaling<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->synaptic_weights->data,
        (const float*)state->potentiation_tags->data,
        params->downscaling_rate,
        params->preservation_factor,
        params->min_weight,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for selective synapse preservation
 *
 * Sets potentiation tags based on importance scores.
 * Important synapses get higher tags, protecting them from downscaling.
 */
__global__ void kernel_synapse_preservation(
    float* __restrict__ potentiation_tags,
    const float* __restrict__ importance_scores,
    float preservation_factor,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float importance = importance_scores[idx];

    // Set tag proportional to importance
    float tag = importance * preservation_factor;
    tag = fminf(1.0f, fmaxf(0.0f, tag));

    // Preserve existing tag if it's higher
    float current_tag = potentiation_tags[idx];
    potentiation_tags[idx] = fmaxf(current_tag, tag);
}

bool nimcp_gpu_synapse_preservation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_tensor_t* importance_scores,
    const nimcp_gpu_homeostasis_params_t* params)
{
    if (!ctx || !state || !importance_scores || !params) {
        LOG_ERROR("Invalid parameters for synapse preservation");
        return false;
    }

    size_t n = state->n_synapses;

    kernel_synapse_preservation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->potentiation_tags->data,
        (const float*)importance_scores->data,
        params->preservation_factor,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for synapse pruning
 *
 * Prunes weak synapses that are below threshold AND not protected
 * by potentiation tags. Tagged synapses are preserved.
 */
__global__ void kernel_synapse_pruning(
    float* __restrict__ weights,
    const float* __restrict__ potentiation_tags,
    float threshold,
    float min_weight,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float w = weights[idx];
    float tag = potentiation_tags[idx];

    // Only prune if weight is below threshold AND tag is low
    // High tags protect synapses from pruning
    float effective_threshold = threshold * (1.0f - tag);

    if (w < effective_threshold) {
        weights[idx] = min_weight;
    }
}

bool nimcp_gpu_synapse_pruning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_homeostasis_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for synapse pruning");
        return false;
    }

    if (!params->selective_pruning) return true;

    size_t n = state->n_synapses;

    kernel_synapse_pruning<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->synaptic_weights->data,
        (const float*)state->potentiation_tags->data,
        params->threshold,
        params->min_weight,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for synaptic tagging based on recent activity
 *
 * Synapses with high recent activity get tagged for consolidation.
 * Tags decay over time and are refreshed by activity.
 */
__global__ void kernel_synaptic_tagging(
    float* __restrict__ potentiation_tags,
    const float* __restrict__ recent_activity,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float activity = recent_activity[idx];
    float current_tag = potentiation_tags[idx];

    // Tag strength increases with activity, decays without it
    float decay = 0.01f * dt;
    float new_tag = current_tag * (1.0f - decay) + activity * 0.1f * dt;

    // Clamp to [0, 1]
    new_tag = fminf(1.0f, fmaxf(0.0f, new_tag));

    potentiation_tags[idx] = new_tag;
}

bool nimcp_gpu_synaptic_tagging(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state,
    const nimcp_gpu_tensor_t* recent_activity,
    float dt)
{
    if (!ctx || !state || !recent_activity) {
        LOG_ERROR("Invalid parameters for synaptic tagging");
        return false;
    }

    size_t n = state->n_synapses;

    kernel_synaptic_tagging<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->potentiation_tags->data,
        (const float*)recent_activity->data,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Memory Replay Functions
//=============================================================================

/**
 * @brief Kernel for prioritized experience replay sampling
 *
 * Samples memories from replay buffer weighted by priority.
 * Uses deterministic sampling based on priority-weighted indices.
 */
__global__ void kernel_replay_sample(
    float* __restrict__ sampled_memories,
    const float* __restrict__ replay_buffer,
    const float* __restrict__ priority_scores,
    float priority_weight,
    int n_samples,
    size_t buffer_size,
    size_t memory_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = (size_t)n_samples * memory_dim;
    if (idx >= total) return;

    size_t sample_idx = idx / memory_dim;
    size_t dim_idx = idx % memory_dim;

    // Select memory based on priority-weighted spacing
    // Higher priority memories are more likely to be selected
    float priority_sum = 0.0f;
    for (size_t m = 0; m < buffer_size; m++) {
        priority_sum += priority_scores[m];
    }

    // Target priority threshold for this sample
    float target = (priority_sum * (sample_idx + 0.5f)) / (float)n_samples;

    // Find corresponding memory index
    float running_sum = 0.0f;
    size_t selected = 0;
    for (size_t m = 0; m < buffer_size; m++) {
        running_sum += priority_scores[m];
        if (running_sum >= target) {
            selected = m;
            break;
        }
    }

    sampled_memories[idx] = replay_buffer[selected * memory_dim + dim_idx];
}

bool nimcp_gpu_replay_sample(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_consolidation_state_t* state,
    nimcp_gpu_tensor_t* sampled_memories,
    int n_samples,
    const nimcp_gpu_replay_params_t* params)
{
    if (!ctx || !state || !sampled_memories || !params) {
        LOG_ERROR("Invalid parameters for replay sampling");
        return false;
    }

    size_t total = (size_t)n_samples * state->memory_dim;

    kernel_replay_sample<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)sampled_memories->data,
        (const float*)state->replay_buffer->data,
        (const float*)state->priority_scores->data,
        params->priority_weight,
        n_samples,
        state->buffer_size,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel to store experience in replay buffer at given slot
 */
__global__ void kernel_replay_store(
    float* __restrict__ replay_buffer,
    float* __restrict__ priority_scores,
    const float* __restrict__ experience,
    float priority,
    size_t slot_idx,
    size_t memory_dim)
{
    size_t dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= memory_dim) return;

    replay_buffer[slot_idx * memory_dim + dim_idx] = experience[dim_idx];

    // Store priority (only one thread needs to do this)
    if (dim_idx == 0) {
        priority_scores[slot_idx] = priority;
    }
}

bool nimcp_gpu_replay_store(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* experience,
    float priority,
    const nimcp_gpu_replay_params_t* params)
{
    if (!ctx || !state || !experience || !params) {
        LOG_ERROR("Invalid parameters for replay storage");
        return false;
    }

    // Find slot to store (use circular buffer approach)
    // For simplicity, use priority_scores to find lowest priority slot
    // or fill sequentially. We use a simple write-position approach
    // by finding the first zero-priority slot or overwriting lowest priority.

    // Read priorities to host to find insertion point
    std::vector<float> priorities(state->buffer_size);
    cudaMemcpy(priorities.data(), state->priority_scores->data,
               state->buffer_size * sizeof(float), cudaMemcpyDeviceToHost);

    size_t slot = 0;
    float min_priority = priorities[0];
    for (size_t i = 0; i < state->buffer_size; i++) {
        if (priorities[i] < min_priority) {
            min_priority = priorities[i];
            slot = i;
        }
        // Prefer empty slots
        if (priorities[i] <= 0.0f) {
            slot = i;
            break;
        }
    }

    kernel_replay_store<<<GRID_SIZE(state->memory_dim), BLOCK_SIZE>>>(
        (float*)state->replay_buffer->data,
        (float*)state->priority_scores->data,
        (const float*)experience->data,
        priority,
        slot,
        state->memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for priority update based on TD errors
 *
 * Higher TD errors indicate more surprising/useful experiences.
 */
__global__ void kernel_replay_update_priorities(
    float* __restrict__ priority_scores,
    const float* __restrict__ td_errors,
    float priority_weight,
    float decay_rate,
    size_t buffer_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= buffer_size) return;

    float current = priority_scores[idx];
    float error = fabsf(td_errors[idx]);

    // Update priority: blend current with new TD error
    float new_priority = (1.0f - priority_weight) * current + priority_weight * error;

    // Apply decay
    new_priority *= (1.0f - decay_rate);

    // Clamp to [0, 1]
    new_priority = fminf(1.0f, fmaxf(0.0f, new_priority));

    priority_scores[idx] = new_priority;
}

bool nimcp_gpu_replay_update_priorities(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state,
    const nimcp_gpu_tensor_t* td_errors,
    const nimcp_gpu_replay_params_t* params)
{
    if (!ctx || !state || !td_errors || !params) {
        LOG_ERROR("Invalid parameters for priority update");
        return false;
    }

    kernel_replay_update_priorities<<<GRID_SIZE(state->buffer_size), BLOCK_SIZE>>>(
        (float*)state->priority_scores->data,
        (const float*)td_errors->data,
        params->priority_weight,
        params->decay_rate,
        state->buffer_size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for temporal compression of replay sequences
 *
 * Compresses sequence by averaging groups of frames.
 * compression_ratio frames -> 1 compressed frame.
 */
__global__ void kernel_replay_compress(
    float* __restrict__ compressed,
    const float* __restrict__ sequence,
    float compression_ratio,
    float sequence_fidelity,
    size_t seq_length,
    size_t compressed_length,
    size_t memory_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = compressed_length * memory_dim;
    if (idx >= total) return;

    size_t comp_frame = idx / memory_dim;
    size_t dim_idx = idx % memory_dim;

    // Determine source frame range
    size_t start_frame = (size_t)(comp_frame * compression_ratio);
    size_t end_frame = (size_t)((comp_frame + 1) * compression_ratio);
    if (end_frame > seq_length) end_frame = seq_length;

    // Average over source frames with fidelity weighting
    float sum = 0.0f;
    float weight_sum = 0.0f;
    for (size_t f = start_frame; f < end_frame; f++) {
        // Higher fidelity = more weight on center frames
        float rel_pos = (float)(f - start_frame) / (float)(end_frame - start_frame);
        float weight = 1.0f + sequence_fidelity * (1.0f - fabsf(2.0f * rel_pos - 1.0f));
        sum += sequence[f * memory_dim + dim_idx] * weight;
        weight_sum += weight;
    }

    if (weight_sum > 0.0f) {
        compressed[idx] = sum / weight_sum;
    }
}

bool nimcp_gpu_replay_compress(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sequence,
    nimcp_gpu_tensor_t* compressed,
    const nimcp_gpu_replay_params_t* params)
{
    if (!ctx || !sequence || !compressed || !params) {
        LOG_ERROR("Invalid parameters for replay compression");
        return false;
    }

    // Infer dimensions from tensors
    size_t memory_dim = compressed->dims[compressed->ndim - 1];
    if (memory_dim == 0) memory_dim = 1;
    size_t compressed_length = compressed->numel / memory_dim;
    size_t seq_length = sequence->numel / memory_dim;

    size_t total = compressed->numel;

    kernel_replay_compress<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)compressed->data,
        (const float*)sequence->data,
        params->compression_ratio,
        params->sequence_fidelity,
        seq_length,
        compressed_length,
        memory_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/sleep/nimcp_sleep_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "SLEEP_GPU"

nimcp_gpu_nrem_params_t nimcp_gpu_nrem_params_default(void)
{
    nimcp_gpu_nrem_params_t params = {0};
    params.slow_wave_freq = 0.8f;
    params.consolidation_rate = 0.01f;
    return params;
}

nimcp_gpu_rem_params_t nimcp_gpu_rem_params_default(void)
{
    nimcp_gpu_rem_params_t params = {0};
    params.theta_freq = 6.0f;
    params.integration_rate = 0.05f;
    return params;
}

nimcp_gpu_homeostasis_params_t nimcp_gpu_homeostasis_params_default(void)
{
    nimcp_gpu_homeostasis_params_t params = {0};
    params.downscaling_rate = 0.1f;
    params.min_weight = 0.01f;
    return params;
}

nimcp_gpu_replay_params_t nimcp_gpu_replay_params_default(void)
{
    nimcp_gpu_replay_params_t params = {0};
    params.compression_ratio = 10.0f;
    params.buffer_size = 1000;
    return params;
}

// Stub implementations
bool nimcp_gpu_sleep_stage_update(nimcp_gpu_context_t* ctx,
    nimcp_gpu_sleep_stage_state_t* state, float circadian_phase,
    float arousal_signal, float dt)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_sleep_transitions(nimcp_gpu_context_t* ctx,
    nimcp_gpu_sleep_stage_state_t* state, nimcp_gpu_tensor_t* transition_probs)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nrem_slow_oscillation(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, float time, const nimcp_gpu_nrem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nrem_replay(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, const nimcp_gpu_nrem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nrem_systems_consolidation(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, float dt, const nimcp_gpu_nrem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_nrem_sharp_wave_ripple(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, nimcp_gpu_tensor_t* ripple_content,
    const nimcp_gpu_nrem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rem_processing(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, float dt, const nimcp_gpu_rem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rem_memory_integration(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, const nimcp_gpu_tensor_t* semantic_memory,
    const nimcp_gpu_rem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rem_emotional_processing(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, const nimcp_gpu_tensor_t* emotional_tags,
    const nimcp_gpu_rem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_rem_dream_generation(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_consolidation_state_t* state, nimcp_gpu_tensor_t* dream_content,
    const nimcp_gpu_rem_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_synaptic_downscaling(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state, const nimcp_gpu_homeostasis_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_synapse_preservation(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state, const nimcp_gpu_tensor_t* importance_scores,
    const nimcp_gpu_homeostasis_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_synapse_pruning(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state, const nimcp_gpu_homeostasis_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_synaptic_tagging(nimcp_gpu_context_t* ctx,
    nimcp_gpu_synaptic_state_t* state, const nimcp_gpu_tensor_t* recent_activity, float dt)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_replay_sample(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_consolidation_state_t* state, nimcp_gpu_tensor_t* sampled_memories,
    int n_samples, const nimcp_gpu_replay_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_replay_store(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, const nimcp_gpu_tensor_t* experience,
    float priority, const nimcp_gpu_replay_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_replay_update_priorities(nimcp_gpu_context_t* ctx,
    nimcp_gpu_consolidation_state_t* state, const nimcp_gpu_tensor_t* td_errors,
    const nimcp_gpu_replay_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_replay_compress(nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sequence, nimcp_gpu_tensor_t* compressed,
    const nimcp_gpu_replay_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

#endif // NIMCP_ENABLE_CUDA
