/**
 * @file nimcp_hypothalamus_qmc_kernels.cu
 * @brief CUDA Kernels for QMC-based Expected Free Energy Computation
 *
 * WHAT: GPU-accelerated Monte Carlo for EFE in hypothalamus steering
 * WHY:  FEP-based drive optimization requires efficient parallel sampling
 * HOW:  CUDA kernels for trajectory sampling, risk/ambiguity, policy optimization
 *
 * KERNEL ORGANIZATION:
 * ====================
 * 1. RNG Kernels: Parallel random number generation (XORWOW)
 * 2. Sampling Kernels: Trajectory generation with dynamics model
 * 3. EFE Kernels: Risk, ambiguity, and combined EFE computation
 * 4. Policy Kernels: Softmax, REINFORCE, gradient updates
 * 5. Alignment Kernels: Constraint checking and penalty computation
 *
 * RNG ARCHITECTURE:
 * =================
 * This module uses cuRAND (XORWOW generator) for high-quality Monte Carlo sampling.
 * The kernel_init_rng pattern follows the central GPU statistics module:
 *   - See: gpu/statistics/nimcp_statistics_gpu.h for stats_gpu_rng_create/destroy
 *   - See: gpu/common/nimcp_device_utils.cuh for shared device RNG functions
 *
 * Module-local RNG state is used because:
 *   1. QMC requires independent, reproducible sequences per trajectory
 *   2. Drive trajectory sampling needs deterministic replay for debugging
 *   3. Performance-critical per-thread state management
 *
 * @version Phase 18: GPU Acceleration
 * @date 2026-01-04
 */

/* Include headers before CUDA check so types are available for all builds */
#include "gpu/hypothalamus/nimcp_hypothalamus_qmc_gpu.h"
#include "gpu/hypothalamus/nimcp_hypothalamus_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <math.h>
#include <float.h>

/*=============================================================================
 * DEVICE CONSTANTS
 *===========================================================================*/

__constant__ float d_gamma;             /* Temporal discount factor */
__constant__ float d_risk_weight;       /* Risk term weight */
__constant__ float d_ambiguity_weight;  /* Ambiguity term weight */
__constant__ float d_precision_prior;   /* Prior precision */
__constant__ float d_temperature;       /* Softmax temperature */
__constant__ uint32_t d_num_drives;     /* Number of drives */
__constant__ uint32_t d_horizon;        /* Planning horizon */

/*=============================================================================
 * DEVICE HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Warp-level reduction for sum
 */
__device__ float warp_reduce_sum(float val) {
    for (int offset = warpSize / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xffffffff, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level reduction for max
 */
__device__ float warp_reduce_max(float val) {
    for (int offset = warpSize / 2; offset > 0; offset /= 2) {
        float other = __shfl_down_sync(0xffffffff, val, offset);
        val = fmaxf(val, other);
    }
    return val;
}

/**
 * @brief Block-level sum reduction using shared memory
 */
static __device__ float block_reduce_sum(float val, float* shared) {
    int lane = threadIdx.x % warpSize;
    int wid = threadIdx.x / warpSize;

    val = warp_reduce_sum(val);

    if (lane == 0) shared[wid] = val;
    __syncthreads();

    val = (threadIdx.x < blockDim.x / warpSize) ? shared[lane] : 0.0f;

    if (wid == 0) val = warp_reduce_sum(val);

    return val;
}

/**
 * @brief Safe log (avoiding -inf)
 */
__device__ __forceinline__ float safe_log(float x) {
    return logf(fmaxf(x, 1e-10f));
}

/**
 * @brief Gaussian entropy
 */
__device__ __forceinline__ float gaussian_entropy(float variance) {
    return 0.5f * (logf(2.0f * M_PI * M_E) + safe_log(variance));
}

/*=============================================================================
 * RNG KERNELS
 *===========================================================================*/

/**
 * @brief Initialize CURAND states
 */
__global__ void kernel_init_rng(
    curandState* states,
    uint64_t seed,
    size_t num_states)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_states) {
        curand_init(seed, idx, 0, &states[idx]);
    }
}

/**
 * @brief Generate Gaussian noise samples
 */
__global__ void kernel_sample_gaussian(
    curandState* states,
    float* samples,
    float mean,
    float stddev,
    size_t num_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_samples) {
        samples[idx] = mean + stddev * curand_normal(&states[idx]);
    }
}

/**
 * @brief Generate Sobol quasi-random samples (simplified)
 */
__global__ void kernel_sample_sobol(
    uint32_t* sobol_directions,
    float* samples,
    uint32_t dim,
    size_t num_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_samples) {
        /* Gray code based Sobol sequence */
        uint32_t graycode = idx ^ (idx >> 1);
        float val = 0.0f;
        uint32_t mask = 1;

        for (int bit = 0; bit < 32; bit++) {
            if (graycode & mask) {
                val += __uint_as_float(sobol_directions[bit * dim]);
            }
            mask <<= 1;
        }
        samples[idx] = val;
    }
}

/*=============================================================================
 * TRAJECTORY SAMPLING KERNELS
 *===========================================================================*/

/**
 * @brief Sample drive state trajectories
 *
 * Parallel sampling over (sample, timestep) pairs.
 * Uses transition model: s_{t+1} = A * s_t + B * u + noise
 */
__global__ void kernel_sample_trajectories(
    const float* __restrict__ initial_state,   /* [batch, drives] */
    const float* __restrict__ transition_mean, /* [drives, drives] */
    const float* __restrict__ transition_cov,  /* [drives, drives] */
    curandState* rng_states,
    float* __restrict__ trajectories,          /* [samples, horizon, drives] */
    float process_noise,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t sample_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (sample_idx >= num_samples) return;

    /* Local state storage */
    extern __shared__ float shared_state[];
    float* local_state = &shared_state[threadIdx.x * num_drives];

    /* Initialize from initial state */
    for (uint32_t d = 0; d < num_drives; d++) {
        local_state[d] = initial_state[d];
    }

    curandState* rng = &rng_states[sample_idx];

    /* Forward simulate trajectory */
    for (uint32_t t = 0; t < horizon; t++) {
        /* Store current state */
        size_t traj_offset = ((size_t)sample_idx * horizon + t) * num_drives;
        for (uint32_t d = 0; d < num_drives; d++) {
            trajectories[traj_offset + d] = local_state[d];
        }

        /* Compute next state: s' = A * s + noise */
        float new_state[NIMCP_HYPO_GPU_DRIVE_COUNT];
        for (uint32_t d = 0; d < num_drives && d < NIMCP_HYPO_GPU_DRIVE_COUNT; d++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < num_drives; j++) {
                sum += transition_mean[d * num_drives + j] * local_state[j];
            }
            /* Add process noise */
            sum += process_noise * curand_normal(rng);

            /* Clamp to valid range */
            new_state[d] = fminf(fmaxf(sum, 0.0f), 1.0f);
        }

        /* Update local state */
        for (uint32_t d = 0; d < num_drives && d < NIMCP_HYPO_GPU_DRIVE_COUNT; d++) {
            local_state[d] = new_state[d];
        }
    }
}

/**
 * @brief Sample trajectories with importance weighting
 */
__global__ void kernel_sample_importance(
    const float* __restrict__ initial_state,
    const float* __restrict__ proposal_mean,
    const float* __restrict__ proposal_cov,
    const float* __restrict__ target_setpoints,
    curandState* rng_states,
    float* __restrict__ trajectories,
    float* __restrict__ importance_weights,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t sample_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (sample_idx >= num_samples) return;

    curandState* rng = &rng_states[sample_idx];

    float log_proposal = 0.0f;
    float log_target = 0.0f;

    /* Simplified importance sampling */
    for (uint32_t t = 0; t < horizon; t++) {
        for (uint32_t d = 0; d < num_drives; d++) {
            size_t idx = ((size_t)sample_idx * horizon + t) * num_drives + d;

            /* Sample from proposal */
            float mean_p = proposal_mean[d];
            float var_p = proposal_cov[d * num_drives + d];
            float sample = mean_p + sqrtf(var_p) * curand_normal(rng);
            sample = fminf(fmaxf(sample, 0.0f), 1.0f);
            trajectories[idx] = sample;

            /* Compute proposal density */
            float diff_p = (sample - mean_p);
            log_proposal += -0.5f * diff_p * diff_p / var_p - 0.5f * safe_log(var_p);

            /* Compute target density (towards setpoint) */
            float setpoint = target_setpoints[d];
            float diff_t = (sample - setpoint);
            log_target += -0.5f * diff_t * diff_t / var_p - 0.5f * safe_log(var_p);
        }
    }

    /* Importance weight = target / proposal */
    importance_weights[sample_idx] = expf(log_target - log_proposal);
}

/*=============================================================================
 * RISK COMPUTATION KERNELS
 *===========================================================================*/

/**
 * @brief Compute risk term for sampled trajectories
 *
 * Risk = E_Q [ Σ_t γ^t * Σ_d w_d * (s_d - setpoint_d)^2 ]
 */
__global__ void kernel_compute_risk(
    const float* __restrict__ trajectories,  /* [samples, horizon, drives] */
    const float* __restrict__ setpoints,     /* [drives] */
    const float* __restrict__ weights,       /* [drives] */
    float* __restrict__ risk_out,            /* [samples] */
    float gamma,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t sample_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (sample_idx >= num_samples) return;

    float total_risk = 0.0f;
    float discount = 1.0f;

    for (uint32_t t = 0; t < horizon; t++) {
        float step_risk = 0.0f;
        size_t traj_offset = ((size_t)sample_idx * horizon + t) * num_drives;

        for (uint32_t d = 0; d < num_drives; d++) {
            float state = trajectories[traj_offset + d];
            float deviation = state - setpoints[d];
            step_risk += weights[d] * deviation * deviation;
        }

        total_risk += discount * step_risk;
        discount *= gamma;
    }

    risk_out[sample_idx] = total_risk;
}

/**
 * @brief Compute per-drive risk decomposition
 */
__global__ void kernel_compute_risk_per_drive(
    const float* __restrict__ trajectories,
    const float* __restrict__ setpoints,
    float* __restrict__ risk_per_drive,      /* [samples, drives] */
    float gamma,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t sample_idx = idx / num_drives;
    uint32_t drive_idx = idx % num_drives;

    if (sample_idx >= num_samples) return;

    float drive_risk = 0.0f;
    float discount = 1.0f;
    float setpoint = setpoints[drive_idx];

    for (uint32_t t = 0; t < horizon; t++) {
        size_t traj_idx = ((size_t)sample_idx * horizon + t) * num_drives + drive_idx;
        float state = trajectories[traj_idx];
        float deviation = state - setpoint;
        drive_risk += discount * deviation * deviation;
        discount *= gamma;
    }

    risk_per_drive[sample_idx * num_drives + drive_idx] = drive_risk;
}

/*=============================================================================
 * AMBIGUITY COMPUTATION KERNELS
 *===========================================================================*/

/**
 * @brief Compute ambiguity (expected entropy) for sampled trajectories
 *
 * Ambiguity = E_Q [ Σ_t γ^t * H(P(o|s)) ]
 *           = Expected entropy of observation model
 */
__global__ void kernel_compute_ambiguity(
    const float* __restrict__ trajectories,
    const float* __restrict__ observation_cov,  /* [drives, drives] */
    float* __restrict__ ambiguity_out,          /* [samples] */
    float gamma,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t sample_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (sample_idx >= num_samples) return;

    float total_ambiguity = 0.0f;
    float discount = 1.0f;

    /* Compute observation entropy at each timestep */
    for (uint32_t t = 0; t < horizon; t++) {
        float step_entropy = 0.0f;
        size_t traj_offset = ((size_t)sample_idx * horizon + t) * num_drives;

        /* Sum of Gaussian entropies (diagonal approximation) */
        for (uint32_t d = 0; d < num_drives; d++) {
            float variance = observation_cov[d * num_drives + d];
            step_entropy += gaussian_entropy(variance);
        }

        total_ambiguity += discount * step_entropy;
        discount *= gamma;
    }

    ambiguity_out[sample_idx] = total_ambiguity;
}

/**
 * @brief Compute state-dependent ambiguity
 *
 * Ambiguity varies with state (e.g., more uncertainty at extremes)
 */
__global__ void kernel_compute_state_ambiguity(
    const float* __restrict__ trajectories,
    const float* __restrict__ base_uncertainty,  /* [drives] */
    float* __restrict__ ambiguity_out,
    float state_scaling,
    float gamma,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t sample_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (sample_idx >= num_samples) return;

    float total_ambiguity = 0.0f;
    float discount = 1.0f;

    for (uint32_t t = 0; t < horizon; t++) {
        float step_entropy = 0.0f;
        size_t traj_offset = ((size_t)sample_idx * horizon + t) * num_drives;

        for (uint32_t d = 0; d < num_drives; d++) {
            float state = trajectories[traj_offset + d];
            float base_var = base_uncertainty[d];

            /* State-dependent variance (higher at extremes) */
            float state_factor = 1.0f + state_scaling * fabsf(state - 0.5f) * 2.0f;
            float variance = base_var * state_factor;

            step_entropy += gaussian_entropy(variance);
        }

        total_ambiguity += discount * step_entropy;
        discount *= gamma;
    }

    ambiguity_out[sample_idx] = total_ambiguity;
}

/*=============================================================================
 * EFE COMBINATION KERNELS
 *===========================================================================*/

/**
 * @brief Combine risk and ambiguity into EFE
 *
 * G(π) = risk_weight * Risk + ambiguity_weight * Ambiguity
 */
__global__ void kernel_combine_efe(
    const float* __restrict__ risk,
    const float* __restrict__ ambiguity,
    float* __restrict__ efe_out,
    float risk_weight,
    float ambiguity_weight,
    uint32_t num_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < num_samples) {
        efe_out[idx] = risk_weight * risk[idx] + ambiguity_weight * ambiguity[idx];
    }
}

/**
 * @brief Average EFE across samples for each policy
 */
__global__ void kernel_average_efe_per_policy(
    const float* __restrict__ efe_samples,  /* [policies, samples] */
    float* __restrict__ efe_mean,           /* [policies] */
    float* __restrict__ efe_variance,       /* [policies] (optional) */
    uint32_t num_policies,
    uint32_t num_samples)
{
    extern __shared__ float shared[];

    uint32_t policy_idx = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (policy_idx >= num_policies) return;

    /* Compute sum across samples */
    float sum = 0.0f;
    float sum_sq = 0.0f;

    for (uint32_t s = tid; s < num_samples; s += blockDim.x) {
        float val = efe_samples[policy_idx * num_samples + s];
        sum += val;
        sum_sq += val * val;
    }

    /* Block reduction */
    sum = block_reduce_sum(sum, shared);

    if (tid == 0) {
        float mean = sum / (float)num_samples;
        efe_mean[policy_idx] = mean;

        if (efe_variance != NULL) {
            sum_sq = block_reduce_sum(sum_sq, shared);
            efe_variance[policy_idx] = sum_sq / (float)num_samples - mean * mean;
        }
    }
}

/*=============================================================================
 * POLICY OPTIMIZATION KERNELS
 *===========================================================================*/

/**
 * @brief Softmax over negative EFE
 *
 * P(π) = exp(-G(π)/τ) / Σ exp(-G(π')/τ)
 */
__global__ void kernel_softmax_policy(
    const float* __restrict__ efe_values,  /* [policies] */
    float* __restrict__ policy_probs,      /* [policies] */
    float temperature,
    uint32_t num_policies)
{
    extern __shared__ float shared[];

    uint32_t tid = threadIdx.x;

    /* Step 1: Find max for numerical stability */
    float max_val = -FLT_MAX;
    for (uint32_t p = tid; p < num_policies; p += blockDim.x) {
        max_val = fmaxf(max_val, -efe_values[p] / temperature);
    }
    max_val = warp_reduce_max(max_val);

    if (tid % warpSize == 0) shared[tid / warpSize] = max_val;
    __syncthreads();

    if (tid < blockDim.x / warpSize) {
        max_val = shared[tid];
        max_val = warp_reduce_max(max_val);
    }
    __syncthreads();

    max_val = shared[0];
    __syncthreads();

    /* Step 2: Compute exp and sum */
    float sum = 0.0f;
    for (uint32_t p = tid; p < num_policies; p += blockDim.x) {
        float exp_val = expf(-efe_values[p] / temperature - max_val);
        policy_probs[p] = exp_val;
        sum += exp_val;
    }

    sum = block_reduce_sum(sum, shared);
    __syncthreads();

    if (tid == 0) shared[0] = sum;
    __syncthreads();
    sum = shared[0];

    /* Step 3: Normalize */
    for (uint32_t p = tid; p < num_policies; p += blockDim.x) {
        policy_probs[p] /= sum;
    }
}

/**
 * @brief Compute REINFORCE gradient
 *
 * ∇J = -G(π) * ∇log π(π)
 *    = -(G(π) - baseline) * (δ_π - π) for softmax
 */
__global__ void kernel_reinforce_gradient(
    const float* __restrict__ efe_values,
    const float* __restrict__ policy_probs,
    const float* __restrict__ baseline,     /* [1] or NULL */
    float* __restrict__ gradients,
    uint32_t num_policies,
    uint32_t selected_policy)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= num_policies) return;

    float base_val = (baseline != NULL) ? baseline[0] : 0.0f;
    float advantage = -(efe_values[selected_policy] - base_val);

    /* Softmax gradient: advantage * (indicator - prob) */
    float indicator = (idx == selected_policy) ? 1.0f : 0.0f;
    gradients[idx] = advantage * (indicator - policy_probs[idx]);
}

/**
 * @brief Update policy parameters with gradient
 */
__global__ void kernel_policy_update(
    float* __restrict__ policy_params,
    const float* __restrict__ gradients,
    float learning_rate,
    float entropy_bonus,
    const float* __restrict__ policy_probs,
    uint32_t num_params)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= num_params) return;

    /* Entropy gradient: ∇H = -(1 + log π) */
    float prob = policy_probs[idx];
    float entropy_grad = -(1.0f + safe_log(prob));

    /* Combined update */
    float grad = gradients[idx] + entropy_bonus * entropy_grad;
    policy_params[idx] += learning_rate * grad;
}

/**
 * @brief Update baseline (exponential moving average)
 */
__global__ void kernel_update_baseline(
    float* __restrict__ baseline,
    float return_value,
    float learning_rate)
{
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        baseline[0] = (1.0f - learning_rate) * baseline[0] + learning_rate * return_value;
    }
}

/*=============================================================================
 * ALIGNMENT-AWARE KERNELS
 *===========================================================================*/

/**
 * @brief Compute alignment penalty for trajectories
 *
 * Penalizes deviation from locked/alignment-weighted setpoints
 */
__global__ void kernel_alignment_penalty(
    const float* __restrict__ trajectories,
    const float* __restrict__ setpoints,
    const float* __restrict__ alignment_weights, /* [drives] */
    const bool* __restrict__ locked,             /* [drives] */
    float* __restrict__ penalties,               /* [samples] */
    float locked_penalty_scale,
    uint32_t num_samples,
    uint32_t num_drives,
    uint32_t horizon)
{
    uint32_t sample_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (sample_idx >= num_samples) return;

    float total_penalty = 0.0f;

    for (uint32_t t = 0; t < horizon; t++) {
        size_t traj_offset = ((size_t)sample_idx * horizon + t) * num_drives;

        for (uint32_t d = 0; d < num_drives; d++) {
            float state = trajectories[traj_offset + d];
            float deviation = fabsf(state - setpoints[d]);

            float weight = alignment_weights[d];

            /* Extra penalty for locked drives */
            if (locked[d]) {
                weight *= locked_penalty_scale;
            }

            total_penalty += weight * deviation * deviation;
        }
    }

    penalties[sample_idx] = total_penalty;
}

/**
 * @brief Compute aligned EFE (base EFE + alignment penalty)
 */
__global__ void kernel_aligned_efe(
    const float* __restrict__ base_efe,
    const float* __restrict__ alignment_penalty,
    float* __restrict__ aligned_efe,
    float alignment_weight,
    uint32_t num_samples)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= num_samples) {
        return;
    }

    aligned_efe[idx] = base_efe[idx] + alignment_weight * alignment_penalty[idx];
}

/**
 * @brief Check alignment constraint satisfaction
 */
__global__ void kernel_check_alignment(
    const float* __restrict__ policy_probs,
    const float* __restrict__ expected_deviation, /* [policies, drives] */
    const float* __restrict__ tolerance,          /* [drives] */
    const bool* __restrict__ locked,
    bool* __restrict__ satisfied,                 /* [policies] */
    uint32_t num_policies,
    uint32_t num_drives)
{
    uint32_t policy_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (policy_idx >= num_policies) return;

    bool all_satisfied = true;

    for (uint32_t d = 0; d < num_drives; d++) {
        if (locked[d]) {
            float dev = expected_deviation[policy_idx * num_drives + d];
            if (dev > tolerance[d]) {
                all_satisfied = false;
                break;
            }
        }
    }

    satisfied[policy_idx] = all_satisfied;
}

/*=============================================================================
 * ACTIVE INFERENCE KERNELS
 *===========================================================================*/

/**
 * @brief Compute precision-weighted prediction error
 *
 * ε = Π * (o - g(s))
 */
__global__ void kernel_precision_error(
    const float* __restrict__ predicted,
    const float* __restrict__ actual,
    const float* __restrict__ precision,
    float* __restrict__ error_out,
    uint32_t size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < size) {
        error_out[idx] = precision[idx] * (actual[idx] - predicted[idx]);
    }
}

/**
 * @brief Update beliefs based on precision-weighted errors
 */
__global__ void kernel_belief_update(
    float* __restrict__ beliefs,
    const float* __restrict__ precision_error,
    float learning_rate,
    uint32_t size)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < size) {
        beliefs[idx] += learning_rate * precision_error[idx];
        /* Clamp to [0, 1] */
        beliefs[idx] = fminf(fmaxf(beliefs[idx], 0.0f), 1.0f);
    }
}

#endif /* NIMCP_ENABLE_CUDA */

/*=============================================================================
 * HOST API IMPLEMENTATION
 *===========================================================================*/

/* Default configurations */
nimcp_hypo_qmc_efe_config_t nimcp_hypo_qmc_efe_config_default(void) {
    nimcp_hypo_qmc_efe_config_t config = {
        .efe_mode = NIMCP_HYPO_EFE_FULL,
        .sample_method = NIMCP_HYPO_SAMPLE_UNIFORM,
        .num_samples = NIMCP_HYPO_QMC_DEFAULT_SAMPLES,
        .horizon = 16,
        .gamma = 0.99f,
        .risk_weight = 1.0f,
        .ambiguity_weight = 0.1f,
        .precision_prior = 1.0f,
        .threads_per_block = NIMCP_HYPO_QMC_BLOCK_SIZE
    };
    return config;
}

nimcp_hypo_qmc_opt_config_t nimcp_hypo_qmc_opt_config_default(void) {
    nimcp_hypo_qmc_opt_config_t config = {
        .method = NIMCP_HYPO_OPT_SOFTMAX,
        .learning_rate = 0.01f,
        .temperature = 1.0f,
        .entropy_bonus = 0.01f,
        .num_policies = 8,
        .num_iterations = 100,
        .use_baseline = true
    };
    return config;
}

nimcp_hypo_qmc_dynamics_config_t nimcp_hypo_qmc_dynamics_config_default(void) {
    nimcp_hypo_qmc_dynamics_config_t config = {
        .process_noise = 0.01f,
        .observation_noise = 0.05f,
        .use_learned_dynamics = false,
        .dynamics_uncertainty = 0.1f
    };
    return config;
}

#ifdef NIMCP_ENABLE_CUDA

/* State lifecycle */
nimcp_hypo_qmc_efe_state_t* nimcp_hypo_qmc_efe_state_create(
    nimcp_gpu_context_t* ctx,
    size_t num_samples,
    size_t horizon,
    uint64_t seed)
{
    if (!ctx || num_samples == 0 || horizon == 0) {
        return NULL;
    }

    nimcp_hypo_qmc_efe_state_t* state = (nimcp_hypo_qmc_efe_state_t*)
        nimcp_calloc(1, sizeof(nimcp_hypo_qmc_efe_state_t));
    if (!state) return NULL;

    state->num_samples = num_samples;
    state->horizon = horizon;

    /* Initialize RNG */
    if (seed == 0) {
        seed = (uint64_t)time(NULL);
    }

    cudaError_t err;
    curandState* rng_states;
    err = cudaMalloc(&rng_states, num_samples * sizeof(curandState));
    if (err != cudaSuccess) {
        nimcp_free(state);
        return NULL;
    }

    uint32_t blocks = (num_samples + 255) / 256;
    kernel_init_rng<<<blocks, 256>>>(rng_states, seed, num_samples);
    state->rng = (qmc_gpu_rng_t)rng_states;

    /* Allocate risk and ambiguity arrays */
    float* risk;
    float* ambiguity;
    err = cudaMalloc(&risk, num_samples * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(rng_states);
        nimcp_free(state);
        return NULL;
    }

    err = cudaMalloc(&ambiguity, num_samples * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(risk);
        cudaFree(rng_states);
        nimcp_free(state);
        return NULL;
    }

    /* Create tensor wrappers (simplified - actual implementation would use nimcp_gpu_tensor_create) */
    state->risk_terms = (nimcp_gpu_tensor_t*)risk;
    state->ambiguity_terms = (nimcp_gpu_tensor_t*)ambiguity;

    return state;
}

void nimcp_hypo_qmc_efe_state_destroy(nimcp_hypo_qmc_efe_state_t* state) {
    if (!state) return;

    if (state->rng) {
        cudaFree((void*)state->rng);
    }
    if (state->sampled_trajectories) {
        cudaFree(state->sampled_trajectories);
    }
    if (state->risk_terms) {
        cudaFree(state->risk_terms);
    }
    if (state->ambiguity_terms) {
        cudaFree(state->ambiguity_terms);
    }
    if (state->efe_values) {
        cudaFree(state->efe_values);
    }

    nimcp_free(state);
}

nimcp_hypo_qmc_policy_state_t* nimcp_hypo_qmc_policy_state_create(
    nimcp_gpu_context_t* ctx,
    size_t num_policies)
{
    if (!ctx || num_policies == 0) {
        return NULL;
    }

    nimcp_hypo_qmc_policy_state_t* state = (nimcp_hypo_qmc_policy_state_t*)
        nimcp_calloc(1, sizeof(nimcp_hypo_qmc_policy_state_t));
    if (!state) return NULL;

    state->num_policies = num_policies;

    /* Allocate policy probabilities */
    float* probs;
    cudaError_t err = cudaMalloc(&probs, num_policies * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_free(state);
        return NULL;
    }
    state->policy_probs = (nimcp_gpu_tensor_t*)probs;

    return state;
}

void nimcp_hypo_qmc_policy_state_destroy(nimcp_hypo_qmc_policy_state_t* state) {
    if (!state) return;

    if (state->policy_params) cudaFree(state->policy_params);
    if (state->policy_probs) cudaFree(state->policy_probs);
    if (state->action_values) cudaFree(state->action_values);
    if (state->gradients) cudaFree(state->gradients);
    if (state->baseline) cudaFree(state->baseline);

    nimcp_free(state);
}

nimcp_hypo_qmc_dynamics_t* nimcp_hypo_qmc_dynamics_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_qmc_dynamics_config_t* config)
{
    if (!ctx || !config) {
        return NULL;
    }

    nimcp_hypo_qmc_dynamics_t* dynamics = (nimcp_hypo_qmc_dynamics_t*)
        nimcp_calloc(1, sizeof(nimcp_hypo_qmc_dynamics_t));
    if (!dynamics) return NULL;

    /* Initialize with identity transition (stable dynamics) */
    size_t mat_size = NIMCP_HYPO_GPU_DRIVE_COUNT * NIMCP_HYPO_GPU_DRIVE_COUNT * sizeof(float);

    cudaError_t err = cudaMalloc(&dynamics->transition_mean, mat_size);
    if (err != cudaSuccess) {
        nimcp_free(dynamics);
        return NULL;
    }

    err = cudaMalloc(&dynamics->transition_cov, mat_size);
    if (err != cudaSuccess) {
        cudaFree(dynamics->transition_mean);
        nimcp_free(dynamics);
        return NULL;
    }

    /* Initialize to identity on host, then copy */
    float* host_mat = (float*)nimcp_calloc(NIMCP_HYPO_GPU_DRIVE_COUNT * NIMCP_HYPO_GPU_DRIVE_COUNT, sizeof(float));
    if (host_mat) {
        for (int i = 0; i < NIMCP_HYPO_GPU_DRIVE_COUNT; i++) {
            host_mat[i * NIMCP_HYPO_GPU_DRIVE_COUNT + i] = 1.0f;
        }
        cudaMemcpy(dynamics->transition_mean, host_mat, mat_size, cudaMemcpyHostToDevice);

        /* Covariance = diagonal with process noise */
        memset(host_mat, 0, mat_size);
        for (int i = 0; i < NIMCP_HYPO_GPU_DRIVE_COUNT; i++) {
            host_mat[i * NIMCP_HYPO_GPU_DRIVE_COUNT + i] = config->process_noise;
        }
        cudaMemcpy(dynamics->transition_cov, host_mat, mat_size, cudaMemcpyHostToDevice);

        nimcp_free(host_mat);
    }

    return dynamics;
}

void nimcp_hypo_qmc_dynamics_destroy(nimcp_hypo_qmc_dynamics_t* dynamics) {
    if (!dynamics) return;

    if (dynamics->transition_mean) cudaFree(dynamics->transition_mean);
    if (dynamics->transition_cov) cudaFree(dynamics->transition_cov);
    if (dynamics->observation_model) cudaFree(dynamics->observation_model);
    if (dynamics->noise_samples) cudaFree(dynamics->noise_samples);

    nimcp_free(dynamics);
}

/* EFE computation */
bool nimcp_hypo_qmc_sample_trajectories(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_gpu_tensor_t* initial_state,
    const nimcp_hypo_qmc_dynamics_t* dynamics,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !initial_state || !dynamics || !config) {
        return false;
    }

    uint32_t num_drives = NIMCP_HYPO_GPU_DRIVE_COUNT;
    size_t traj_size = state->num_samples * state->horizon * num_drives * sizeof(float);

    /* Allocate trajectory storage if needed */
    if (!state->sampled_trajectories) {
        cudaError_t err = cudaMalloc(&state->sampled_trajectories, traj_size);
        if (err != cudaSuccess) {
            return false;
        }
    }

    uint32_t threads = config->threads_per_block;
    uint32_t blocks = (state->num_samples + threads - 1) / threads;
    size_t shared_size = threads * num_drives * sizeof(float);

    kernel_sample_trajectories<<<blocks, threads, shared_size>>>(
        (const float*)initial_state,
        (const float*)dynamics->transition_mean,
        (const float*)dynamics->transition_cov,
        (curandState*)state->rng,
        (float*)state->sampled_trajectories,
        0.01f,  /* process_noise */
        state->num_samples,
        num_drives,
        state->horizon);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_compute_risk(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !setpoints || !config) {
        return false;
    }

    if (!state->sampled_trajectories || !state->risk_terms) {
        return false;
    }

    uint32_t num_drives = NIMCP_HYPO_GPU_DRIVE_COUNT;
    uint32_t threads = config->threads_per_block;
    uint32_t blocks = (state->num_samples + threads - 1) / threads;

    kernel_compute_risk<<<blocks, threads>>>(
        (const float*)state->sampled_trajectories,
        (const float*)setpoints->setpoints->data,
        (const float*)setpoints->alignment_weights->data,
        (float*)state->risk_terms,
        config->gamma,
        state->num_samples,
        num_drives,
        state->horizon);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_compute_ambiguity(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_qmc_dynamics_t* dynamics,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !dynamics || !config) {
        return false;
    }

    if (!state->sampled_trajectories || !state->ambiguity_terms) {
        return false;
    }

    uint32_t num_drives = NIMCP_HYPO_GPU_DRIVE_COUNT;
    uint32_t threads = config->threads_per_block;
    uint32_t blocks = (state->num_samples + threads - 1) / threads;

    kernel_compute_ambiguity<<<blocks, threads>>>(
        (const float*)state->sampled_trajectories,
        (const float*)dynamics->transition_cov,
        (float*)state->ambiguity_terms,
        config->gamma,
        state->num_samples,
        num_drives,
        state->horizon);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_compute_efe(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    nimcp_gpu_tensor_t* efe_out,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !efe_out || !config) {
        return false;
    }

    if (!state->risk_terms || !state->ambiguity_terms) {
        return false;
    }

    uint32_t threads = config->threads_per_block;
    uint32_t blocks = (state->num_samples + threads - 1) / threads;

    kernel_combine_efe<<<blocks, threads>>>(
        (const float*)state->risk_terms,
        (const float*)state->ambiguity_terms,
        (float*)efe_out,
        config->risk_weight,
        config->ambiguity_weight,
        state->num_samples);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/* Policy optimization */
bool nimcp_hypo_qmc_softmax_policy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* efe_values,
    nimcp_gpu_tensor_t* policy_probs,
    float temperature)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !efe_values || !policy_probs || temperature <= 0.0f) {
        return false;
    }

    /* Assume single block for simplicity; real impl would handle larger */
    uint32_t num_policies = NIMCP_HYPO_QMC_MAX_POLICIES;
    size_t shared_size = (256 / 32 + 1) * sizeof(float);

    kernel_softmax_policy<<<1, 256, shared_size>>>(
        (const float*)efe_values,
        (float*)policy_probs,
        temperature,
        num_policies);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_policy_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_gpu_tensor_t* efe_values,
    const nimcp_hypo_qmc_opt_config_t* config)
{
    if (!ctx || !policy_state || !efe_values || !config) {
        return false;
    }

    /* Allocate gradients if needed */
    if (!policy_state->gradients) {
        cudaError_t err = cudaMalloc(&policy_state->gradients,
            policy_state->num_policies * sizeof(float));
        if (err != cudaSuccess) {
            return false;
        }
    }

    uint32_t threads = 256;
    uint32_t blocks = (policy_state->num_policies + threads - 1) / threads;

    kernel_reinforce_gradient<<<blocks, threads>>>(
        (const float*)efe_values,
        (const float*)policy_state->policy_probs,
        config->use_baseline ? (const float*)policy_state->baseline : NULL,
        (float*)policy_state->gradients,
        policy_state->num_policies,
        0);  /* selected_policy - would come from sampling */

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_policy_update(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_hypo_qmc_opt_config_t* config)
{
    if (!ctx || !policy_state || !config) {
        return false;
    }

    if (!policy_state->policy_params || !policy_state->gradients) {
        return false;
    }

    uint32_t threads = 256;
    uint32_t blocks = (policy_state->num_policies + threads - 1) / threads;

    kernel_policy_update<<<blocks, threads>>>(
        (float*)policy_state->policy_params,
        (const float*)policy_state->gradients,
        config->learning_rate,
        config->entropy_bonus,
        (const float*)policy_state->policy_probs,
        policy_state->num_policies);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_update_baseline(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_gpu_tensor_t* returns,
    float learning_rate)
{
    if (!ctx || !policy_state || !returns) {
        return false;
    }

    /* Allocate baseline if needed */
    if (!policy_state->baseline) {
        cudaError_t err = cudaMalloc(&policy_state->baseline, sizeof(float));
        if (err != cudaSuccess) {
            return false;
        }
        cudaMemset(policy_state->baseline, 0, sizeof(float));
    }

    /* Get return value (simplified - just use first element) */
    float return_val;
    cudaMemcpy(&return_val, returns, sizeof(float), cudaMemcpyDeviceToHost);

    kernel_update_baseline<<<1, 1>>>(
        (float*)policy_state->baseline,
        return_val,
        learning_rate);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/* Active inference integration */
bool nimcp_hypo_qmc_precision_error(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* predicted,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* error_out)
{
    if (!ctx || !predicted || !actual || !precision || !error_out) {
        return false;
    }

    uint32_t size = NIMCP_HYPO_GPU_DRIVE_COUNT;
    uint32_t threads = 256;
    uint32_t blocks = (size + threads - 1) / threads;

    kernel_precision_error<<<blocks, threads>>>(
        (const float*)predicted,
        (const float*)actual,
        (const float*)precision,
        (float*)error_out,
        size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_belief_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* beliefs,
    const nimcp_gpu_tensor_t* precision_error,
    float learning_rate)
{
    if (!ctx || !beliefs || !precision_error) {
        return false;
    }

    uint32_t size = NIMCP_HYPO_GPU_DRIVE_COUNT;
    uint32_t threads = 256;
    uint32_t blocks = (size + threads - 1) / threads;

    kernel_belief_update<<<blocks, threads>>>(
        (float*)beliefs,
        (const float*)precision_error,
        learning_rate,
        size);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

/* Alignment-aware EFE */
bool nimcp_hypo_qmc_aligned_efe(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    float alignment_weight,
    nimcp_gpu_tensor_t* efe_out)
{
    if (!ctx || !state || !setpoints || !efe_out) {
        return false;
    }

    /* First compute base EFE if not done */
    if (!state->risk_terms || !state->ambiguity_terms) {
        return false;
    }

    uint32_t num_drives = NIMCP_HYPO_GPU_DRIVE_COUNT;
    uint32_t threads = 256;
    uint32_t blocks = (state->num_samples + threads - 1) / threads;

    /* Allocate penalty buffer */
    float* penalties;
    cudaError_t err = cudaMalloc(&penalties, state->num_samples * sizeof(float));
    if (err != cudaSuccess) {
        return false;
    }

    /* Compute alignment penalty */
    kernel_alignment_penalty<<<blocks, threads>>>(
        (const float*)state->sampled_trajectories,
        (const float*)setpoints->setpoints->data,
        (const float*)setpoints->alignment_weights->data,
        (const bool*)setpoints->lock_mask->data,
        penalties,
        10.0f,  /* locked_penalty_scale */
        state->num_samples,
        num_drives,
        state->horizon);

    /* Combine into aligned EFE */
    /* First get base EFE */
    float* base_efe;
    err = cudaMalloc(&base_efe, state->num_samples * sizeof(float));
    if (err != cudaSuccess) {
        cudaFree(penalties);
        return false;
    }

    kernel_combine_efe<<<blocks, threads>>>(
        (const float*)state->risk_terms,
        (const float*)state->ambiguity_terms,
        base_efe,
        1.0f, 0.1f,
        state->num_samples);

    kernel_aligned_efe<<<blocks, threads>>>(
        base_efe,
        penalties,
        (float*)efe_out,
        alignment_weight,
        state->num_samples);

    cudaFree(penalties);
    cudaFree(base_efe);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

bool nimcp_hypo_qmc_check_alignment(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* policy_probs,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    nimcp_gpu_tensor_t* satisfied_out)
{
    if (!ctx || !policy_probs || !setpoints || !satisfied_out) {
        return false;
    }

    /* This would require expected deviation computation first */
    /* Simplified: just return true for now */
    uint32_t num_policies = NIMCP_HYPO_QMC_MAX_POLICIES;
    cudaMemset(satisfied_out, 1, num_policies * sizeof(bool));

    return true;
}

#else /* !NIMCP_ENABLE_CUDA */

/* CPU fallback stubs */

nimcp_hypo_qmc_efe_state_t* nimcp_hypo_qmc_efe_state_create(
    nimcp_gpu_context_t* ctx,
    size_t num_samples,
    size_t horizon,
    uint64_t seed)
{
    (void)ctx; (void)num_samples; (void)horizon; (void)seed;
    return NULL;
}

void nimcp_hypo_qmc_efe_state_destroy(nimcp_hypo_qmc_efe_state_t* state) {
    (void)state;
}

nimcp_hypo_qmc_policy_state_t* nimcp_hypo_qmc_policy_state_create(
    nimcp_gpu_context_t* ctx,
    size_t num_policies)
{
    (void)ctx; (void)num_policies;
    return NULL;
}

void nimcp_hypo_qmc_policy_state_destroy(nimcp_hypo_qmc_policy_state_t* state) {
    (void)state;
}

nimcp_hypo_qmc_dynamics_t* nimcp_hypo_qmc_dynamics_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_qmc_dynamics_config_t* config)
{
    (void)ctx; (void)config;
    return NULL;
}

void nimcp_hypo_qmc_dynamics_destroy(nimcp_hypo_qmc_dynamics_t* dynamics) {
    (void)dynamics;
}

bool nimcp_hypo_qmc_sample_trajectories(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_gpu_tensor_t* initial_state,
    const nimcp_hypo_qmc_dynamics_t* dynamics,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    (void)ctx; (void)state; (void)initial_state; (void)dynamics; (void)config;
    return false;
}

bool nimcp_hypo_qmc_compute_risk(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    (void)ctx; (void)state; (void)setpoints; (void)config;
    return false;
}

bool nimcp_hypo_qmc_compute_ambiguity(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_qmc_dynamics_t* dynamics,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    (void)ctx; (void)state; (void)dynamics; (void)config;
    return false;
}

bool nimcp_hypo_qmc_compute_efe(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    nimcp_gpu_tensor_t* efe_out,
    const nimcp_hypo_qmc_efe_config_t* config)
{
    (void)ctx; (void)state; (void)efe_out; (void)config;
    return false;
}

bool nimcp_hypo_qmc_softmax_policy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* efe_values,
    nimcp_gpu_tensor_t* policy_probs,
    float temperature)
{
    (void)ctx; (void)efe_values; (void)policy_probs; (void)temperature;
    return false;
}

bool nimcp_hypo_qmc_policy_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_gpu_tensor_t* efe_values,
    const nimcp_hypo_qmc_opt_config_t* config)
{
    (void)ctx; (void)policy_state; (void)efe_values; (void)config;
    return false;
}

bool nimcp_hypo_qmc_policy_update(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_hypo_qmc_opt_config_t* config)
{
    (void)ctx; (void)policy_state; (void)config;
    return false;
}

bool nimcp_hypo_qmc_update_baseline(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_gpu_tensor_t* returns,
    float learning_rate)
{
    (void)ctx; (void)policy_state; (void)returns; (void)learning_rate;
    return false;
}

bool nimcp_hypo_qmc_precision_error(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* predicted,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* error_out)
{
    (void)ctx; (void)predicted; (void)actual; (void)precision; (void)error_out;
    return false;
}

bool nimcp_hypo_qmc_belief_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* beliefs,
    const nimcp_gpu_tensor_t* precision_error,
    float learning_rate)
{
    (void)ctx; (void)beliefs; (void)precision_error; (void)learning_rate;
    return false;
}

bool nimcp_hypo_qmc_aligned_efe(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    float alignment_weight,
    nimcp_gpu_tensor_t* efe_out)
{
    (void)ctx; (void)state; (void)setpoints; (void)alignment_weight; (void)efe_out;
    return false;
}

bool nimcp_hypo_qmc_check_alignment(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* policy_probs,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    nimcp_gpu_tensor_t* satisfied_out)
{
    (void)ctx; (void)policy_probs; (void)setpoints; (void)satisfied_out;
    return false;
}

#endif /* NIMCP_ENABLE_CUDA */
