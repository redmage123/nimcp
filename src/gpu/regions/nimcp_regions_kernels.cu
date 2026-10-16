/**
 * @file nimcp_regions_kernels.cu
 * @brief GPU Brain Region Specialization CUDA Kernels
 *
 * WHAT: CUDA kernels for specialized brain region computations
 * WHY:  GPU acceleration for region-specific neural processing
 * HOW:  Custom kernels for cortical areas, subcortical structures
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <float.h>

#include "gpu/regions/nimcp_regions_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "REGIONS_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_column_params_t nimcp_gpu_column_params_default(void)
{
    nimcp_gpu_column_params_t params;
    params.n_columns = 100;
    params.n_neurons_per_column = 80;
    params.lateral_inhibition = 0.5f;
    params.recurrent_excitation = 0.3f;
    params.feedforward_gain = 1.0f;
    params.feedback_gain = 0.5f;
    params.adaptation_rate = 0.1f;

    // Initialize layer connectivity (canonical microcircuit)
    for (int i = 0; i < NIMCP_LAYER_COUNT; i++) {
        for (int j = 0; j < NIMCP_LAYER_COUNT; j++) {
            params.layer_connectivity[i][j] = 0.0f;
        }
    }
    // L4 -> L2/3, L2/3 -> L5, L5 -> L6, L6 -> L4
    params.layer_connectivity[NIMCP_LAYER_4][NIMCP_LAYER_2_3] = 0.8f;
    params.layer_connectivity[NIMCP_LAYER_2_3][NIMCP_LAYER_5] = 0.6f;
    params.layer_connectivity[NIMCP_LAYER_5][NIMCP_LAYER_6] = 0.5f;
    params.layer_connectivity[NIMCP_LAYER_6][NIMCP_LAYER_4] = 0.3f;

    return params;
}

nimcp_gpu_pfc_params_t nimcp_gpu_pfc_params_default(void)
{
    nimcp_gpu_pfc_params_t params;
    params.n_slots = 4;
    params.maintenance_gain = 0.9f;
    params.gating_threshold = 0.5f;
    params.decay_rate = 0.01f;
    params.interference_factor = 0.2f;
    params.dopamine_modulation = 1.0f;
    params.recurrent_maintenance = true;
    return params;
}

nimcp_gpu_motor_params_t nimcp_gpu_motor_params_default(void)
{
    nimcp_gpu_motor_params_t params;
    params.n_actions = 10;
    params.n_muscles = 20;
    params.population_coding_sigma = 30.0f;
    params.motor_noise = 0.05f;
    params.planning_horizon = 1.0f;
    params.sequence_learning_rate = 0.01f;
    params.use_internal_model = true;
    return params;
}

nimcp_gpu_parietal_params_t nimcp_gpu_parietal_params_default(void)
{
    nimcp_gpu_parietal_params_t params;
    params.spatial_resolution = 64;
    params.attention_gain = 2.0f;
    params.coordinate_transform_lr = 0.01f;
    params.multisensory_weight = 0.5f;
    params.egocentric_weight = 0.6f;
    params.allocentric_weight = 0.4f;
    return params;
}

nimcp_gpu_interregion_params_t nimcp_gpu_interregion_params_default(void)
{
    nimcp_gpu_interregion_params_t params;
    params.connection_strength = 0.5f;
    params.transmission_delay = 10.0f;
    params.plasticity_rate = 0.001f;
    params.bidirectional = true;
    params.feedback_ratio = 0.3f;
    return params;
}

//=============================================================================
// Cortical Column Kernels
//=============================================================================

/**
 * @brief Kernel for cortical column update
 */
/**
 * @brief Column update kernel - operates on 1D column outputs
 *
 * column_output and input are both [n_columns] sized.
 * lateral_connections is [n_columns x n_columns].
 * adaptation_state is [n_columns].
 */
__global__ void kernel_column_update(
    float* __restrict__ column_output,
    float* __restrict__ adaptation_state,
    const float* __restrict__ input,
    const float* __restrict__ feedback,
    const float* __restrict__ lateral_connections,
    float feedforward_gain,
    float feedback_gain,
    float lateral_inhibition,
    float recurrent_excitation,
    float adaptation_rate,
    float dt,
    size_t n_columns)
{
    size_t col_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (col_idx >= n_columns) return;

    float in = input[col_idx];
    float fb = feedback ? feedback[col_idx] : 0.0f;
    float adapt = adaptation_state[col_idx];

    // Feedforward input
    float ff_input = feedforward_gain * in;

    // Feedback modulation
    float fb_input = feedback_gain * fb;

    // Lateral interactions
    float lateral = 0.0f;
    for (size_t other_col = 0; other_col < n_columns; other_col++) {
        if (other_col != col_idx) {
            float conn = lateral_connections[col_idx * n_columns + other_col];
            float other_activity = column_output[other_col];
            lateral += conn * other_activity;
        }
    }

    // Compute activation
    float total_input = ff_input + fb_input - lateral_inhibition * lateral - adapt;
    float activation = 1.0f / (1.0f + expf(-total_input));  // Sigmoid

    // Update adaptation
    adapt += adaptation_rate * (activation - adapt) * dt;

    column_output[col_idx] = activation;
    adaptation_state[col_idx] = adapt;
}

bool nimcp_gpu_column_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* feedback,
    float dt,
    const nimcp_gpu_column_params_t* params)
{
    if (!ctx || !state || !input || !params) {
        LOG_ERROR("Invalid parameters for column update");
        return false;
    }

    size_t n = state->column_output->numel;

    kernel_column_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->column_output->data,
        (float*)state->adaptation_state->data,
        (const float*)input->data,
        feedback ? (const float*)feedback->data : NULL,
        (const float*)state->lateral_connections->data,
        params->feedforward_gain,
        params->feedback_gain,
        params->lateral_inhibition,
        params->recurrent_excitation,
        params->adaptation_rate,
        dt,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for lateral inhibition between columns
 *
 * Competitive inhibition: each column is suppressed proportionally to
 * the mean activity of other columns, weighted by connections.
 * Uses divisive normalization to prevent over-suppression in large networks.
 */
__global__ void kernel_lateral_inhibition(
    float* __restrict__ column_output,
    const float* __restrict__ lateral_connections,
    float inhibition_strength,
    size_t n_columns)
{
    size_t col = blockIdx.x * blockDim.x + threadIdx.x;
    if (col >= n_columns) return;

    float my_activity = column_output[col];

    // Weighted sum of other columns' activity
    float inhibition = 0.0f;
    for (size_t other = 0; other < n_columns; other++) {
        if (other != col) {
            float conn = lateral_connections[col * n_columns + other];
            inhibition += conn * column_output[other];
        }
    }

    // Divisive normalization: activity / (1 + inhibition_strength * inhibition)
    // This preserves relative differences while preventing over-suppression
    float new_activity = my_activity / (1.0f + inhibition_strength * inhibition);
    column_output[col] = fmaxf(0.0f, new_activity);
}

bool nimcp_gpu_column_lateral_inhibition(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state,
    const nimcp_gpu_column_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for lateral inhibition");
        return false;
    }

    // column_output is sized [n_columns], lateral_connections is [n_columns x n_columns]
    size_t n_cols = state->column_output->numel;
    kernel_lateral_inhibition<<<GRID_SIZE(n_cols), BLOCK_SIZE>>>(
        (float*)state->column_output->data,
        (const float*)state->lateral_connections->data,
        params->lateral_inhibition,
        n_cols);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for layer-to-layer propagation
 *
 * For each (source_layer, target_layer) pair with nonzero connectivity,
 * target += connectivity * source.
 */
__global__ void kernel_layer_propagate(
    float* __restrict__ target_layer,
    const float* __restrict__ source_layer,
    float connectivity,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    target_layer[idx] += connectivity * source_layer[idx];
}

bool nimcp_gpu_column_layer_propagate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state,
    const nimcp_gpu_column_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for layer propagation");
        return false;
    }

    // Propagate through cortical layers according to connectivity
    for (int src = 0; src < NIMCP_LAYER_COUNT; src++) {
        for (int tgt = 0; tgt < NIMCP_LAYER_COUNT; tgt++) {
            float conn = params->layer_connectivity[src][tgt];
            if (conn > 0.0f && state->layer_activity[src] && state->layer_activity[tgt]) {
                size_t n = state->layer_activity[tgt]->numel;
                if (state->layer_activity[src]->numel < n) {
                    n = state->layer_activity[src]->numel;
                }
                kernel_layer_propagate<<<GRID_SIZE(n), BLOCK_SIZE>>>(
                    (float*)state->layer_activity[tgt]->data,
                    (const float*)state->layer_activity[src]->data,
                    conn, n);
            }
        }
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// PFC Working Memory Kernels
//=============================================================================

/**
 * @brief Kernel for WM gating
 */
__global__ void kernel_pfc_gating(
    float* __restrict__ gate_state,
    const float* __restrict__ gate_signal,
    const float* __restrict__ dopamine,
    float gating_threshold,
    float dopamine_modulation,
    size_t n_slots)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_slots) return;

    float signal = gate_signal[idx];
    float da = dopamine ? dopamine[idx] : 1.0f;

    // DA modulates gating threshold
    float effective_threshold = gating_threshold / (dopamine_modulation * da + 0.1f);

    // Binary gating decision
    gate_state[idx] = (signal > effective_threshold) ? 1.0f : 0.0f;
}

/**
 * @brief Kernel for WM update
 *
 * Input is broadcast to all slots (size slot_dim, not n_slots*slot_dim).
 * Gate controls which slot receives the input.
 */
__global__ void kernel_pfc_wm_update(
    float* __restrict__ working_memory,
    const float* __restrict__ input,
    const float* __restrict__ gate_state,
    float maintenance_gain,
    float decay_rate,
    float interference_factor,
    float dt,
    size_t n_slots,
    size_t slot_dim)
{
    size_t slot_idx = blockIdx.x;
    size_t dim_idx = threadIdx.x;

    if (slot_idx >= n_slots || dim_idx >= slot_dim) return;

    size_t idx = slot_idx * slot_dim + dim_idx;

    float wm = working_memory[idx];
    float in = input[dim_idx];  // Broadcast input to all slots
    float gate = gate_state[slot_idx];

    // Interference from other slots
    float interference = 0.0f;
    for (size_t other = 0; other < n_slots; other++) {
        if (other != slot_idx) {
            interference += working_memory[other * slot_dim + dim_idx];
        }
    }
    interference *= interference_factor / n_slots;

    // Update: gate controls input, maintenance preserves content
    float new_wm = gate * in + (1.0f - gate) * maintenance_gain * wm;
    new_wm -= interference * dt;
    new_wm *= (1.0f - decay_rate * dt);

    working_memory[idx] = new_wm;
}

/**
 * @brief Kernel to compute PFC output from working memory
 *
 * Output is the average of all gated (non-empty) WM slots.
 * Runs with slot_dim threads.
 */
__global__ void kernel_pfc_compute_output(
    float* __restrict__ output,
    const float* __restrict__ working_memory,
    const float* __restrict__ gate_state,
    size_t n_slots,
    size_t slot_dim)
{
    size_t dim_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (dim_idx >= slot_dim) return;

    // Output = sum of WM contents weighted by whether slot has content
    float val = 0.0f;
    float total_weight = 0.0f;
    for (size_t s = 0; s < n_slots; s++) {
        float wm_val = working_memory[s * slot_dim + dim_idx];
        // Check if slot has content (any non-negligible value in this dim)
        float slot_strength = fabsf(wm_val);
        val += wm_val;
        if (slot_strength > 1e-6f) {
            total_weight += 1.0f;
        }
    }

    // Average over slots that have content, or just pass through sum
    if (total_weight > 0.0f) {
        output[dim_idx] = val / total_weight;
    } else {
        output[dim_idx] = 0.0f;
    }
}

bool nimcp_gpu_pfc_wm_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* input,
    const nimcp_gpu_tensor_t* dopamine,
    float dt,
    const nimcp_gpu_pfc_params_t* params)
{
    if (!ctx || !state || !input || !params) {
        LOG_ERROR("Invalid parameters for WM update");
        return false;
    }

    dim3 grid(state->n_slots);
    dim3 block(state->slot_dim);

    kernel_pfc_wm_update<<<grid, block>>>(
        (float*)state->working_memory->data,
        (const float*)input->data,
        (const float*)state->gate_state->data,
        params->maintenance_gain,
        params->decay_rate,
        params->interference_factor,
        dt,
        state->n_slots,
        state->slot_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Compute PFC output from updated working memory
    kernel_pfc_compute_output<<<GRID_SIZE(state->slot_dim), BLOCK_SIZE>>>(
        (float*)state->output->data,
        (const float*)state->working_memory->data,
        (const float*)state->gate_state->data,
        state->n_slots,
        state->slot_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_pfc_gating(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* gate_signal,
    const nimcp_gpu_pfc_params_t* params)
{
    if (!ctx || !state || !gate_signal || !params) {
        LOG_ERROR("Invalid parameters for PFC gating");
        return false;
    }

    kernel_pfc_gating<<<GRID_SIZE(state->n_slots), BLOCK_SIZE>>>(
        (float*)state->gate_state->data,
        (const float*)gate_signal->data,
        NULL,
        params->gating_threshold,
        params->dopamine_modulation,
        state->n_slots);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_pfc_maintenance(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    float dt,
    const nimcp_gpu_pfc_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for WM maintenance");
        return false;
    }

    // Recurrent maintenance already in wm_update
    return true;
}

/**
 * @brief Kernel for computing dot-product attention scores between query and WM slots
 *
 * Computes score[slot] = dot(query, wm[slot]) / sqrt(slot_dim), then softmax.
 * Also computes output = sum(attention[slot] * wm[slot]).
 */
__global__ void kernel_pfc_attention(
    float* __restrict__ attention_weights,
    float* __restrict__ output,
    const float* __restrict__ working_memory,
    const float* __restrict__ query,
    size_t n_slots,
    size_t slot_dim)
{
    // Single thread computes all attention scores (n_slots is typically small, 4-8)
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    float inv_sqrt_dim = 1.0f / sqrtf((float)slot_dim);

    // Compute dot-product scores
    float scores[32];  // Max 32 slots
    float max_score = -1e30f;
    for (size_t s = 0; s < n_slots && s < 32; s++) {
        float dot = 0.0f;
        for (size_t d = 0; d < slot_dim; d++) {
            dot += query[d] * working_memory[s * slot_dim + d];
        }
        scores[s] = dot * inv_sqrt_dim;
        if (scores[s] > max_score) max_score = scores[s];
    }

    // Softmax
    float sum_exp = 0.0f;
    for (size_t s = 0; s < n_slots && s < 32; s++) {
        scores[s] = expf(scores[s] - max_score);
        sum_exp += scores[s];
    }
    for (size_t s = 0; s < n_slots && s < 32; s++) {
        attention_weights[s] = scores[s] / (sum_exp + 1e-10f);
    }

    // Compute attention-weighted output: output = sum(attn[s] * wm[s])
    for (size_t d = 0; d < slot_dim; d++) {
        float val = 0.0f;
        for (size_t s = 0; s < n_slots && s < 32; s++) {
            val += attention_weights[s] * working_memory[s * slot_dim + d];
        }
        output[d] = val;
    }
}

bool nimcp_gpu_pfc_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* query,
    const nimcp_gpu_pfc_params_t* params)
{
    if (!ctx || !state || !query || !params) {
        LOG_ERROR("Invalid parameters for PFC attention");
        return false;
    }

    // Launch single-block kernel for attention computation
    kernel_pfc_attention<<<1, 1>>>(
        (float*)state->attention_weights->data,
        (float*)state->output->data,
        (const float*)state->working_memory->data,
        (const float*)query->data,
        state->n_slots,
        state->slot_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Motor Cortex Kernels
//=============================================================================

/**
 * @brief Kernel for motor planning: project goal to action plan
 */
__global__ void kernel_motor_plan(
    float* __restrict__ action_plan,
    const float* __restrict__ goal,
    float planning_horizon,
    size_t n_actions)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_actions) return;

    // Generate plan as softmax-like projection of goal
    float g = goal[idx];
    action_plan[idx] = g * planning_horizon;
}

bool nimcp_gpu_motor_plan(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* goal,
    const nimcp_gpu_motor_params_t* params)
{
    if (!ctx || !state || !goal || !params) {
        LOG_ERROR("Invalid parameters for motor planning");
        return false;
    }

    size_t n = state->n_actions;
    kernel_motor_plan<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->action_plan->data,
        (const float*)goal->data,
        params->planning_horizon,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for population coding
 */
__global__ void kernel_population_code(
    float* __restrict__ population_code,
    const float* __restrict__ direction,
    float sigma,
    int n_neurons,
    int dim)
{
    int neuron_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (neuron_idx >= n_neurons) return;

    // Each neuron has preferred direction
    float preferred_angle = (NIMCP_TWO_PI_F * neuron_idx) / n_neurons;

    float response = 0.0f;
    for (int d = 0; d < dim; d++) {
        float dir = direction[d];
        float diff = dir - preferred_angle;

        // Wrap angle difference
        while (diff > 3.14159f) diff -= NIMCP_TWO_PI_F;
        while (diff < -3.14159f) diff += NIMCP_TWO_PI_F;

        response += expf(-diff * diff / (2.0f * sigma * sigma));
    }

    population_code[neuron_idx] = response / dim;
}

/**
 * @brief Kernel for motor execution: generate motor commands from action plan
 *
 * Maps n_actions plan values to n_muscles output values.
 * Also creates efference copy.
 */
__global__ void kernel_motor_execute(
    float* __restrict__ motor_output,
    float* __restrict__ efference_copy,
    const float* __restrict__ action_plan,
    float motor_noise,
    size_t n_actions,
    size_t n_muscles)
{
    size_t muscle_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (muscle_idx >= n_muscles) return;

    // Each muscle is driven by weighted combination of actions
    float command = 0.0f;
    for (size_t a = 0; a < n_actions; a++) {
        // Simple mapping: each action contributes to muscles in its range
        float weight = expf(-((float)(muscle_idx % n_actions) - (float)a) *
                            ((float)(muscle_idx % n_actions) - (float)a) / 4.0f);
        command += weight * action_plan[a];
    }
    command /= (float)n_actions;

    motor_output[muscle_idx] = command;
    efference_copy[muscle_idx] = command;  // Copy for internal monitoring
}

bool nimcp_gpu_motor_execute(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    nimcp_gpu_tensor_t* motor_output,
    const nimcp_gpu_motor_params_t* params)
{
    if (!ctx || !state || !motor_output || !params) {
        LOG_ERROR("Invalid parameters for motor execution");
        return false;
    }

    size_t n = state->n_muscles;
    kernel_motor_execute<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)motor_output->data,
        (float*)state->efference_copy->data,
        (const float*)state->action_plan->data,
        params->motor_noise,
        state->n_actions,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for forward model update with prediction error
 */
__global__ void kernel_forward_model_update(
    float* __restrict__ forward_model,
    const float* __restrict__ predicted,
    const float* __restrict__ actual,
    float learning_rate,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float error = actual[idx] - predicted[idx];
    forward_model[idx] += learning_rate * error;
}

bool nimcp_gpu_motor_update_forward_model(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* predicted,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_motor_params_t* params)
{
    if (!ctx || !state || !predicted || !actual || !params) {
        LOG_ERROR("Invalid parameters for forward model update");
        return false;
    }

    size_t n = state->forward_model->numel;
    size_t pred_n = predicted->numel < n ? predicted->numel : n;

    kernel_forward_model_update<<<GRID_SIZE(pred_n), BLOCK_SIZE>>>(
        (float*)state->forward_model->data,
        (const float*)predicted->data,
        (const float*)actual->data,
        params->sequence_learning_rate,
        pred_n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_motor_population_code(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* direction,
    const nimcp_gpu_motor_params_t* params)
{
    if (!ctx || !state || !direction || !params) {
        LOG_ERROR("Invalid parameters for population coding");
        return false;
    }

    int n_neurons = state->n_actions * 10;  // 10 neurons per action

    kernel_population_code<<<GRID_SIZE(n_neurons), BLOCK_SIZE>>>(
        (float*)state->population_code->data,
        (const float*)direction->data,
        params->population_coding_sigma,
        n_neurons,
        direction->numel);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Parietal Cortex Kernels
//=============================================================================

/**
 * @brief Kernel for parietal attention: update attention map from visual + top-down
 */
__global__ void kernel_parietal_attention(
    float* __restrict__ attention_map,
    const float* __restrict__ visual_input,
    const float* __restrict__ top_down,
    float attention_gain,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float vis = visual_input[idx];
    float td = top_down ? top_down[idx] : 0.0f;

    // Bottom-up saliency + top-down modulation
    float priority = vis + attention_gain * td;
    attention_map[idx] = 1.0f / (1.0f + expf(-priority));  // Sigmoid
}

bool nimcp_gpu_parietal_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_tensor_t* top_down,
    const nimcp_gpu_parietal_params_t* params)
{
    if (!ctx || !state || !visual_input || !params) {
        LOG_ERROR("Invalid parameters for parietal attention");
        return false;
    }

    size_t n = state->attention_map->numel;
    kernel_parietal_attention<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->attention_map->data,
        (const float*)visual_input->data,
        top_down ? (const float*)top_down->data : NULL,
        params->attention_gain,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for coordinate transformation
 *
 * Transforms spatial_map to egocentric/allocentric representations.
 * Egocentric: shifted by eye position. Allocentric: based on world frame.
 */
__global__ void kernel_parietal_transform(
    float* __restrict__ egocentric,
    float* __restrict__ allocentric,
    const float* __restrict__ spatial_map,
    float eye_x,
    float eye_y,
    float ego_weight,
    float allo_weight,
    size_t map_size)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= map_size * map_size) return;

    size_t y = idx / map_size;
    size_t x = idx % map_size;

    // Egocentric: shift by eye position
    float shift_x = eye_x * (float)map_size;
    float shift_y = eye_y * (float)map_size;

    int src_x = (int)x + (int)shift_x;
    int src_y = (int)y + (int)shift_y;

    float ego_val = 0.0f;
    if (src_x >= 0 && src_x < (int)map_size && src_y >= 0 && src_y < (int)map_size) {
        ego_val = spatial_map[src_y * map_size + src_x] * ego_weight;
    }
    egocentric[idx] = ego_val;

    // Allocentric: world-centered (complement of egocentric shift)
    int allo_x = (int)x - (int)shift_x;
    int allo_y = (int)y - (int)shift_y;

    float allo_val = 0.0f;
    if (allo_x >= 0 && allo_x < (int)map_size && allo_y >= 0 && allo_y < (int)map_size) {
        allo_val = spatial_map[allo_y * map_size + allo_x] * allo_weight;
    }
    allocentric[idx] = allo_val;
}

bool nimcp_gpu_parietal_transform(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state,
    const nimcp_gpu_tensor_t* eye_position,
    const nimcp_gpu_parietal_params_t* params)
{
    if (!ctx || !state || !eye_position || !params) {
        LOG_ERROR("Invalid parameters for coordinate transform");
        return false;
    }

    // Read eye position from device
    float eye_data[2] = {0.0f, 0.0f};
    cudaMemcpy(eye_data, eye_position->data,
               (eye_position->numel < 2 ? eye_position->numel : 2) * sizeof(float),
               cudaMemcpyDeviceToHost);

    size_t n = state->map_size * state->map_size;
    kernel_parietal_transform<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->egocentric_rep->data,
        (float*)state->allocentric_rep->data,
        (const float*)state->spatial_map->data,
        eye_data[0], eye_data[1],
        params->egocentric_weight,
        params->allocentric_weight,
        state->map_size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for multisensory integration (Bayesian cue combination)
 */
__global__ void kernel_parietal_multisensory(
    float* __restrict__ spatial_map,
    const float* __restrict__ visual,
    const float* __restrict__ auditory,
    const float* __restrict__ proprioceptive,
    float vis_weight,
    float aud_weight,
    float prop_weight,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float v = visual ? visual[idx] : 0.0f;
    float a = auditory ? auditory[idx] : 0.0f;
    float p = proprioceptive ? proprioceptive[idx] : 0.0f;

    // Weighted sum (approximate Bayesian fusion)
    float total_weight = vis_weight + aud_weight + prop_weight;
    float integrated = (vis_weight * v + aud_weight * a + prop_weight * p) / (total_weight + 1e-10f);

    spatial_map[idx] = integrated;
}

bool nimcp_gpu_parietal_multisensory(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state,
    const nimcp_gpu_tensor_t* visual,
    const nimcp_gpu_tensor_t* auditory,
    const nimcp_gpu_tensor_t* proprioceptive,
    const nimcp_gpu_parietal_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for multisensory integration");
        return false;
    }

    size_t n = state->spatial_map->numel;

    // Visual gets highest weight, auditory and proprioceptive lower
    float vis_weight = params->multisensory_weight;
    float aud_weight = (1.0f - params->multisensory_weight) * 0.6f;
    float prop_weight = (1.0f - params->multisensory_weight) * 0.4f;

    kernel_parietal_multisensory<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)state->spatial_map->data,
        visual ? (const float*)visual->data : NULL,
        auditory ? (const float*)auditory->data : NULL,
        proprioceptive ? (const float*)proprioceptive->data : NULL,
        vis_weight, aud_weight, prop_weight,
        n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Inter-Region Communication
//=============================================================================

/**
 * @brief Kernel for interregion signal transmission
 *
 * target[j] = connection_strength * sum_i(weights[j,i] * source[i])
 */
__global__ void kernel_interregion_transmit(
    float* __restrict__ target_input,
    const float* __restrict__ source_activity,
    const float* __restrict__ connection_weights,
    float connection_strength,
    size_t source_dim,
    size_t target_dim)
{
    size_t j = blockIdx.x * blockDim.x + threadIdx.x;
    if (j >= target_dim) return;

    float sum = 0.0f;
    for (size_t i = 0; i < source_dim; i++) {
        sum += connection_weights[j * source_dim + i] * source_activity[i];
    }

    target_input[j] = connection_strength * sum;
}

bool nimcp_gpu_interregion_transmit(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_interregion_state_t* state,
    const nimcp_gpu_tensor_t* source_activity,
    nimcp_gpu_tensor_t* target_input,
    float dt,
    const nimcp_gpu_interregion_params_t* params)
{
    if (!ctx || !state || !source_activity || !target_input || !params) {
        LOG_ERROR("Invalid parameters for interregion transmission");
        return false;
    }

    kernel_interregion_transmit<<<GRID_SIZE(state->target_dim), BLOCK_SIZE>>>(
        (float*)target_input->data,
        (const float*)source_activity->data,
        (const float*)state->connection_weights->data,
        params->connection_strength,
        state->source_dim,
        state->target_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for Hebbian interregion plasticity
 *
 * delta_w[j,i] = plasticity_rate * target[j] * source[i] * dt
 */
__global__ void kernel_interregion_plasticity(
    float* __restrict__ connection_weights,
    const float* __restrict__ source_activity,
    const float* __restrict__ target_activity,
    float plasticity_rate,
    float dt,
    size_t source_dim,
    size_t target_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = source_dim * target_dim;
    if (idx >= total) return;

    size_t j = idx / source_dim;
    size_t i = idx % source_dim;

    float pre = source_activity[i];
    float post = target_activity[j];

    // Hebbian: fire together, wire together
    connection_weights[idx] += plasticity_rate * pre * post * dt;
}

bool nimcp_gpu_interregion_plasticity(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_interregion_state_t* state,
    const nimcp_gpu_tensor_t* source_activity,
    const nimcp_gpu_tensor_t* target_activity,
    float dt,
    const nimcp_gpu_interregion_params_t* params)
{
    if (!ctx || !state || !source_activity || !target_activity || !params) {
        LOG_ERROR("Invalid parameters for interregion plasticity");
        return false;
    }

    size_t total = state->source_dim * state->target_dim;
    kernel_interregion_plasticity<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)state->connection_weights->data,
        (const float*)source_activity->data,
        (const float*)target_activity->data,
        params->plasticity_rate,
        dt,
        state->source_dim,
        state->target_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/regions/nimcp_regions_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "REGIONS_GPU"

nimcp_gpu_column_params_t nimcp_gpu_column_params_default(void)
{
    nimcp_gpu_column_params_t params = {0};
    params.n_columns = 100;
    params.n_neurons_per_column = 80;
    params.lateral_inhibition = 0.5f;
    return params;
}

nimcp_gpu_pfc_params_t nimcp_gpu_pfc_params_default(void)
{
    nimcp_gpu_pfc_params_t params = {0};
    params.n_slots = 4;
    params.maintenance_gain = 0.9f;
    params.gating_threshold = 0.5f;
    return params;
}

nimcp_gpu_motor_params_t nimcp_gpu_motor_params_default(void)
{
    nimcp_gpu_motor_params_t params = {0};
    params.n_actions = 10;
    params.n_muscles = 20;
    return params;
}

nimcp_gpu_parietal_params_t nimcp_gpu_parietal_params_default(void)
{
    nimcp_gpu_parietal_params_t params = {0};
    params.spatial_resolution = 64;
    params.attention_gain = 2.0f;
    return params;
}

nimcp_gpu_interregion_params_t nimcp_gpu_interregion_params_default(void)
{
    nimcp_gpu_interregion_params_t params = {0};
    params.connection_strength = 0.5f;
    params.transmission_delay = 10.0f;
    return params;
}

// Stub implementations
bool nimcp_gpu_column_update(nimcp_gpu_context_t* ctx, nimcp_gpu_column_state_t* state,
    const nimcp_gpu_tensor_t* input, const nimcp_gpu_tensor_t* feedback, float dt,
    const nimcp_gpu_column_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_column_lateral_inhibition(nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state, const nimcp_gpu_column_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_column_layer_propagate(nimcp_gpu_context_t* ctx,
    nimcp_gpu_column_state_t* state, const nimcp_gpu_column_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_pfc_wm_update(nimcp_gpu_context_t* ctx, nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* input, const nimcp_gpu_tensor_t* dopamine, float dt,
    const nimcp_gpu_pfc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_pfc_gating(nimcp_gpu_context_t* ctx, nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* gate_signal, const nimcp_gpu_pfc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_pfc_maintenance(nimcp_gpu_context_t* ctx, nimcp_gpu_pfc_state_t* state,
    float dt, const nimcp_gpu_pfc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_pfc_attention(nimcp_gpu_context_t* ctx, nimcp_gpu_pfc_state_t* state,
    const nimcp_gpu_tensor_t* query, const nimcp_gpu_pfc_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_motor_plan(nimcp_gpu_context_t* ctx, nimcp_gpu_motor_state_t* state,
    const nimcp_gpu_tensor_t* goal, const nimcp_gpu_motor_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_motor_execute(nimcp_gpu_context_t* ctx, nimcp_gpu_motor_state_t* state,
    nimcp_gpu_tensor_t* motor_output, const nimcp_gpu_motor_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_motor_update_forward_model(nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state, const nimcp_gpu_tensor_t* predicted,
    const nimcp_gpu_tensor_t* actual, const nimcp_gpu_motor_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_motor_population_code(nimcp_gpu_context_t* ctx,
    nimcp_gpu_motor_state_t* state, const nimcp_gpu_tensor_t* direction,
    const nimcp_gpu_motor_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_parietal_attention(nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state, const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_tensor_t* top_down, const nimcp_gpu_parietal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_parietal_transform(nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state, const nimcp_gpu_tensor_t* eye_position,
    const nimcp_gpu_parietal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_parietal_multisensory(nimcp_gpu_context_t* ctx,
    nimcp_gpu_parietal_state_t* state, const nimcp_gpu_tensor_t* visual,
    const nimcp_gpu_tensor_t* auditory, const nimcp_gpu_tensor_t* proprioceptive,
    const nimcp_gpu_parietal_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_interregion_transmit(nimcp_gpu_context_t* ctx,
    nimcp_gpu_interregion_state_t* state, const nimcp_gpu_tensor_t* source_activity,
    nimcp_gpu_tensor_t* target_input, float dt, const nimcp_gpu_interregion_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_interregion_plasticity(nimcp_gpu_context_t* ctx,
    nimcp_gpu_interregion_state_t* state, const nimcp_gpu_tensor_t* source_activity,
    const nimcp_gpu_tensor_t* target_activity, float dt,
    const nimcp_gpu_interregion_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

#endif // NIMCP_ENABLE_CUDA
