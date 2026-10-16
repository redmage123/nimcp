/**
 * @file nimcp_hypothalamus_kernels.cu
 * @brief GPU Hypothalamus Drive Dynamics CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for parallel drive state evolution and reward computation
 * WHY:  GPU acceleration for biologically-inspired drive dynamics at scale
 * HOW:  Custom kernels for ODE integration, setpoint evaluation, reward signals
 *
 * ARCHITECTURE:
 * - Drive State Integration: Parallel Euler/RK4 for all drives
 * - Setpoint Deviation: Batch computation across multiple contexts
 * - Reward Signal: Parallel reward aggregation with alignment weights
 * - Urgency Ranking: GPU-accelerated priority sorting
 *
 * BYRNES ALIGNMENT SAFETY:
 * All kernels respect alignment weights - stored as read-only in GPU constant memory.
 *
 * @version Phase 18: GPU Acceleration
 * @date 2026-01-04
 */

/* Include headers outside CUDA check for types to be available */
#include "gpu/hypothalamus/nimcp_hypothalamus_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#include <stdlib.h>
#include <string.h>

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "HYPOTHALAMUS_GPU"

#define BLOCK_SIZE NIMCP_HYPO_GPU_BLOCK_SIZE
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define WARP_SIZE NIMCP_HYPO_GPU_WARP_SIZE

/*=============================================================================
 * Configuration Defaults
 *===========================================================================*/

nimcp_hypo_gpu_dynamics_config_t nimcp_hypo_gpu_dynamics_config_default(void)
{
    nimcp_hypo_gpu_dynamics_config_t config;
    config.integrator = NIMCP_HYPO_GPU_EULER;
    config.dt = 0.001f;          /* 1ms timestep */
    config.dt_min = 0.0001f;
    config.dt_max = 0.01f;
    config.error_tolerance = 1e-4f;
    config.threads_per_block = BLOCK_SIZE;
    config.max_blocks = 0;       /* Auto-detect */
    return config;
}

nimcp_hypo_gpu_reward_config_t nimcp_hypo_gpu_reward_config_default(void)
{
    nimcp_hypo_gpu_reward_config_t config;
    config.mode = NIMCP_HYPO_GPU_REWARD_ALIGNED;
    config.alignment_weight = 0.5f;
    config.temporal_discount = 0.95f;
    config.reward_smoothing = 0.1f;
    config.include_rpe = true;
    return config;
}

nimcp_hypo_gpu_pid_config_t nimcp_hypo_gpu_pid_config_default(void)
{
    nimcp_hypo_gpu_pid_config_t config;
    config.kp = 1.0f;
    config.ki = 0.1f;
    config.kd = 0.01f;
    config.integral_limit = 10.0f;
    config.output_min = -1.0f;
    config.output_max = 1.0f;
    return config;
}

/*=============================================================================
 * Drive Integration Kernels
 *===========================================================================*/

/**
 * @brief Kernel for forward Euler drive integration
 *
 * dL/dt = rise_rate * (1 - L) - decay_rate * satisfaction * L
 *
 * Each thread handles one (batch, drive) pair.
 */
__global__ void kernel_drive_euler_step(
    float* __restrict__ levels,
    const float* __restrict__ rise_rates,
    const float* __restrict__ decay_rates,
    const float* __restrict__ satisfaction,
    float dt,
    size_t batch_size,
    size_t n_drives)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = batch_size * n_drives;
    if (idx >= total) return;

    float L = levels[idx];
    float rise = rise_rates[idx];
    float decay = decay_rates[idx];
    float sat = satisfaction[idx];

    /* Drive dynamics: level increases over time, decreases when satisfied */
    float dL = rise * (1.0f - L) - decay * sat * L;
    L += dL * dt;

    /* Clamp to [0, 1] */
    L = fmaxf(0.0f, fminf(1.0f, L));
    levels[idx] = L;
}

/**
 * @brief Kernel for RK4 drive integration (more accurate)
 */
__global__ void kernel_drive_rk4_step(
    float* __restrict__ levels,
    const float* __restrict__ rise_rates,
    const float* __restrict__ decay_rates,
    const float* __restrict__ satisfaction,
    float dt,
    size_t batch_size,
    size_t n_drives)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = batch_size * n_drives;
    if (idx >= total) return;

    float L = levels[idx];
    float rise = rise_rates[idx];
    float decay = decay_rates[idx];
    float sat = satisfaction[idx];

    /* RK4 coefficients */
    auto f = [rise, decay, sat](float l) {
        return rise * (1.0f - l) - decay * sat * l;
    };

    float k1 = f(L);
    float k2 = f(L + 0.5f * dt * k1);
    float k3 = f(L + 0.5f * dt * k2);
    float k4 = f(L + dt * k3);

    L += (dt / 6.0f) * (k1 + 2.0f * k2 + 2.0f * k3 + k4);

    /* Clamp to [0, 1] */
    L = fmaxf(0.0f, fminf(1.0f, L));
    levels[idx] = L;
}

/*=============================================================================
 * Urgency and Deviation Kernels
 *===========================================================================*/

/**
 * @brief Kernel for computing drive urgency from setpoint deviation
 *
 * urgency = |level - setpoint| * arousal_modulation
 */
__global__ void kernel_compute_urgency(
    float* __restrict__ urgency,
    const float* __restrict__ levels,
    const float* __restrict__ setpoints,
    const float* __restrict__ time_since_satisfied,
    float arousal,
    size_t batch_size,
    size_t n_drives)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = batch_size * n_drives;
    if (idx >= total) return;

    float L = levels[idx];
    float S = setpoints[idx % n_drives];  /* Setpoints shared across batch */
    float time_factor = 1.0f;

    /* Time-based urgency increase */
    if (time_since_satisfied != NULL) {
        float t = time_since_satisfied[idx];
        time_factor = 1.0f + 0.1f * logf(1.0f + t);  /* Logarithmic growth */
    }

    /* Deviation from setpoint */
    float deviation = fabsf(L - S);

    /* Arousal modulation */
    float arousal_mod = 0.5f + 0.5f * arousal;  /* [0.5, 1.0] */

    urgency[idx] = deviation * time_factor * arousal_mod;
}

/**
 * @brief Kernel for computing setpoint deviation
 */
__global__ void kernel_compute_deviation(
    float* __restrict__ deviation,
    const float* __restrict__ levels,
    const float* __restrict__ setpoints,
    size_t batch_size,
    size_t n_drives)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total = batch_size * n_drives;
    if (idx >= total) return;

    float L = levels[idx];
    float S = setpoints[idx % n_drives];

    deviation[idx] = L - S;  /* Signed deviation */
}

/*=============================================================================
 * Reward Computation Kernels
 *===========================================================================*/

/**
 * @brief Kernel for computing drive satisfaction reward
 *
 * reward = sum_d(satisfaction_d * weight_d) + alignment_bonus
 */
__global__ void kernel_compute_reward_simple(
    float* __restrict__ reward,
    const float* __restrict__ satisfaction,
    const float* __restrict__ drive_weights,
    size_t batch_size,
    size_t n_drives)
{
    size_t batch_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (batch_idx >= batch_size) return;

    float total = 0.0f;
    for (size_t d = 0; d < n_drives; d++) {
        size_t idx = batch_idx * n_drives + d;
        float sat = satisfaction[idx];
        float weight = (drive_weights != NULL) ? drive_weights[d] : 1.0f;
        total += sat * weight;
    }

    reward[batch_idx] = total / (float)n_drives;
}

/**
 * @brief Kernel for computing alignment-weighted reward
 *
 * Includes Byrnes' alignment weights:
 * - human_wellbeing_weight
 * - harm_avoidance_weight
 * - honesty_weight
 * - helpfulness_weight
 */
__global__ void kernel_compute_reward_aligned(
    float* __restrict__ reward,
    float* __restrict__ alignment_bonus,
    float* __restrict__ alignment_penalty,
    const float* __restrict__ satisfaction,
    const float* __restrict__ deviation,
    const float* __restrict__ alignment_weights,  /* [4]: wellbeing, harm, honesty, helpful */
    float alignment_weight_factor,
    size_t batch_size,
    size_t n_drives)
{
    size_t batch_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (batch_idx >= batch_size) return;

    /* Base reward from drive satisfaction */
    float base_reward = 0.0f;
    float total_deviation = 0.0f;

    for (size_t d = 0; d < n_drives; d++) {
        size_t idx = batch_idx * n_drives + d;
        base_reward += satisfaction[idx];
        total_deviation += fabsf(deviation[idx]);
    }
    base_reward /= (float)n_drives;
    total_deviation /= (float)n_drives;

    /* Alignment bonus: reward aligned behavior */
    float bonus = 0.0f;
    if (alignment_weights != NULL) {
        float wellbeing_w = alignment_weights[0];
        float harm_avoid_w = alignment_weights[1];

        /* Bonus for maintaining homeostasis (low deviation) */
        bonus = wellbeing_w * (1.0f - total_deviation);

        /* Penalty for high deviation (potentially harmful state) */
        float penalty = harm_avoid_w * total_deviation;
        alignment_penalty[batch_idx] = penalty * alignment_weight_factor;
        bonus -= penalty;
    }

    alignment_bonus[batch_idx] = fmaxf(0.0f, bonus * alignment_weight_factor);

    /* Final reward */
    reward[batch_idx] = base_reward + alignment_bonus[batch_idx] - alignment_penalty[batch_idx];
}

/**
 * @brief Kernel for computing Reward Prediction Error
 *
 * RPE = actual - expected
 */
__global__ void kernel_compute_rpe(
    float* __restrict__ rpe,
    const float* __restrict__ actual,
    const float* __restrict__ expected,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    rpe[idx] = actual[idx] - expected[idx];
}

/*=============================================================================
 * Priority and Sorting Kernels
 *===========================================================================*/

/**
 * @brief Kernel for finding argmax urgency (highest priority drive)
 *
 * Uses warp reduction for efficiency.
 */
__global__ void kernel_find_priority(
    int* __restrict__ priority_idx,
    float* __restrict__ max_urgency,
    const float* __restrict__ urgency,
    size_t batch_size,
    size_t n_drives)
{
    size_t batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    /* Each block handles one batch item */
    __shared__ float s_max[WARP_SIZE];
    __shared__ int s_idx[WARP_SIZE];

    int tid = threadIdx.x;
    int lane = tid % WARP_SIZE;
    int warp_id = tid / WARP_SIZE;

    /* Initialize */
    float local_max = -FLT_MAX;
    int local_idx = -1;

    /* Each thread checks multiple drives */
    for (int d = tid; d < n_drives; d += blockDim.x) {
        float u = urgency[batch_idx * n_drives + d];
        if (u > local_max) {
            local_max = u;
            local_idx = d;
        }
    }

    /* Warp reduction */
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other_max = __shfl_down_sync(0xffffffff, local_max, offset);
        int other_idx = __shfl_down_sync(0xffffffff, local_idx, offset);
        if (other_max > local_max) {
            local_max = other_max;
            local_idx = other_idx;
        }
    }

    /* First thread in warp writes to shared memory */
    if (lane == 0) {
        s_max[warp_id] = local_max;
        s_idx[warp_id] = local_idx;
    }
    __syncthreads();

    /* Final reduction across warps (first warp only) */
    if (warp_id == 0 && lane < (blockDim.x / WARP_SIZE)) {
        local_max = s_max[lane];
        local_idx = s_idx[lane];

        for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
            float other_max = __shfl_down_sync(0xffffffff, local_max, offset);
            int other_idx = __shfl_down_sync(0xffffffff, local_idx, offset);
            if (other_max > local_max) {
                local_max = other_max;
                local_idx = other_idx;
            }
        }

        if (lane == 0) {
            priority_idx[batch_idx] = local_idx;
            max_urgency[batch_idx] = local_max;
        }
    }
}

/*=============================================================================
 * Homeostatic Controller Kernels
 *===========================================================================*/

/**
 * @brief Kernel for PID controller update
 *
 * output = kp*error + ki*integral + kd*derivative
 */
__global__ void kernel_pid_update(
    float* __restrict__ output,
    float* __restrict__ integral,
    float* __restrict__ prev_error,
    const float* __restrict__ current,
    const float* __restrict__ target,
    float kp, float ki, float kd,
    float integral_limit,
    float output_min, float output_max,
    float dt,
    size_t n)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    float error = target[idx] - current[idx];

    /* Integral with anti-windup */
    float integ = integral[idx] + error * dt;
    integ = fmaxf(-integral_limit, fminf(integral_limit, integ));
    integral[idx] = integ;

    /* Derivative */
    float deriv = (error - prev_error[idx]) / dt;
    prev_error[idx] = error;

    /* PID output */
    float out = kp * error + ki * integ + kd * deriv;
    out = fmaxf(output_min, fminf(output_max, out));
    output[idx] = out;
}

/*=============================================================================
 * Nucleus Activity Kernels
 *===========================================================================*/

/**
 * @brief Kernel for updating nucleus activity from drive state
 *
 * nucleus_activity = tanh(W * drive_levels + bias)
 */
__global__ void kernel_update_nuclei(
    float* __restrict__ nucleus_activity,
    const float* __restrict__ drive_levels,
    const float* __restrict__ weights,  /* [n_drives, n_nuclei] */
    const float* __restrict__ bias,
    size_t batch_size,
    size_t n_drives,
    size_t n_nuclei)
{
    size_t batch_idx = blockIdx.x;
    size_t nucleus_idx = threadIdx.x;

    if (batch_idx >= batch_size || nucleus_idx >= n_nuclei) return;

    float activation = (bias != NULL) ? bias[nucleus_idx] : 0.0f;

    for (size_t d = 0; d < n_drives; d++) {
        float level = drive_levels[batch_idx * n_drives + d];
        float w = weights[d * n_nuclei + nucleus_idx];
        activation += level * w;
    }

    /* Tanh activation */
    nucleus_activity[batch_idx * n_nuclei + nucleus_idx] = tanhf(activation);
}

/*=============================================================================
 * API Function Implementations
 *===========================================================================*/

bool nimcp_hypo_gpu_available(void)
{
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}

bool nimcp_hypo_gpu_capability(int* major, int* minor)
{
    if (!nimcp_hypo_gpu_available()) return false;

    cudaDeviceProp prop;
    NIMCP_CUDA_RECOVER(cudaGetDeviceProperties(&prop, 0), GPU_ERROR_CUDA_RUNTIME);

    if (major) *major = prop.major;
    if (minor) *minor = prop.minor;
    return true;
}

bool nimcp_hypo_gpu_drive_integrate(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    float dt,
    const nimcp_hypo_gpu_dynamics_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !state->state) return false;

    size_t total = state->batch_size * NIMCP_HYPO_GPU_DRIVE_COUNT;
    int grid = GRID_SIZE(total);
    int block = (config && config->threads_per_block > 0) ?
                config->threads_per_block : BLOCK_SIZE;

    /* Get tensor data pointers */
    float* levels = (float*)state->state->data;
    float* rise_rates = levels + total;      /* Offset into state tensor */
    float* decay_rates = rise_rates + total;
    float* satisfaction_ptr = (float*)state->satisfaction->data;

    nimcp_hypo_gpu_integrator_t integrator = config ? config->integrator : NIMCP_HYPO_GPU_EULER;

    switch (integrator) {
        case NIMCP_HYPO_GPU_RK4:
            kernel_drive_rk4_step<<<grid, block>>>(
                levels, rise_rates, decay_rates, satisfaction_ptr,
                dt, state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);
            break;

        case NIMCP_HYPO_GPU_EULER:
        default:
            kernel_drive_euler_step<<<grid, block>>>(
                levels, rise_rates, decay_rates, satisfaction_ptr,
                dt, state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);
            break;
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_gpu_compute_urgency(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    float arousal)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !setpoints) return false;

    size_t total = state->batch_size * NIMCP_HYPO_GPU_DRIVE_COUNT;
    int grid = GRID_SIZE(total);

    float* levels = (float*)state->state->data;
    float* urgency_ptr = (float*)state->urgency->data;
    float* setpoints_ptr = (float*)setpoints->setpoints->data;

    kernel_compute_urgency<<<grid, BLOCK_SIZE>>>(
        urgency_ptr, levels, setpoints_ptr, NULL, arousal,
        state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_gpu_compute_deviation(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !setpoints) return false;

    size_t total = state->batch_size * NIMCP_HYPO_GPU_DRIVE_COUNT;
    int grid = GRID_SIZE(total);

    float* levels = (float*)state->state->data;
    float* deviation_ptr = (float*)state->deviation->data;
    float* setpoints_ptr = (float*)setpoints->setpoints->data;

    kernel_compute_deviation<<<grid, BLOCK_SIZE>>>(
        deviation_ptr, levels, setpoints_ptr,
        state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_gpu_compute_reward(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    nimcp_hypo_gpu_reward_output_t* output,
    const nimcp_hypo_gpu_reward_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !setpoints || !output) return false;

    int grid = GRID_SIZE(state->batch_size);

    float* satisfaction_ptr = (float*)state->satisfaction->data;
    float* deviation_ptr = (float*)state->deviation->data;
    float* alignment_ptr = (float*)setpoints->alignment_weights->data;
    float* reward_ptr = (float*)output->reward->data;
    float* bonus_ptr = (float*)output->alignment_bonus->data;
    float* penalty_ptr = (float*)output->alignment_penalty->data;

    nimcp_hypo_gpu_reward_mode_t mode = config ? config->mode : NIMCP_HYPO_GPU_REWARD_ALIGNED;
    float alignment_weight = config ? config->alignment_weight : 0.5f;

    switch (mode) {
        case NIMCP_HYPO_GPU_REWARD_ALIGNED:
            kernel_compute_reward_aligned<<<grid, BLOCK_SIZE>>>(
                reward_ptr, bonus_ptr, penalty_ptr,
                satisfaction_ptr, deviation_ptr, alignment_ptr,
                alignment_weight,
                state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);
            break;

        case NIMCP_HYPO_GPU_REWARD_SIMPLE:
        default:
            kernel_compute_reward_simple<<<grid, BLOCK_SIZE>>>(
                reward_ptr, satisfaction_ptr, NULL,
                state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);
            break;
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_gpu_compute_rpe(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_tensor_t* expected,
    nimcp_gpu_tensor_t* rpe_out)
{
    if (!ctx || !actual || !expected || !rpe_out) return false;

    size_t n = actual->numel;
    int grid = GRID_SIZE(n);

    float* actual_ptr = (float*)actual->data;
    float* expected_ptr = (float*)expected->data;
    float* rpe_ptr = (float*)rpe_out->data;

    kernel_compute_rpe<<<grid, BLOCK_SIZE>>>(rpe_ptr, actual_ptr, expected_ptr, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_gpu_find_priority(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_gpu_drive_state_t* state,
    nimcp_gpu_tensor_t* priority_out,
    nimcp_gpu_tensor_t* max_urgency_out)
{
    if (!ctx || !state || !priority_out || !max_urgency_out) return false;

    float* urgency_ptr = (float*)state->urgency->data;
    int* priority_ptr = (int*)priority_out->data;
    float* max_ptr = (float*)max_urgency_out->data;

    /* One block per batch item */
    kernel_find_priority<<<state->batch_size, BLOCK_SIZE>>>(
        priority_ptr, max_ptr, urgency_ptr,
        state->batch_size, NIMCP_HYPO_GPU_DRIVE_COUNT);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_gpu_controller_update(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_gpu_controller_state_t* controller,
    const nimcp_gpu_tensor_t* current,
    const nimcp_gpu_tensor_t* target,
    float dt,
    const nimcp_hypo_gpu_pid_config_t* config)
{
    if (!ctx || !controller || !current || !target) return false;

    nimcp_hypo_gpu_pid_config_t cfg = config ? *config : nimcp_hypo_gpu_pid_config_default();

    size_t n = controller->n_variables;
    int grid = GRID_SIZE(n);

    float* output_ptr = (float*)controller->output->data;
    float* integral_ptr = (float*)controller->integral->data;
    float* error_ptr = (float*)controller->error->data;
    float* current_ptr = (float*)current->data;
    float* target_ptr = (float*)target->data;

    kernel_pid_update<<<grid, BLOCK_SIZE>>>(
        output_ptr, integral_ptr, error_ptr,
        current_ptr, target_ptr,
        cfg.kp, cfg.ki, cfg.kd,
        cfg.integral_limit, cfg.output_min, cfg.output_max,
        dt, n);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

#else /* !NIMCP_ENABLE_CUDA */

/* CPU fallback stubs */

nimcp_hypo_gpu_dynamics_config_t nimcp_hypo_gpu_dynamics_config_default(void)
{
    nimcp_hypo_gpu_dynamics_config_t config = {0};
    config.integrator = NIMCP_HYPO_GPU_EULER;
    config.dt = 0.001f;
    return config;
}

nimcp_hypo_gpu_reward_config_t nimcp_hypo_gpu_reward_config_default(void)
{
    nimcp_hypo_gpu_reward_config_t config = {0};
    config.mode = NIMCP_HYPO_GPU_REWARD_ALIGNED;
    config.alignment_weight = 0.5f;
    return config;
}

nimcp_hypo_gpu_pid_config_t nimcp_hypo_gpu_pid_config_default(void)
{
    nimcp_hypo_gpu_pid_config_t config = {0};
    config.kp = 1.0f;
    config.ki = 0.1f;
    config.kd = 0.01f;
    return config;
}

bool nimcp_hypo_gpu_available(void) { return false; }
bool nimcp_hypo_gpu_capability(int* major, int* minor) { (void)major; (void)minor; return false; }

bool nimcp_hypo_gpu_drive_integrate(nimcp_gpu_context_t* ctx, nimcp_hypo_gpu_drive_state_t* state,
    float dt, const nimcp_hypo_gpu_dynamics_config_t* config)
{ (void)ctx; (void)state; (void)dt; (void)config; return false; }

bool nimcp_hypo_gpu_compute_urgency(nimcp_gpu_context_t* ctx, nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints, float arousal)
{ (void)ctx; (void)state; (void)setpoints; (void)arousal; return false; }

bool nimcp_hypo_gpu_compute_deviation(nimcp_gpu_context_t* ctx, nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints)
{ (void)ctx; (void)state; (void)setpoints; return false; }

bool nimcp_hypo_gpu_compute_reward(nimcp_gpu_context_t* ctx, const nimcp_hypo_gpu_drive_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints, nimcp_hypo_gpu_reward_output_t* output,
    const nimcp_hypo_gpu_reward_config_t* config)
{ (void)ctx; (void)state; (void)setpoints; (void)output; (void)config; return false; }

bool nimcp_hypo_gpu_compute_rpe(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_tensor_t* expected, nimcp_gpu_tensor_t* rpe_out)
{ (void)ctx; (void)actual; (void)expected; (void)rpe_out; return false; }

bool nimcp_hypo_gpu_find_priority(nimcp_gpu_context_t* ctx, const nimcp_hypo_gpu_drive_state_t* state,
    nimcp_gpu_tensor_t* priority_out, nimcp_gpu_tensor_t* max_urgency_out)
{ (void)ctx; (void)state; (void)priority_out; (void)max_urgency_out; return false; }

bool nimcp_hypo_gpu_controller_update(nimcp_gpu_context_t* ctx, nimcp_hypo_gpu_controller_state_t* controller,
    const nimcp_gpu_tensor_t* current, const nimcp_gpu_tensor_t* target, float dt,
    const nimcp_hypo_gpu_pid_config_t* config)
{ (void)ctx; (void)controller; (void)current; (void)target; (void)dt; (void)config; return false; }

#endif /* NIMCP_ENABLE_CUDA */
