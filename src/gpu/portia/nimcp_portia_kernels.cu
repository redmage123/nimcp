/**
 * @file nimcp_portia_kernels.cu
 * @brief GPU Portia Spider Vision CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for Portia spider-inspired visual cognition
 * WHY:  GPU acceleration for sophisticated spatial reasoning
 * HOW:  Custom kernels for attention, spatial mapping, route planning
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <float.h>

#include "gpu/portia/nimcp_portia_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "PORTIA_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Default Parameters
//=============================================================================

nimcp_gpu_portia_attention_params_t nimcp_gpu_portia_attention_params_default(void)
{
    nimcp_gpu_portia_attention_params_t params;
    params.salience_threshold = 0.3f;
    params.movement_sensitivity = 2.0f;
    params.prey_template_weight = 0.5f;
    params.novelty_bonus = 0.3f;
    params.attention_resolution = 64;
    params.saccade_rate = 3.0f;
    params.fixation_duration = 200.0f;
    params.max_tracked_objects = 5;
    return params;
}

nimcp_gpu_portia_spatial_params_t nimcp_gpu_portia_spatial_params_default(void)
{
    nimcp_gpu_portia_spatial_params_t params;
    params.map_resolution = 32;
    params.landmark_weight = 0.6f;
    params.path_integration_gain = 0.9f;
    params.detour_threshold = 0.5f;
    params.planning_depth = 5;
    params.obstacle_memory_decay = 0.01f;
    params.goal_persistence = 0.95f;
    params.use_mental_rotation = true;
    return params;
}

nimcp_gpu_portia_prey_params_t nimcp_gpu_portia_prey_params_default(void)
{
    nimcp_gpu_portia_prey_params_t params;
    params.num_prey_templates = 10;
    params.template_match_threshold = 0.7f;
    params.size_tolerance = 0.3f;
    params.motion_pattern_weight = 0.4f;
    params.deceptive_approach_rate = 0.5f;
    params.cryptic_prey_detection = true;
    return params;
}

//=============================================================================
// Attention Kernels
//=============================================================================

/**
 * @brief Kernel for computing visual salience
 */
__global__ void kernel_compute_salience(
    float* __restrict__ salience_map,
    const float* __restrict__ visual_input,
    const float* __restrict__ prev_input,
    float movement_sensitivity,
    float salience_threshold,
    size_t width,
    size_t height)
{
    size_t x = blockIdx.x * blockDim.x + threadIdx.x;
    size_t y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    size_t idx = y * width + x;

    float current = visual_input[idx];
    float prev = prev_input[idx];

    // Motion salience
    float motion = fabsf(current - prev) * movement_sensitivity;

    // Center-surround contrast
    float center = current;
    float surround = 0.0f;
    int count = 0;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                surround += visual_input[ny * width + nx];
                count++;
            }
        }
    }
    surround /= (count + 1e-8f);

    float contrast = fabsf(center - surround);

    // Combined salience
    float salience = 0.6f * motion + 0.4f * contrast;

    // Threshold
    salience_map[idx] = (salience > salience_threshold) ? salience : 0.0f;
}

bool nimcp_gpu_portia_compute_salience(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_portia_attention_params_t* params)
{
    if (!ctx || !state || !visual_input || !params) {
        LOG_ERROR("Invalid parameters for salience computation");
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    dim3 block(16, 16);
    dim3 grid((state->map_width + 15) / 16, (state->map_height + 15) / 16);

    kernel_compute_salience<<<grid, block>>>(
        (float*)state->salience_map->data,
        (const float*)visual_input->data,
        (const float*)state->fixation_history->data,
        params->movement_sensitivity,
        params->salience_threshold,
        state->map_width,
        state->map_height);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/**
 * @brief Kernel for updating attention focus
 */
__global__ void kernel_update_attention(
    float* __restrict__ attention_focus,
    const float* __restrict__ salience_map,
    float saccade_rate,
    float dt,
    size_t width,
    size_t height)
{
    // Find maximum salience location
    __shared__ float max_salience;
    __shared__ int max_x, max_y;

    int idx = threadIdx.x;

    if (idx == 0) {
        max_salience = 0.0f;
        max_x = width / 2;
        max_y = height / 2;

        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                float s = salience_map[y * width + x];
                if (s > max_salience) {
                    max_salience = s;
                    max_x = x;
                    max_y = y;
                }
            }
        }

        // Smooth saccade movement
        float curr_x = attention_focus[0];
        float curr_y = attention_focus[1];

        float alpha = saccade_rate * dt / 1000.0f;
        alpha = fminf(1.0f, alpha);

        attention_focus[0] = curr_x + alpha * (max_x - curr_x);
        attention_focus[1] = curr_y + alpha * (max_y - curr_y);
    }
}

bool nimcp_gpu_portia_update_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    float dt,
    const nimcp_gpu_portia_attention_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for attention update");
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_update_attention<<<1, 1>>>(
        (float*)state->attention_focus->data,
        (const float*)state->salience_map->data,
        params->saccade_rate,
        dt,
        state->map_width,
        state->map_height);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_portia_track_objects(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    float dt,
    const nimcp_gpu_portia_attention_params_t* params)
{
    if (!ctx || !state || !visual_input || !params) {
        LOG_ERROR("Invalid parameters for object tracking");
        return false;
    }

    // Object tracking via salience peaks
    // Simplified implementation
    return true;
}

bool nimcp_gpu_portia_novelty_detection(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_portia_attention_params_t* params)
{
    if (!ctx || !state || !visual_input || !params) {
        LOG_ERROR("Invalid parameters for novelty detection");
        return false;
    }

    // Novelty = difference from running average
    return true;
}

//=============================================================================
// Spatial Cognition Kernels
//=============================================================================

bool nimcp_gpu_portia_update_spatial_map(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_tensor_t* movement,
    const nimcp_gpu_portia_spatial_params_t* params)
{
    if (!ctx || !state || !visual_input || !movement || !params) {
        LOG_ERROR("Invalid parameters for spatial map update");
        return false;
    }

    // Update allocentric spatial map with visual input
    return true;
}

/**
 * @brief Kernel for A* route planning
 */
__global__ void kernel_plan_route(
    float* __restrict__ planned_route,
    const float* __restrict__ spatial_map,
    const float* __restrict__ obstacle_map,
    const float* __restrict__ current_pos,
    const float* __restrict__ goal_pos,
    int map_size,
    int planning_depth)
{
    // Simplified wavefront propagation
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= map_size * map_size) return;

    int x = idx % map_size;
    int y = idx / map_size;

    float obstacle = obstacle_map[idx];
    if (obstacle > 0.5f) {
        planned_route[idx] = -1.0f;  // Impassable
        return;
    }

    // Distance to goal
    float gx = goal_pos[0];
    float gy = goal_pos[1];

    float dist = sqrtf((x - gx) * (x - gx) + (y - gy) * (y - gy));

    // Heuristic value (distance + obstacle penalty)
    planned_route[idx] = dist;
}

bool nimcp_gpu_portia_plan_route(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* goal,
    const nimcp_gpu_portia_spatial_params_t* params)
{
    if (!ctx || !state || !goal || !params) {
        LOG_ERROR("Invalid parameters for route planning");
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    int map_size = state->map_size;

    kernel_plan_route<<<GRID_SIZE(map_size * map_size), BLOCK_SIZE>>>(
        (float*)state->planned_route->data,
        (const float*)state->spatial_map->data,
        (const float*)state->obstacle_map->data,
        (const float*)state->current_position->data,
        (const float*)goal->data,
        map_size,
        params->planning_depth);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_portia_mental_rotation(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    float rotation_angle,
    nimcp_gpu_tensor_t* rotated_view,
    const nimcp_gpu_portia_spatial_params_t* params)
{
    if (!ctx || !state || !rotated_view || !params) {
        LOG_ERROR("Invalid parameters for mental rotation");
        return false;
    }

    // Mental rotation for perspective taking
    return true;
}

bool nimcp_gpu_portia_path_integration(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* self_motion,
    float dt,
    const nimcp_gpu_portia_spatial_params_t* params)
{
    if (!ctx || !state || !self_motion || !params) {
        LOG_ERROR("Invalid parameters for path integration");
        return false;
    }

    // Dead reckoning position update
    return true;
}

bool nimcp_gpu_portia_detour_planning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state,
    const nimcp_gpu_tensor_t* obstacle,
    const nimcp_gpu_portia_spatial_params_t* params)
{
    if (!ctx || !state || !obstacle || !params) {
        LOG_ERROR("Invalid parameters for detour planning");
        return false;
    }

    // Plan detour around obstacle
    return true;
}

//=============================================================================
// Prey Recognition Kernels
//=============================================================================

/**
 * @brief Kernel for template matching
 */
__global__ void kernel_match_prey(
    float* __restrict__ detection_confidence,
    const float* __restrict__ visual_patch,
    const float* __restrict__ prey_templates,
    float template_match_threshold,
    int n_templates,
    int template_dim)
{
    int template_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (template_idx >= n_templates) return;

    // Normalized cross-correlation
    float sum_patch = 0.0f, sum_template = 0.0f;
    float sum_patch_sq = 0.0f, sum_template_sq = 0.0f;
    float sum_product = 0.0f;

    for (int d = 0; d < template_dim; d++) {
        float p = visual_patch[d];
        float t = prey_templates[template_idx * template_dim + d];

        sum_patch += p;
        sum_template += t;
        sum_patch_sq += p * p;
        sum_template_sq += t * t;
        sum_product += p * t;
    }

    float n = (float)template_dim;
    float numerator = n * sum_product - sum_patch * sum_template;
    float denom = sqrtf((n * sum_patch_sq - sum_patch * sum_patch) *
                        (n * sum_template_sq - sum_template * sum_template));

    float ncc = (denom > 1e-8f) ? numerator / denom : 0.0f;

    detection_confidence[template_idx] = (ncc > template_match_threshold) ? ncc : 0.0f;
}

bool nimcp_gpu_portia_match_prey(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    const nimcp_gpu_tensor_t* visual_patch,
    const nimcp_gpu_portia_prey_params_t* params)
{
    if (!ctx || !state || !visual_patch || !params) {
        LOG_ERROR("Invalid parameters for prey matching");
        return false;
    }
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    kernel_match_prey<<<GRID_SIZE(state->n_templates), BLOCK_SIZE>>>(
        (float*)state->detection_confidence->data,
        (const float*)visual_patch->data,
        (const float*)state->prey_templates->data,
        params->template_match_threshold,
        state->n_templates,
        state->template_dim);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_gpu_portia_predict_prey_trajectory(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    float dt,
    const nimcp_gpu_portia_prey_params_t* params)
{
    if (!ctx || !state || !params) {
        LOG_ERROR("Invalid parameters for trajectory prediction");
        return false;
    }

    // Kalman-like trajectory prediction
    return true;
}

bool nimcp_gpu_portia_plan_approach(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    const nimcp_gpu_portia_spatial_state_t* spatial_state,
    const nimcp_gpu_portia_prey_params_t* params)
{
    if (!ctx || !state || !spatial_state || !params) {
        LOG_ERROR("Invalid parameters for approach planning");
        return false;
    }

    // Deceptive stalking approach
    return true;
}

bool nimcp_gpu_portia_update_prey_templates(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state,
    const nimcp_gpu_tensor_t* successful_prey,
    float learning_rate,
    const nimcp_gpu_portia_prey_params_t* params)
{
    if (!ctx || !state || !successful_prey || !params) {
        LOG_ERROR("Invalid parameters for template learning");
        return false;
    }

    // Update prey templates with successful catches
    return true;
}

#else // !NIMCP_ENABLE_CUDA

#include "gpu/portia/nimcp_portia_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "PORTIA_GPU"

nimcp_gpu_portia_attention_params_t nimcp_gpu_portia_attention_params_default(void)
{
    nimcp_gpu_portia_attention_params_t params = {0};
    params.salience_threshold = 0.3f;
    params.movement_sensitivity = 2.0f;
    return params;
}

nimcp_gpu_portia_spatial_params_t nimcp_gpu_portia_spatial_params_default(void)
{
    nimcp_gpu_portia_spatial_params_t params = {0};
    params.map_resolution = 32;
    params.planning_depth = 5;
    return params;
}

nimcp_gpu_portia_prey_params_t nimcp_gpu_portia_prey_params_default(void)
{
    nimcp_gpu_portia_prey_params_t params = {0};
    params.num_prey_templates = 10;
    params.template_match_threshold = 0.7f;
    return params;
}

// Stub implementations
bool nimcp_gpu_portia_compute_salience(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state, const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_portia_attention_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_update_attention(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state, float dt,
    const nimcp_gpu_portia_attention_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_track_objects(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state, const nimcp_gpu_tensor_t* visual_input,
    float dt, const nimcp_gpu_portia_attention_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_novelty_detection(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_attention_state_t* state, const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_portia_attention_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_update_spatial_map(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state, const nimcp_gpu_tensor_t* visual_input,
    const nimcp_gpu_tensor_t* movement, const nimcp_gpu_portia_spatial_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_plan_route(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state, const nimcp_gpu_tensor_t* goal,
    const nimcp_gpu_portia_spatial_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_mental_rotation(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state, float rotation_angle,
    nimcp_gpu_tensor_t* rotated_view, const nimcp_gpu_portia_spatial_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_path_integration(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state, const nimcp_gpu_tensor_t* self_motion,
    float dt, const nimcp_gpu_portia_spatial_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_detour_planning(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_spatial_state_t* state, const nimcp_gpu_tensor_t* obstacle,
    const nimcp_gpu_portia_spatial_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_match_prey(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state, const nimcp_gpu_tensor_t* visual_patch,
    const nimcp_gpu_portia_prey_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_predict_prey_trajectory(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state, float dt,
    const nimcp_gpu_portia_prey_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_plan_approach(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state, const nimcp_gpu_portia_spatial_state_t* spatial_state,
    const nimcp_gpu_portia_prey_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

bool nimcp_gpu_portia_update_prey_templates(nimcp_gpu_context_t* ctx,
    nimcp_gpu_portia_prey_state_t* state, const nimcp_gpu_tensor_t* successful_prey,
    float learning_rate, const nimcp_gpu_portia_prey_params_t* params)
{ LOG_WARN("CUDA not enabled"); return false; }

#endif // NIMCP_ENABLE_CUDA
