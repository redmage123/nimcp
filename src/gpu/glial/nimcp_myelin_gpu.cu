/**
 * @file nimcp_myelin_gpu.cu
 * @brief GPU CUDA Kernels for Myelin Sheath Biophysics
 *
 * WHAT: CUDA kernels for GPU-accelerated myelin biophysics computations
 * WHY:  GPU acceleration for parallel myelin network simulation
 * HOW:  Custom kernels for g-ratio, cable theory, saltatory conduction, plasticity
 *
 * BIOLOGICAL BASIS:
 * =================
 * Myelin sheaths enable saltatory conduction through:
 * - Optimal G-ratio: Rushton's theory (g_opt ~ 0.77 for large axons)
 * - Cable theory: Space constant lambda, time constant tau
 * - Saltatory conduction: AP jumps between nodes of Ranvier
 * - Activity-dependent plasticity: Neural activity modulates myelination
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Myelin computation benefits from massive parallelism:
 * - Per-axon g-ratio: O(1) per thread, N threads
 * - Per-segment cable params: O(1) per thread, N*M threads
 * - Velocity aggregation: Parallel reduction
 * - Plasticity: All axons updated simultaneously
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "gpu/glial/nimcp_myelin_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "MYELIN_GPU"

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Device Constants
//=============================================================================

// G-ratio optimization constants
__constant__ float d_G_RATIO_BASE = 0.77f;
__constant__ float d_G_RATIO_ALPHA = 0.08f;
__constant__ float d_D_CRITICAL = 0.5f;
__constant__ float d_G_RATIO_MIN = 0.4f;
__constant__ float d_G_RATIO_MAX = 0.95f;

// Cable theory constants
__constant__ float d_R_M_BASE = 1000.0f;      // Base membrane resistance (Ohm*cm^2)
__constant__ float d_R_A_CYTOPLASM = 100.0f;  // Axoplasmic resistivity (Ohm*cm)
__constant__ float d_C_M_BASE = 1.0f;         // Base membrane capacitance (uF/cm^2)
__constant__ float d_R_M_PER_LAMELLA = 100.0f;
__constant__ float d_C_M_REDUCTION = 0.9f;

// Saltatory conduction constants
__constant__ float d_TAU_NODE_MS = 0.03f;     // Node delay (ms)
__constant__ float d_V_PASSIVE = 1000.0f;     // Passive propagation (um/ms)
__constant__ float d_V_MIN_MS = 0.5f;         // Minimum velocity (m/s)
__constant__ float d_V_MAX_MS = 150.0f;       // Maximum velocity (m/s)
__constant__ float d_VELOCITY_COEFF = 6.0f;   // Hursh's law coefficient

// Block probability constants
__constant__ float d_I_CRITICAL = 0.4f;       // 50% block integrity threshold
__constant__ float d_BLOCK_SIGMA = 0.1f;      // Transition steepness
__constant__ float d_T_REF = 37.0f;           // Reference temperature (C)
__constant__ float d_T_SENSITIVITY = 0.05f;   // Temperature sensitivity

// Plasticity constants
__constant__ float d_K_DEMYELIN = 0.1f;       // Demyelination rate constant
__constant__ float d_K_DEMYELIN_HALF = 0.1f;  // Half-max for demyelination
__constant__ float d_SATURATION_LAMELLAE = 160.0f;
__constant__ float d_MIN_LAMELLAE = 1.0f;
__constant__ float d_MAX_LAMELLAE = 160.0f;

// Internode constants
__constant__ float d_INTERNODE_ALPHA = 150.0f;
__constant__ float d_INTERNODE_BETA = 0.9f;

//=============================================================================
// Device Utility Functions
//=============================================================================

__device__ __forceinline__ float device_clamp(float val, float min_val, float max_val) {
    return fminf(fmaxf(val, min_val), max_val);
}

__device__ __forceinline__ float device_safe_div(float num, float denom) {
    return num / (denom + 1e-9f);
}

//=============================================================================
// CUDA Kernel: Rushton G-Ratio Calculation
//=============================================================================

/**
 * @brief Compute optimal g-ratio for each axon based on diameter
 *
 * Formula: g_opt(d) = g_base + alpha * exp(-d / d_critical)
 *
 * Each thread processes one axon
 */
__global__ void kernel_rushton_g_ratio(
    const float* __restrict__ axon_diameters,  // [N_axons]
    float* __restrict__ g_ratios,              // [N_axons]
    uint32_t n_axons
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float d = axon_diameters[idx];

    // Rushton model with diameter dependence
    // Smaller axons have higher optimal g-ratio (thinner myelin)
    float g_opt = d_G_RATIO_BASE + d_G_RATIO_ALPHA * expf(-d / d_D_CRITICAL);

    // Clamp to physiological range
    g_ratios[idx] = device_clamp(g_opt, d_G_RATIO_MIN, d_G_RATIO_MAX);
}

/**
 * @brief Compute g-ratio efficiency factor
 *
 * Efficiency = 1 - k * (g - g_opt)^2
 * Peaks at optimal g-ratio
 */
__global__ void kernel_g_ratio_efficiency(
    const float* __restrict__ g_ratios,        // [N_axons]
    const float* __restrict__ optimal_g,       // [N_axons]
    float* __restrict__ efficiency,            // [N_axons]
    uint32_t n_axons
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float g = g_ratios[idx];
    float g_opt = optimal_g[idx];
    float deviation = g - g_opt;

    // Parabolic efficiency factor (k = 25 gives reasonable dropoff)
    float eff = 1.0f - 25.0f * deviation * deviation;
    efficiency[idx] = device_clamp(eff, 0.5f, 1.0f);
}

//=============================================================================
// CUDA Kernel: Cable Theory Computation
//=============================================================================

/**
 * @brief Compute cable theory parameters for all segments
 *
 * Formulas:
 *   r_m = r_m_base + r_m_per_lamella * n_lamellae
 *   c_m = c_m_base * (c_m_reduction ^ n_lamellae)
 *   lambda = sqrt(r_m * d / (4 * r_a))
 *   tau = r_m * c_m
 *
 * Each thread processes one segment [axon_idx, internode_idx]
 */
__global__ void kernel_cable_theory(
    const float* __restrict__ axon_diameters,    // [N_axons]
    const float* __restrict__ num_lamellae,      // [N_axons, N_internodes] as float
    float* __restrict__ space_constants,         // [N_axons, N_internodes] lambda
    float* __restrict__ time_constants,          // [N_axons, N_internodes] tau
    float* __restrict__ membrane_resistance,     // [N_axons, N_internodes] r_m
    float* __restrict__ axial_resistance,        // [N_axons, N_internodes] r_a
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    uint32_t axon_idx = idx / n_internodes;
    // uint32_t internode_idx = idx % n_internodes;  // Not needed for computation

    float d = axon_diameters[axon_idx];
    float n_lam = num_lamellae[idx];

    // Membrane resistance increases with lamellae (better insulation)
    float r_m = d_R_M_BASE + d_R_M_PER_LAMELLA * n_lam;

    // Membrane capacitance decreases with lamellae (layers in series)
    float c_m = d_C_M_BASE * powf(d_C_M_REDUCTION, n_lam);

    // Axial resistance depends on axon diameter
    // r_a = rho_a / (pi * (d/2)^2) where rho_a is axoplasmic resistivity
    // Convert: d in um, we want r_a in Ohm/cm
    float d_cm = d * 1e-4f;  // um to cm
    float r_a = d_R_A_CYTOPLASM / (NIMCP_PI_F * 0.25f * d_cm * d_cm);

    // Space constant: lambda = sqrt(r_m * d / (4 * r_a))
    // This gives lambda in cm, convert to um
    float lambda_cm = sqrtf(r_m * d_cm / (4.0f * r_a));
    float lambda_um = lambda_cm * 1e4f;

    // Time constant: tau = r_m * c_m (in ms, given units)
    float tau_ms = r_m * c_m * 1e-3f;  // Convert to ms

    // Store results
    space_constants[idx] = lambda_um;
    time_constants[idx] = tau_ms;
    membrane_resistance[idx] = r_m;
    axial_resistance[idx] = r_a;
}

/**
 * @brief Compute signal attenuation along segments
 *
 * attenuation = exp(-L / lambda)
 */
__global__ void kernel_attenuation(
    const float* __restrict__ internode_lengths,  // [N_axons, N_internodes]
    const float* __restrict__ space_constants,    // [N_axons, N_internodes]
    float* __restrict__ attenuation,              // [N_axons, N_internodes]
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    float L = internode_lengths[idx];
    float lambda = space_constants[idx];

    // Exponential decay
    attenuation[idx] = expf(-L / (lambda + 1e-6f));
}

//=============================================================================
// CUDA Kernel: Saltatory Conduction Velocity
//=============================================================================

/**
 * @brief Compute saltatory conduction velocity for each segment
 *
 * Formula:
 *   v = L_internode / (tau_node + tau_internode)
 *   tau_internode = L^2 / (lambda^2 * v_passive)
 *   v_final = v * g_efficiency * compaction * integrity
 *
 * Each thread processes one segment
 */
__global__ void kernel_saltatory_velocity(
    const float* __restrict__ axon_diameters,     // [N_axons]
    const float* __restrict__ internode_lengths,  // [N_axons, N_internodes]
    const float* __restrict__ space_constants,    // [N_axons, N_internodes]
    const float* __restrict__ g_ratios,           // [N_axons]
    const float* __restrict__ compaction_scores,  // [N_axons, N_internodes]
    const float* __restrict__ integrity,          // [N_axons, N_internodes]
    const float* __restrict__ num_lamellae,       // [N_axons, N_internodes]
    float* __restrict__ segment_velocities,       // [N_axons, N_internodes]
    float* __restrict__ propagation_delays,       // [N_axons, N_internodes]
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    uint32_t axon_idx = idx / n_internodes;

    float d = axon_diameters[axon_idx];
    float L = internode_lengths[idx];
    float lambda = space_constants[idx];
    float g = g_ratios[axon_idx];
    float compact = compaction_scores[idx];
    float integ = integrity[idx];
    float n_lam = num_lamellae[idx];

    // Skip if no myelin
    if (n_lam < d_MIN_LAMELLAE || L < 1.0f) {
        // Unmyelinated velocity: v = 1.0 * sqrt(d) m/s
        float v_unmyelin = sqrtf(d);
        segment_velocities[idx] = v_unmyelin;
        propagation_delays[idx] = (L * 1e-6f) / v_unmyelin * 1e3f;  // Convert to ms
        return;
    }

    // Calculate optimal g-ratio for this diameter
    float g_opt = d_G_RATIO_BASE + d_G_RATIO_ALPHA * expf(-d / d_D_CRITICAL);
    g_opt = device_clamp(g_opt, d_G_RATIO_MIN, d_G_RATIO_MAX);

    // G-ratio efficiency factor
    float g_deviation = g - g_opt;
    float g_efficiency = 1.0f - 25.0f * g_deviation * g_deviation;
    g_efficiency = device_clamp(g_efficiency, 0.5f, 1.0f);

    // Internode delay: tau_internode = L^2 / (lambda^2 * v_passive)
    float tau_internode_ms = (L * L) / (lambda * lambda * d_V_PASSIVE + 1e-6f);

    // Total segment delay = node delay + internode delay
    float total_delay_ms = d_TAU_NODE_MS + tau_internode_ms;

    // Base velocity from delay
    // v = L / total_delay, convert L from um to m and delay to s
    float v_base = device_safe_div(L * 1e-6f, total_delay_ms * 1e-3f);

    // Myelin fraction factor (more lamellae = better insulation up to optimal)
    float optimal_lamellae = 40.0f;  // Typical CNS
    float myelin_fraction = device_clamp(n_lam / optimal_lamellae, 0.0f, 1.0f);

    // Final velocity with all efficiency factors
    float v_final = v_base * g_efficiency * compact * integ * myelin_fraction;

    // Also consider Hursh's law contribution: v = k * d for myelinated
    float v_hursh = d_VELOCITY_COEFF * d * g_efficiency * compact * integ;

    // Blend the two models (weighted average)
    float velocity = 0.5f * v_final + 0.5f * v_hursh;

    // Clamp to physiological range
    velocity = device_clamp(velocity, d_V_MIN_MS, d_V_MAX_MS);

    // Compute final delay
    float delay_ms = device_safe_div(L * 1e-6f, velocity) * 1e3f;

    segment_velocities[idx] = velocity;
    propagation_delays[idx] = delay_ms;
}

/**
 * @brief Aggregate segment velocities to axon-level velocity
 *
 * Uses harmonic mean weighted by length (proper for velocities)
 * Each block processes one axon
 */
__global__ void kernel_aggregate_velocities(
    const float* __restrict__ segment_velocities,  // [N_axons, N_internodes]
    const float* __restrict__ internode_lengths,   // [N_axons, N_internodes]
    const float* __restrict__ propagation_delays,  // [N_axons, N_internodes]
    float* __restrict__ axon_velocities,           // [N_axons]
    float* __restrict__ total_delays,              // [N_axons] (optional)
    uint32_t n_axons,
    uint32_t n_internodes
) {
    extern __shared__ float shared_data[];
    float* s_total_length = shared_data;
    float* s_total_delay = &shared_data[blockDim.x];

    uint32_t axon_idx = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (axon_idx >= n_axons) return;

    // Initialize shared memory
    s_total_length[tid] = 0.0f;
    s_total_delay[tid] = 0.0f;

    // Each thread processes some segments
    for (uint32_t i = tid; i < n_internodes; i += blockDim.x) {
        uint32_t seg_idx = axon_idx * n_internodes + i;
        float L = internode_lengths[seg_idx];
        float delay = propagation_delays[seg_idx];

        if (L > 0.0f) {
            s_total_length[tid] += L;
            s_total_delay[tid] += delay;
        }
    }
    __syncthreads();

    // Reduction for total length and delay
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_total_length[tid] += s_total_length[tid + s];
            s_total_delay[tid] += s_total_delay[tid + s];
        }
        __syncthreads();
    }

    // Thread 0 computes final velocity
    if (tid == 0) {
        float total_L = s_total_length[0];
        float total_d = s_total_delay[0];

        // Effective velocity = total_length / total_delay
        float v_eff = device_safe_div(total_L * 1e-6f, total_d * 1e-3f);
        v_eff = device_clamp(v_eff, d_V_MIN_MS, d_V_MAX_MS);

        axon_velocities[axon_idx] = v_eff;
        if (total_delays != NULL) {
            total_delays[axon_idx] = total_d;
        }
    }
}

//=============================================================================
// CUDA Kernel: Myelin Plasticity
//=============================================================================

/**
 * @brief Apply activity-dependent myelination
 *
 * Formula:
 *   rate_myelin = k_max * (A^n / (K^n + A^n)) * (1 - N/N_max)
 *   rate_demyelin = k_demyelin * (K_d / (K_d + A))
 *   net_rate = rate_myelin - rate_demyelin
 *
 * Each thread processes one segment
 */
__global__ void kernel_myelin_plasticity(
    const float* __restrict__ activity,         // [N_axons]
    const float* __restrict__ current_lamellae, // [N_axons, N_internodes]
    float* __restrict__ lamellae_out,           // [N_axons, N_internodes]
    float k_max,
    float k_half,
    float hill_n,
    float dt,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    uint32_t axon_idx = idx / n_internodes;

    float A = activity[axon_idx];
    float current_N = current_lamellae[idx];

    // Hill function for myelination rate
    float A_n = powf(A, hill_n);
    float K_n = powf(k_half, hill_n);
    float hill_term = A_n / (K_n + A_n + 1e-9f);

    // Saturation term (slows as approaching max)
    float saturation = 1.0f - (current_N / d_SATURATION_LAMELLAE);
    saturation = device_clamp(saturation, 0.0f, 1.0f);

    // Myelination rate
    float rate_myelin = k_max * hill_term * saturation;

    // Demyelination rate (inverse relationship with activity)
    float rate_demyelin = d_K_DEMYELIN * (d_K_DEMYELIN_HALF / (d_K_DEMYELIN_HALF + A + 1e-9f));

    // Net rate
    float net_rate = rate_myelin - rate_demyelin;

    // Update lamellae (fractional for accumulation)
    float new_N = current_N + net_rate * dt;

    // Clamp to valid range
    lamellae_out[idx] = device_clamp(new_N, d_MIN_LAMELLAE, d_MAX_LAMELLAE);
}

/**
 * @brief Update activity exponential moving average
 */
__global__ void kernel_update_activity_ema(
    const float* __restrict__ activity,    // [N_axons]
    float* __restrict__ activity_ema,      // [N_axons]
    float alpha,                           // EMA weight
    uint32_t n_axons
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float current_ema = activity_ema[idx];
    float new_activity = activity[idx];

    // EMA update: ema = alpha * new + (1 - alpha) * old
    activity_ema[idx] = alpha * new_activity + (1.0f - alpha) * current_ema;
}

/**
 * @brief Commit fractional lamellae to integer values
 */
__global__ void kernel_commit_lamellae(
    const float* __restrict__ lamellae_fractional,  // [N_axons, N_internodes]
    float* __restrict__ num_lamellae,               // [N_axons, N_internodes] as float
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    float frac = lamellae_fractional[idx];
    float rounded = roundf(frac);
    num_lamellae[idx] = device_clamp(rounded, d_MIN_LAMELLAE, d_MAX_LAMELLAE);
}

//=============================================================================
// CUDA Kernel: Conduction Block
//=============================================================================

/**
 * @brief Compute conduction block probability
 *
 * Formula:
 *   P_base = 1 / (1 + exp((I - I_crit) / sigma))
 *   T_factor = 1 + k_T * max(0, T - T_ref)
 *   P_block = P_base * T_factor
 */
__global__ void kernel_block_probability(
    const float* __restrict__ integrity,       // [N_axons, N_internodes]
    float* __restrict__ block_probability,     // [N_axons, N_internodes]
    float temperature_c,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    float I = integrity[idx];

    // Sigmoid probability (high when integrity is low)
    float z = (I - d_I_CRITICAL) / d_BLOCK_SIGMA;
    float P_base = 1.0f / (1.0f + expf(z));

    // Temperature factor (Uhthoff phenomenon)
    float T_delta = fmaxf(0.0f, temperature_c - d_T_REF);
    float T_factor = 1.0f + d_T_SENSITIVITY * T_delta;

    // Final block probability
    float P_block = P_base * T_factor;
    block_probability[idx] = device_clamp(P_block, 0.0f, 1.0f);
}

/**
 * @brief Apply stochastic conduction blocks
 *
 * Uses LCG random number generator for reproducibility
 */
__global__ void kernel_apply_blocks(
    const float* __restrict__ block_probability,  // [N_axons, N_internodes]
    float* __restrict__ is_conducting,            // [N_axons, N_internodes]
    uint64_t seed,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    // LCG random number generator
    uint64_t state = seed ^ (idx * 6364136223846793005ULL + 1442695040888963407ULL);
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    float rand_val = (float)(state >> 33) / (float)(1ULL << 31);

    float P_block = block_probability[idx];

    // Conducting if random value exceeds block probability
    is_conducting[idx] = (rand_val > P_block) ? 1.0f : 0.0f;
}

//=============================================================================
// CUDA Kernel: Damage and Repair
//=============================================================================

/**
 * @brief Apply damage to integrity
 */
__global__ void kernel_apply_damage(
    const float* __restrict__ damage,     // [N_axons, N_internodes] or [N_axons]
    float* __restrict__ integrity,        // [N_axons, N_internodes]
    bool broadcast_axon,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    uint32_t damage_idx = broadcast_axon ? (idx / n_internodes) : idx;
    float dmg = damage[damage_idx];
    float I = integrity[idx];

    integrity[idx] = device_clamp(I - dmg, 0.0f, 1.0f);
}

/**
 * @brief Apply repair to integrity
 */
__global__ void kernel_apply_repair(
    float* __restrict__ integrity,        // [N_axons, N_internodes]
    float repair_amount,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    float I = integrity[idx];

    // Only repair damaged segments
    if (I < 1.0f) {
        integrity[idx] = device_clamp(I + repair_amount, 0.0f, 1.0f);
    }
}

/**
 * @brief Apply natural decay to integrity
 */
__global__ void kernel_apply_decay(
    float* __restrict__ integrity,        // [N_axons, N_internodes]
    float decay_amount,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total = n_axons * n_internodes;
    if (idx >= total) return;

    float I = integrity[idx];
    integrity[idx] = device_clamp(I - decay_amount, 0.0f, 1.0f);
}

//=============================================================================
// Enhanced Myelin GPU Kernels for Saltatory Conduction Simulation
//=============================================================================

/**
 * @brief Myelin segment GPU context for detailed saltatory conduction
 *
 * This structure provides a specialized context for per-segment myelin
 * simulation with explicit node of Ranvier modeling.
 */
typedef struct nimcp_myelin_gpu_ctx {
    void* gpu_context;

    // Per-segment data
    float* d_thickness;         // Myelin thickness per segment
    float* d_conductance;       // Axial conductance per segment
    float* d_capacitance;       // Membrane capacitance per segment
    int* d_segment_axon_map;    // Which axon each segment belongs to

    // Node of Ranvier data
    float* d_node_voltage;      // Voltage at each node
    float* d_node_conductance;  // Node ion channel conductance

    size_t num_segments;
    size_t num_nodes;
    size_t num_axons;
} nimcp_myelin_gpu_ctx_t;

// Forward declaration
extern "C" void nimcp_myelin_gpu_destroy(nimcp_myelin_gpu_ctx_t* ctx);

/**
 * @brief Create enhanced myelin GPU context for saltatory conduction
 */
extern "C" nimcp_myelin_gpu_ctx_t* nimcp_myelin_gpu_create(
    void* gpu_ctx,
    size_t num_segments,
    size_t num_nodes,
    size_t num_axons)
{
    if (!gpu_ctx || num_segments == 0 || num_nodes == 0 || num_axons == 0) {
        LOG_ERROR("Invalid parameters for myelin GPU context creation");
        return NULL;
    }

    nimcp_myelin_gpu_ctx_t* ctx = (nimcp_myelin_gpu_ctx_t*)nimcp_calloc(1, sizeof(nimcp_myelin_gpu_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate myelin GPU context");
        return NULL;
    }

    ctx->gpu_context = gpu_ctx;
    ctx->num_segments = num_segments;
    ctx->num_nodes = num_nodes;
    ctx->num_axons = num_axons;

    cudaError_t err;

    // Allocate per-segment data
    err = cudaMalloc(&ctx->d_thickness, num_segments * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_conductance, num_segments * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_capacitance, num_segments * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_segment_axon_map, num_segments * sizeof(int));
    if (err != cudaSuccess) goto cleanup;

    // Allocate node data
    err = cudaMalloc(&ctx->d_node_voltage, num_nodes * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    err = cudaMalloc(&ctx->d_node_conductance, num_nodes * sizeof(float));
    if (err != cudaSuccess) goto cleanup;

    // Initialize to defaults
    cudaMemset(ctx->d_thickness, 0, num_segments * sizeof(float));
    cudaMemset(ctx->d_conductance, 0, num_segments * sizeof(float));
    cudaMemset(ctx->d_capacitance, 0, num_segments * sizeof(float));
    cudaMemset(ctx->d_node_voltage, 0, num_nodes * sizeof(float));
    cudaMemset(ctx->d_node_conductance, 0, num_nodes * sizeof(float));

    LOG_DEBUG("Created myelin GPU context: segments=%zu, nodes=%zu, axons=%zu",
              num_segments, num_nodes, num_axons);
    return ctx;

cleanup:
    LOG_ERROR("Failed to allocate myelin GPU buffers: %s", cudaGetErrorString(err));
    nimcp_myelin_gpu_destroy(ctx);
    return NULL;
}

/**
 * @brief Destroy enhanced myelin GPU context
 */
extern "C" void nimcp_myelin_gpu_destroy(nimcp_myelin_gpu_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->d_thickness) cudaFree(ctx->d_thickness);
    if (ctx->d_conductance) cudaFree(ctx->d_conductance);
    if (ctx->d_capacitance) cudaFree(ctx->d_capacitance);
    if (ctx->d_segment_axon_map) cudaFree(ctx->d_segment_axon_map);
    if (ctx->d_node_voltage) cudaFree(ctx->d_node_voltage);
    if (ctx->d_node_conductance) cudaFree(ctx->d_node_conductance);

    nimcp_free(ctx);
    LOG_DEBUG("Destroyed myelin GPU context");
}

/**
 * @brief Update conductance based on myelin thickness
 *
 * Thicker myelin = higher axial conductance (better insulation)
 * Formula: conductance = base_conductance * (1 + k * thickness^2)
 */
__global__ void kernel_myelin_conductance_update(
    float* conductance,
    const float* thickness,
    size_t num_segments,
    float base_conductance)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_segments) return;

    float t = thickness[idx];

    // Conductance increases with thickness squared (cable theory approximation)
    // G = G_base * (1 + k * t^2) where k accounts for myelin layers
    float k = 0.1f;  // Scaling factor
    conductance[idx] = base_conductance * (1.0f + k * t * t);
}

/**
 * @brief Saltatory propagation simulation
 *
 * Simulates action potential jumping between nodes of Ranvier.
 * Each node receives current from adjacent myelinated segments.
 *
 * @param node_voltage Current voltage at each node
 * @param segment_conductance Conductance of myelinated segments
 * @param segment_to_node_map Mapping from segments to their terminal nodes
 * @param num_nodes Number of nodes of Ranvier
 * @param num_segments Number of myelinated segments
 * @param dt Time step in ms
 */
__global__ void kernel_saltatory_propagation(
    float* node_voltage,
    const float* segment_conductance,
    const int* segment_to_node_map,
    size_t num_nodes,
    size_t num_segments,
    float dt)
{
    size_t node_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (node_idx >= num_nodes) return;

    // Node parameters
    float V_rest = -70.0f;      // Resting potential (mV)
    float V_thresh = -55.0f;    // Threshold potential (mV)
    float V_peak = 40.0f;       // Peak AP voltage (mV)
    float g_leak = 0.1f;        // Leak conductance (mS/cm^2)
    float C_m = 1.0f;           // Membrane capacitance (uF/cm^2)

    float V = node_voltage[node_idx];

    // Calculate incoming current from adjacent segments
    // In a real implementation, we'd track which segments connect to this node
    float I_in = 0.0f;

    // Check segments that terminate at this node
    for (size_t seg = 0; seg < num_segments; seg++) {
        if (segment_to_node_map[seg] == (int)node_idx) {
            float G = segment_conductance[seg];
            // Current from previous node through this segment
            // Simplified: assume fixed driving voltage for active segments
            I_in += G * (V_peak - V);
        }
    }

    // Leak current
    float I_leak = g_leak * (V - V_rest);

    // Membrane potential update (forward Euler)
    float dV = (I_in - I_leak) / C_m * dt;
    V += dV;

    // Simple threshold crossing for AP generation
    if (V >= V_thresh && V < V_peak) {
        V = V_peak;  // Generate AP
    } else if (V >= V_peak) {
        V = V_rest;  // Reset after AP
    }

    node_voltage[node_idx] = V;
}

/**
 * @brief Activity-dependent myelin plasticity update
 *
 * Adjusts myelin thickness based on axon activity levels.
 * Higher activity leads to increased myelination.
 *
 * @param thickness Current myelin thickness per segment
 * @param activity Activity level per axon (firing rate)
 * @param segment_axon_map Mapping from segments to their parent axons
 * @param num_segments Number of myelinated segments
 * @param learning_rate Rate of thickness adjustment
 * @param min_thickness Minimum allowable thickness
 * @param max_thickness Maximum allowable thickness
 */
__global__ void kernel_myelin_plasticity(
    float* thickness,
    const float* activity,
    const int* segment_axon_map,
    size_t num_segments,
    float learning_rate,
    float min_thickness,
    float max_thickness)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_segments) return;

    int axon_idx = segment_axon_map[idx];
    float A = activity[axon_idx];
    float t = thickness[idx];

    // Hill-like response: high activity promotes myelination
    float K_half = 10.0f;  // Half-max activity
    float n = 2.0f;        // Hill coefficient
    float A_n = powf(A, n);
    float K_n = powf(K_half, n);
    float myelination_drive = A_n / (K_n + A_n + 1e-9f);

    // Compute target thickness based on activity
    float target = min_thickness + (max_thickness - min_thickness) * myelination_drive;

    // Gradual adjustment toward target
    float delta = learning_rate * (target - t);
    t = t + delta;

    // Clamp to valid range
    thickness[idx] = device_clamp(t, min_thickness, max_thickness);
}

/**
 * @brief Compute myelination signal from firing rates
 *
 * Determines which axons should be myelinated based on activity threshold.
 *
 * @param firing_rates Average firing rate per axon
 * @param myelination_signal Output signal strength for myelination
 * @param num_axons Number of axons
 * @param threshold Minimum firing rate to trigger myelination
 */
__global__ void kernel_compute_myelination_signal(
    const float* firing_rates,
    float* myelination_signal,
    size_t num_axons,
    float threshold)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_axons) return;

    float rate = firing_rates[idx];

    // Sigmoid activation above threshold
    // sigma(x) = 1 / (1 + exp(-(x - threshold) / k))
    float k = 5.0f;  // Steepness
    float z = (rate - threshold) / k;
    float signal = 1.0f / (1.0f + expf(-z));

    // Scale signal by how far above threshold
    if (rate > threshold) {
        signal *= (rate - threshold) / (rate + 1e-6f);
    }

    myelination_signal[idx] = signal;
}

/**
 * @brief Initialize myelin GPU context from network data
 */
extern "C" int nimcp_myelin_gpu_init_from_network(
    nimcp_myelin_gpu_ctx_t* ctx,
    const void* myelin_network)
{
    if (!ctx || !myelin_network) return -1;

    // This would extract data from a CPU myelin network structure
    // For now, initialize with default values
    float default_thickness = 1.0f;  // 1 um
    float default_capacitance = 1.0f;  // 1 uF/cm^2

    // Initialize thickness and capacitance
    std::vector<float> thickness(ctx->num_segments, default_thickness);
    std::vector<float> capacitance(ctx->num_segments, default_capacitance);

    cudaMemcpy(ctx->d_thickness, thickness.data(),
               ctx->num_segments * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(ctx->d_capacitance, capacitance.data(),
               ctx->num_segments * sizeof(float), cudaMemcpyHostToDevice);

    // Initialize segment-to-axon mapping (simple linear assignment)
    std::vector<int> seg_axon_map(ctx->num_segments);
    size_t segs_per_axon = ctx->num_segments / ctx->num_axons;
    for (size_t i = 0; i < ctx->num_segments; i++) {
        seg_axon_map[i] = (int)(i / segs_per_axon);
    }
    cudaMemcpy(ctx->d_segment_axon_map, seg_axon_map.data(),
               ctx->num_segments * sizeof(int), cudaMemcpyHostToDevice);

    // Initialize conductance based on thickness
    kernel_myelin_conductance_update<<<GRID_SIZE(ctx->num_segments), BLOCK_SIZE>>>(
        ctx->d_conductance, ctx->d_thickness, ctx->num_segments, 1.0f);

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

/**
 * @brief Propagate signals through myelin network
 */
extern "C" int nimcp_myelin_gpu_propagate(nimcp_myelin_gpu_ctx_t* ctx, float dt)
{
    if (!ctx) return -1;

    // Update conductances based on current thickness
    kernel_myelin_conductance_update<<<GRID_SIZE(ctx->num_segments), BLOCK_SIZE>>>(
        ctx->d_conductance, ctx->d_thickness, ctx->num_segments, 1.0f);

    // Create segment-to-node mapping for propagation
    // Simple mapping: segment i connects to node i and i+1
    int* d_seg_to_node;
    cudaMalloc(&d_seg_to_node, ctx->num_segments * sizeof(int));

    std::vector<int> seg_to_node(ctx->num_segments);
    for (size_t i = 0; i < ctx->num_segments; i++) {
        seg_to_node[i] = (int)(i % ctx->num_nodes);
    }
    cudaMemcpy(d_seg_to_node, seg_to_node.data(),
               ctx->num_segments * sizeof(int), cudaMemcpyHostToDevice);

    // Perform saltatory propagation
    kernel_saltatory_propagation<<<GRID_SIZE(ctx->num_nodes), BLOCK_SIZE>>>(
        ctx->d_node_voltage,
        ctx->d_conductance,
        d_seg_to_node,
        ctx->num_nodes,
        ctx->num_segments,
        dt);

    cudaFree(d_seg_to_node);

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

/**
 * @brief Update myelin thickness based on activity
 */
extern "C" int nimcp_myelin_gpu_update_thickness(
    nimcp_myelin_gpu_ctx_t* ctx,
    const float* activity_levels)
{
    if (!ctx || !activity_levels) return -1;

    float learning_rate = 0.01f;
    float min_thickness = 0.1f;
    float max_thickness = 5.0f;

    // Copy activity to device
    float* d_activity;
    cudaMalloc(&d_activity, ctx->num_axons * sizeof(float));
    cudaMemcpy(d_activity, activity_levels,
               ctx->num_axons * sizeof(float), cudaMemcpyHostToDevice);

    // Apply plasticity
    kernel_myelin_plasticity<<<GRID_SIZE(ctx->num_segments), BLOCK_SIZE>>>(
        ctx->d_thickness,
        d_activity,
        ctx->d_segment_axon_map,
        ctx->num_segments,
        learning_rate,
        min_thickness,
        max_thickness);

    cudaFree(d_activity);

    cudaError_t err = cudaGetLastError();
    return (err == cudaSuccess) ? 0 : -1;
}

//=============================================================================
// CUDA Kernel: Statistics
//=============================================================================

/**
 * @brief Compute network mean values via parallel reduction
 */
static __global__ void kernel_compute_mean(
    const float* __restrict__ values,    // [N]
    float* __restrict__ mean_out,        // [1]
    uint32_t n
) {
    extern __shared__ float s_data[];

    uint32_t tid = threadIdx.x;
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    // Load data to shared memory
    s_data[tid] = (i < n) ? values[i] : 0.0f;
    __syncthreads();

    // Reduction in shared memory
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_data[tid] += s_data[tid + s];
        }
        __syncthreads();
    }

    // Block result
    if (tid == 0) {
        atomicAdd(mean_out, s_data[0] / (float)n);
    }
}

//=============================================================================
// API Implementation: Configuration
//=============================================================================

extern "C" myelin_gpu_config_t myelin_gpu_default_config(void) {
    myelin_gpu_config_t config;
    config.max_axons = MYELIN_GPU_DEFAULT_MAX_AXONS;
    config.max_internodes = MYELIN_GPU_DEFAULT_MAX_INTERNODES;
    config.max_nodes = MYELIN_GPU_DEFAULT_MAX_INTERNODES - 1;
    config.target_g_ratio = 0.77f;
    config.temperature_c = 37.0f;
    config.myelination_rate_max = 2.0f;
    config.demyelination_rate = 0.1f;
    config.hill_coefficient = 2.5f;
    config.enable_stochastic = false;
    config.enable_block_modeling = true;
    config.enable_async_transfer = true;
    return config;
}

//=============================================================================
// API Implementation: Lifecycle
//=============================================================================

extern "C" myelin_gpu_context_t* myelin_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const myelin_gpu_config_t* config
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    myelin_gpu_context_t* ctx = (myelin_gpu_context_t*)nimcp_calloc(1, sizeof(myelin_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate myelin GPU context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : myelin_gpu_default_config();
    ctx->n_axons = 0;
    ctx->n_internodes = ctx->config.max_internodes;
    ctx->n_nodes = ctx->config.max_internodes - 1;

    // Create CUDA stream
    cudaError_t err = cudaStreamCreate(&ctx->stream);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create CUDA stream: %s", cudaGetErrorString(err));
        nimcp_free(ctx);
        return NULL;
    }

    // Pre-calculate dimensions
    size_t n_ax = ctx->config.max_axons;
    size_t n_int = ctx->config.max_internodes;
    size_t n_nod = ctx->config.max_nodes;

    // Allocate 1D tensors [N_axons]
    size_t dims_1d[] = {n_ax};
    ctx->g_ratios = nimcp_gpu_tensor_create(gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->conduction_velocities = nimcp_gpu_tensor_create(gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->axon_diameters = nimcp_gpu_tensor_create(gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->outer_diameters = nimcp_gpu_tensor_create(gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->activity_ema = nimcp_gpu_tensor_create(gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->temp_buffer_1d = nimcp_gpu_tensor_create(gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);

    // Allocate 2D tensors [N_axons, N_internodes]
    size_t dims_2d[] = {n_ax, n_int};
    ctx->sheath_thickness = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->internode_lengths = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->capacitance = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->num_lamellae = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->compaction_scores = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->space_constants = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->time_constants = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->membrane_resistance = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->axial_resistance = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->segment_velocities = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->propagation_delays = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->block_probabilities = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->is_conducting = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->integrity = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->damage_accumulated = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->lamellae_fractional = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->temp_buffer_2d = nimcp_gpu_tensor_create(gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);

    // Allocate node tensor [N_axons, N_nodes]
    size_t dims_nodes[] = {n_ax, n_nod};
    ctx->node_widths = nimcp_gpu_tensor_create(gpu_ctx, dims_nodes, 2, NIMCP_GPU_PRECISION_FP32);

    // Allocate scalar statistics tensors [1]
    size_t dims_scalar[] = {1};
    ctx->mean_g_ratio = nimcp_gpu_tensor_create(gpu_ctx, dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->mean_velocity = nimcp_gpu_tensor_create(gpu_ctx, dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->mean_integrity = nimcp_gpu_tensor_create(gpu_ctx, dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);

    // Check all allocations
    if (!ctx->g_ratios || !ctx->conduction_velocities || !ctx->axon_diameters ||
        !ctx->sheath_thickness || !ctx->internode_lengths || !ctx->num_lamellae ||
        !ctx->space_constants || !ctx->time_constants || !ctx->integrity ||
        !ctx->segment_velocities || !ctx->propagation_delays) {
        LOG_ERROR("Failed to allocate one or more GPU tensors");
        myelin_gpu_destroy(ctx);
        return NULL;
    }

    // Initialize tensors to default values
    nimcp_gpu_ones(gpu_ctx, ctx->integrity);
    nimcp_gpu_ones(gpu_ctx, ctx->compaction_scores);
    nimcp_gpu_ones(gpu_ctx, ctx->is_conducting);
    nimcp_gpu_zeros(gpu_ctx, ctx->damage_accumulated);
    nimcp_gpu_zeros(gpu_ctx, ctx->activity_ema);
    nimcp_gpu_zeros(gpu_ctx, ctx->block_probabilities);

    LOG_INFO("GPU Myelin context created (max_axons: %u, max_internodes: %u)",
             ctx->config.max_axons, ctx->config.max_internodes);

    return ctx;
}

extern "C" void myelin_gpu_destroy(myelin_gpu_context_t* ctx) {
    if (!ctx) return;

    if (ctx->stream) {
        cudaStreamSynchronize(ctx->stream);
        cudaStreamDestroy(ctx->stream);
    }

    // Destroy all tensors
    nimcp_gpu_tensor_destroy(ctx->g_ratios);
    nimcp_gpu_tensor_destroy(ctx->sheath_thickness);
    nimcp_gpu_tensor_destroy(ctx->internode_lengths);
    nimcp_gpu_tensor_destroy(ctx->node_widths);
    nimcp_gpu_tensor_destroy(ctx->conduction_velocities);
    nimcp_gpu_tensor_destroy(ctx->capacitance);
    nimcp_gpu_tensor_destroy(ctx->axon_diameters);
    nimcp_gpu_tensor_destroy(ctx->outer_diameters);
    nimcp_gpu_tensor_destroy(ctx->num_lamellae);
    nimcp_gpu_tensor_destroy(ctx->compaction_scores);
    nimcp_gpu_tensor_destroy(ctx->space_constants);
    nimcp_gpu_tensor_destroy(ctx->time_constants);
    nimcp_gpu_tensor_destroy(ctx->membrane_resistance);
    nimcp_gpu_tensor_destroy(ctx->axial_resistance);
    nimcp_gpu_tensor_destroy(ctx->segment_velocities);
    nimcp_gpu_tensor_destroy(ctx->propagation_delays);
    nimcp_gpu_tensor_destroy(ctx->block_probabilities);
    nimcp_gpu_tensor_destroy(ctx->is_conducting);
    nimcp_gpu_tensor_destroy(ctx->integrity);
    nimcp_gpu_tensor_destroy(ctx->damage_accumulated);
    nimcp_gpu_tensor_destroy(ctx->activity_ema);
    nimcp_gpu_tensor_destroy(ctx->lamellae_fractional);
    nimcp_gpu_tensor_destroy(ctx->temp_buffer_1d);
    nimcp_gpu_tensor_destroy(ctx->temp_buffer_2d);
    nimcp_gpu_tensor_destroy(ctx->mean_g_ratio);
    nimcp_gpu_tensor_destroy(ctx->mean_velocity);
    nimcp_gpu_tensor_destroy(ctx->mean_integrity);

    nimcp_free(ctx);
    LOG_DEBUG("GPU Myelin context destroyed");
}

extern "C" bool myelin_gpu_synchronize(myelin_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_reset(myelin_gpu_context_t* ctx) {
    if (!ctx) return false;

    nimcp_gpu_ones(ctx->gpu_ctx, ctx->integrity);
    nimcp_gpu_ones(ctx->gpu_ctx, ctx->compaction_scores);
    nimcp_gpu_ones(ctx->gpu_ctx, ctx->is_conducting);
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->damage_accumulated);
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->activity_ema);
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->block_probabilities);

    ctx->kernel_launches = 0;
    ctx->g_ratio_computations = 0;
    ctx->velocity_computations = 0;
    ctx->plasticity_updates = 0;
    ctx->total_kernel_time_ms = 0.0f;

    return true;
}

//=============================================================================
// API Implementation: CPU Network Integration
//=============================================================================

extern "C" bool myelin_gpu_init_from_cpu(
    myelin_gpu_context_t* ctx,
    const myelin_sheath_network_t* cpu_network
) {
    if (!ctx || !cpu_network) return false;

    // TODO: Implement when myelin_sheath_network_t is fully defined
    // This would extract data from the CPU network structure and upload to GPU tensors
    LOG_WARN("myelin_gpu_init_from_cpu not yet fully implemented");
    return false;
}

extern "C" bool myelin_gpu_download_to_cpu(
    const myelin_gpu_context_t* ctx,
    myelin_sheath_network_t* cpu_network
) {
    if (!ctx || !cpu_network) return false;

    // TODO: Implement when myelin_sheath_network_t is fully defined
    // This would download GPU tensors back to the CPU network structure
    LOG_WARN("myelin_gpu_download_to_cpu not yet fully implemented");
    return false;
}

//=============================================================================
// API Implementation: Data Upload
//=============================================================================

extern "C" bool myelin_gpu_upload_axon_properties(
    myelin_gpu_context_t* ctx,
    const float* axon_diameters,
    const float* internode_lengths,
    uint32_t n_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !axon_diameters || !internode_lengths) return false;
    if (n_axons > ctx->config.max_axons) {
        LOG_ERROR("n_axons (%u) exceeds max_axons (%u)", n_axons, ctx->config.max_axons);
        return false;
    }

    ctx->n_axons = n_axons;

    // Upload axon diameters
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->axon_diameters->data, axon_diameters,
                               n_axons * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Upload internode lengths
    size_t internode_bytes = n_axons * ctx->n_internodes * sizeof(float);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->internode_lengths->data, internode_lengths,
                               internode_bytes,
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_upload_lamellae(
    myelin_gpu_context_t* ctx,
    const uint32_t* lamellae
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !lamellae) return false;

    // Convert uint32_t to float for GPU processing
    size_t total = ctx->n_axons * ctx->n_internodes;
    float* h_lamellae_f = (float*)nimcp_malloc(total * sizeof(float));
    if (!h_lamellae_f) return false;

    for (size_t i = 0; i < total; i++) {
        h_lamellae_f[i] = (float)lamellae[i];
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->num_lamellae->data, h_lamellae_f,
                               total * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    // Also copy to fractional for plasticity tracking
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->lamellae_fractional->data, h_lamellae_f,
                               total * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    nimcp_free(h_lamellae_f);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_upload_integrity(
    myelin_gpu_context_t* ctx,
    const float* integrity
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !integrity) return false;

    size_t total = ctx->n_axons * ctx->n_internodes;
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->integrity->data, integrity,
                               total * sizeof(float),
                               cudaMemcpyHostToDevice, ctx->stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_download_velocities(
    const myelin_gpu_context_t* ctx,
    float* velocities
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !velocities) return false;

    NIMCP_CUDA_RECOVER(cudaMemcpy(velocities, ctx->conduction_velocities->data,
                          ctx->n_axons * sizeof(float),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Core Kernels
//=============================================================================

extern "C" bool myelin_gpu_compute_g_ratios(myelin_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    kernel_rushton_g_ratio<<<GRID_SIZE(ctx->n_axons), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->axon_diameters->data,
        (float*)ctx->g_ratios->data,
        ctx->n_axons
    );

    ctx->kernel_launches++;
    ctx->g_ratio_computations++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_cable_params(myelin_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    kernel_cable_theory<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->axon_diameters->data,
        (float*)ctx->num_lamellae->data,
        (float*)ctx->space_constants->data,
        (float*)ctx->time_constants->data,
        (float*)ctx->membrane_resistance->data,
        (float*)ctx->axial_resistance->data,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_velocities(myelin_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    // First compute per-segment velocities
    kernel_saltatory_velocity<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->axon_diameters->data,
        (float*)ctx->internode_lengths->data,
        (float*)ctx->space_constants->data,
        (float*)ctx->g_ratios->data,
        (float*)ctx->compaction_scores->data,
        (float*)ctx->integrity->data,
        (float*)ctx->num_lamellae->data,
        (float*)ctx->segment_velocities->data,
        (float*)ctx->propagation_delays->data,
        ctx->n_axons,
        ctx->n_internodes
    );

    // Then aggregate to axon-level velocities
    size_t shared_mem = 2 * BLOCK_SIZE * sizeof(float);
    kernel_aggregate_velocities<<<ctx->n_axons, BLOCK_SIZE, shared_mem, ctx->stream>>>(
        (float*)ctx->segment_velocities->data,
        (float*)ctx->internode_lengths->data,
        (float*)ctx->propagation_delays->data,
        (float*)ctx->conduction_velocities->data,
        (float*)ctx->temp_buffer_1d->data,  // total delays
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches += 2;
    ctx->velocity_computations++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_apply_plasticity(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !activity || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    kernel_myelin_plasticity<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)activity->data,
        (float*)ctx->lamellae_fractional->data,
        (float*)ctx->lamellae_fractional->data,  // In-place update
        ctx->config.myelination_rate_max,
        0.3f,  // k_half
        ctx->config.hill_coefficient,
        dt,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    ctx->plasticity_updates++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_commit_lamellae(myelin_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    kernel_commit_lamellae<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->lamellae_fractional->data,
        (float*)ctx->num_lamellae->data,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_block_probabilities(myelin_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    kernel_block_probability<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->integrity->data,
        (float*)ctx->block_probabilities->data,
        ctx->config.temperature_c,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_apply_blocks(myelin_gpu_context_t* ctx, uint64_t seed) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    kernel_apply_blocks<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->block_probabilities->data,
        (float*)ctx->is_conducting->data,
        seed,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_set_temperature(myelin_gpu_context_t* ctx, float temperature_c) {
    if (!ctx) return false;
    ctx->config.temperature_c = temperature_c;
    return true;
}

extern "C" bool myelin_gpu_compute_g_efficiency(
    myelin_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* efficiency_out
) {
    if (!ctx || ctx->n_axons == 0) return false;

    // Use output tensor or temp buffer
    nimcp_gpu_tensor_t* output = efficiency_out ? efficiency_out : ctx->temp_buffer_1d;

    kernel_g_ratio_efficiency<<<GRID_SIZE(ctx->n_axons), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->g_ratios->data,
        (float*)ctx->g_ratios->data,  // Use same as optimal (computed from diameter)
        (float*)output->data,
        ctx->n_axons
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_attenuation(
    myelin_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* attenuation_out
) {
    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;
    nimcp_gpu_tensor_t* output = attenuation_out ? attenuation_out : ctx->temp_buffer_2d;

    kernel_attenuation<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->internode_lengths->data,
        (float*)ctx->space_constants->data,
        (float*)output->data,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_segment_velocities(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    // Compute per-segment velocities (without aggregation)
    kernel_saltatory_velocity<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->axon_diameters->data,
        (float*)ctx->internode_lengths->data,
        (float*)ctx->space_constants->data,
        (float*)ctx->g_ratios->data,
        (float*)ctx->compaction_scores->data,
        (float*)ctx->integrity->data,
        (float*)ctx->num_lamellae->data,
        (float*)ctx->segment_velocities->data,
        (float*)ctx->propagation_delays->data,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_delays(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return false;

    // Delays are computed as part of segment velocities
    // This just ensures they are current
    return myelin_gpu_compute_segment_velocities(ctx);
}

extern "C" bool myelin_gpu_compute_total_delays(
    myelin_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* total_delays_out
) {
    if (!ctx || ctx->n_axons == 0) return false;

    nimcp_gpu_tensor_t* output = total_delays_out ? total_delays_out : ctx->temp_buffer_1d;

    // Use aggregation kernel (second output is total delays)
    size_t shared_mem = 2 * BLOCK_SIZE * sizeof(float);
    kernel_aggregate_velocities<<<ctx->n_axons, BLOCK_SIZE, shared_mem, ctx->stream>>>(
        (float*)ctx->segment_velocities->data,
        (float*)ctx->internode_lengths->data,
        (float*)ctx->propagation_delays->data,
        (float*)ctx->conduction_velocities->data,  // Also updates velocities
        (float*)output->data,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_update_activity_ema(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    float dt
) {
    if (!ctx || !activity || ctx->n_axons == 0) return false;

    // EMA alpha based on time constant (tau = 1.0 second)
    float tau = 1.0f;
    float alpha = 1.0f - expf(-dt / tau);

    kernel_update_activity_ema<<<GRID_SIZE(ctx->n_axons), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)activity->data,
        (float*)ctx->activity_ema->data,
        alpha,
        ctx->n_axons
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_compute_statistics(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return false;

    // Compute all mean values
    myelin_gpu_get_mean_g_ratio(ctx);
    myelin_gpu_get_mean_velocity(ctx);
    myelin_gpu_get_mean_integrity(ctx);

    return true;
}

//=============================================================================
// API Implementation: Damage and Repair
//=============================================================================

extern "C" bool myelin_gpu_apply_damage(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* damage,
    bool broadcast_axon
) {
    if (!ctx || !damage || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;

    kernel_apply_damage<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)damage->data,
        (float*)ctx->integrity->data,
        broadcast_axon,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_apply_repair(myelin_gpu_context_t* ctx, float repair_rate, float dt) {
    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;
    float repair_amount = repair_rate * dt;

    kernel_apply_repair<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->integrity->data,
        repair_amount,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool myelin_gpu_apply_decay(myelin_gpu_context_t* ctx, float decay_rate, float dt) {
    if (!ctx || ctx->n_axons == 0) return false;

    uint32_t total = ctx->n_axons * ctx->n_internodes;
    float decay_amount = decay_rate * dt;

    kernel_apply_decay<<<GRID_SIZE(total), BLOCK_SIZE, 0, ctx->stream>>>(
        (float*)ctx->integrity->data,
        decay_amount,
        ctx->n_axons,
        ctx->n_internodes
    );

    ctx->kernel_launches++;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Batch Operations
//=============================================================================

extern "C" bool myelin_gpu_step(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->n_axons == 0) return false;

    // 1. Compute cable parameters
    if (!myelin_gpu_compute_cable_params(ctx)) return false;

    // 2. Compute velocities
    if (!myelin_gpu_compute_velocities(ctx)) return false;

    // 3. Compute block probabilities (if enabled)
    if (ctx->config.enable_block_modeling) {
        if (!myelin_gpu_compute_block_probabilities(ctx)) return false;
    }

    // 4. Apply plasticity (if activity provided)
    if (activity) {
        if (!myelin_gpu_apply_plasticity(ctx, activity, dt)) return false;
        if (!myelin_gpu_commit_lamellae(ctx)) return false;
    }

    return true;
}

extern "C" bool myelin_gpu_recompute_all(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return false;

    // 1. Compute g-ratios
    if (!myelin_gpu_compute_g_ratios(ctx)) return false;

    // 2. Compute cable parameters
    if (!myelin_gpu_compute_cable_params(ctx)) return false;

    // 3. Compute velocities
    if (!myelin_gpu_compute_velocities(ctx)) return false;

    return true;
}

//=============================================================================
// API Implementation: Statistics
//=============================================================================

extern "C" float myelin_gpu_get_mean_g_ratio(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return 0.0f;

    // Zero the output
    float zero = 0.0f;
    cudaMemcpy(ctx->mean_g_ratio->data, &zero, sizeof(float), cudaMemcpyHostToDevice);

    // Compute mean via reduction
    size_t shared_mem = BLOCK_SIZE * sizeof(float);
    kernel_compute_mean<<<GRID_SIZE(ctx->n_axons), BLOCK_SIZE, shared_mem, ctx->stream>>>(
        (float*)ctx->g_ratios->data,
        (float*)ctx->mean_g_ratio->data,
        ctx->n_axons
    );

    float result;
    cudaMemcpy(&result, ctx->mean_g_ratio->data, sizeof(float), cudaMemcpyDeviceToHost);
    return result;
}

extern "C" float myelin_gpu_get_mean_velocity(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return 0.0f;

    float zero = 0.0f;
    cudaMemcpy(ctx->mean_velocity->data, &zero, sizeof(float), cudaMemcpyHostToDevice);

    size_t shared_mem = BLOCK_SIZE * sizeof(float);
    kernel_compute_mean<<<GRID_SIZE(ctx->n_axons), BLOCK_SIZE, shared_mem, ctx->stream>>>(
        (float*)ctx->conduction_velocities->data,
        (float*)ctx->mean_velocity->data,
        ctx->n_axons
    );

    float result;
    cudaMemcpy(&result, ctx->mean_velocity->data, sizeof(float), cudaMemcpyDeviceToHost);
    return result;
}

extern "C" float myelin_gpu_get_mean_integrity(myelin_gpu_context_t* ctx) {
    if (!ctx || ctx->n_axons == 0) return 0.0f;

    float zero = 0.0f;
    cudaMemcpy(ctx->mean_integrity->data, &zero, sizeof(float), cudaMemcpyHostToDevice);

    uint32_t total = ctx->n_axons * ctx->n_internodes;
    size_t shared_mem = BLOCK_SIZE * sizeof(float);
    kernel_compute_mean<<<GRID_SIZE(total), BLOCK_SIZE, shared_mem, ctx->stream>>>(
        (float*)ctx->integrity->data,
        (float*)ctx->mean_integrity->data,
        total
    );

    float result;
    cudaMemcpy(&result, ctx->mean_integrity->data, sizeof(float), cudaMemcpyDeviceToHost);
    return result;
}

extern "C" bool myelin_gpu_get_stats(const myelin_gpu_context_t* ctx, myelin_gpu_stats_t* stats) {
    if (!ctx || !stats) return false;

    stats->kernel_launches = ctx->kernel_launches;
    stats->g_ratio_computations = ctx->g_ratio_computations;
    stats->velocity_computations = ctx->velocity_computations;
    stats->plasticity_updates = ctx->plasticity_updates;
    stats->total_kernel_time_ms = ctx->total_kernel_time_ms;
    stats->n_axons = ctx->n_axons;
    stats->n_internodes = ctx->n_internodes;

    // Compute means
    stats->mean_g_ratio = myelin_gpu_get_mean_g_ratio((myelin_gpu_context_t*)ctx);
    stats->mean_velocity = myelin_gpu_get_mean_velocity((myelin_gpu_context_t*)ctx);
    stats->mean_integrity = myelin_gpu_get_mean_integrity((myelin_gpu_context_t*)ctx);

    // Estimate GPU memory used
    size_t tensor_size_1d = ctx->config.max_axons * sizeof(float);
    size_t tensor_size_2d = ctx->config.max_axons * ctx->config.max_internodes * sizeof(float);
    stats->gpu_memory_used = 6 * tensor_size_1d + 18 * tensor_size_2d;

    return true;
}

extern "C" void myelin_gpu_reset_stats(myelin_gpu_context_t* ctx) {
    if (!ctx) return;
    ctx->kernel_launches = 0;
    ctx->g_ratio_computations = 0;
    ctx->velocity_computations = 0;
    ctx->plasticity_updates = 0;
    ctx->total_kernel_time_ms = 0.0f;
}

//=============================================================================
// API Implementation: Tensor Access
//=============================================================================

extern "C" const nimcp_gpu_tensor_t* myelin_gpu_get_g_ratios(const myelin_gpu_context_t* ctx) {
    return ctx ? ctx->g_ratios : NULL;
}

extern "C" const nimcp_gpu_tensor_t* myelin_gpu_get_velocities(const myelin_gpu_context_t* ctx) {
    return ctx ? ctx->conduction_velocities : NULL;
}

extern "C" const nimcp_gpu_tensor_t* myelin_gpu_get_integrity(const myelin_gpu_context_t* ctx) {
    return ctx ? ctx->integrity : NULL;
}

extern "C" const nimcp_gpu_tensor_t* myelin_gpu_get_space_constants(const myelin_gpu_context_t* ctx) {
    return ctx ? ctx->space_constants : NULL;
}

//=============================================================================
// CPU Reference Implementations
//=============================================================================

extern "C" void myelin_cpu_compute_g_ratios(
    const float* axon_diameters,
    float* g_ratios,
    uint32_t n_axons
) {
    for (uint32_t i = 0; i < n_axons; i++) {
        float d = axon_diameters[i];
        float g_opt = 0.77f + 0.08f * expf(-d / 0.5f);
        g_ratios[i] = fminf(fmaxf(g_opt, 0.4f), 0.95f);
    }
}

extern "C" void myelin_cpu_compute_cable_params(
    const float* axon_diameters,
    const uint32_t* num_lamellae,
    float* space_constants,
    float* time_constants,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    for (uint32_t i = 0; i < n_axons; i++) {
        float d = axon_diameters[i];
        float d_cm = d * 1e-4f;

        for (uint32_t j = 0; j < n_internodes; j++) {
            uint32_t idx = i * n_internodes + j;
            uint32_t n_lam = num_lamellae[idx];

            float r_m = 1000.0f + 100.0f * n_lam;
            float c_m = 1.0f * powf(0.9f, (float)n_lam);
            float r_a = 100.0f / (NIMCP_PI_F * 0.25f * d_cm * d_cm);

            float lambda_cm = sqrtf(r_m * d_cm / (4.0f * r_a));
            float lambda_um = lambda_cm * 1e4f;
            float tau_ms = r_m * c_m * 1e-3f;

            space_constants[idx] = lambda_um;
            time_constants[idx] = tau_ms;
        }
    }
}

extern "C" void myelin_cpu_compute_velocities(
    const float* axon_diameters,
    const float* internode_lengths,
    const uint32_t* num_lamellae,
    const float* g_ratios,
    const float* compaction_scores,
    const float* integrity,
    float* velocities,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    for (uint32_t i = 0; i < n_axons; i++) {
        float d = axon_diameters[i];
        float g = g_ratios[i];
        float g_opt = 0.77f + 0.08f * expf(-d / 0.5f);
        float g_eff = 1.0f - 25.0f * (g - g_opt) * (g - g_opt);
        g_eff = fminf(fmaxf(g_eff, 0.5f), 1.0f);

        float total_length = 0.0f;
        float total_delay = 0.0f;

        for (uint32_t j = 0; j < n_internodes; j++) {
            uint32_t idx = i * n_internodes + j;
            float L = internode_lengths[idx];
            float compact = compaction_scores[idx];
            float integ = integrity[idx];

            if (L > 0.0f) {
                float v_hursh = 6.0f * d * g_eff * compact * integ;
                v_hursh = fminf(fmaxf(v_hursh, 0.5f), 150.0f);

                float delay = (L * 1e-6f) / v_hursh * 1e3f;
                total_length += L;
                total_delay += delay;
            }
        }

        if (total_delay > 0.0f) {
            velocities[i] = (total_length * 1e-6f) / (total_delay * 1e-3f);
            velocities[i] = fminf(fmaxf(velocities[i], 0.5f), 150.0f);
        } else {
            velocities[i] = 1.0f;  // Unmyelinated default
        }
    }
}

extern "C" void myelin_cpu_apply_plasticity(
    const float* activity,
    float* lamellae_fractional,
    uint32_t* num_lamellae,
    float k_max,
    float k_half,
    float hill_n,
    float dt,
    uint32_t n_axons,
    uint32_t n_internodes
) {
    for (uint32_t i = 0; i < n_axons; i++) {
        float A = activity[i];
        float A_n = powf(A, hill_n);
        float K_n = powf(k_half, hill_n);
        float hill_term = A_n / (K_n + A_n + 1e-9f);

        for (uint32_t j = 0; j < n_internodes; j++) {
            uint32_t idx = i * n_internodes + j;
            float current_N = lamellae_fractional[idx];

            float saturation = 1.0f - (current_N / 160.0f);
            saturation = fminf(fmaxf(saturation, 0.0f), 1.0f);

            float rate_myelin = k_max * hill_term * saturation;
            float rate_demyelin = 0.1f * (0.1f / (0.1f + A + 1e-9f));

            float net_rate = rate_myelin - rate_demyelin;
            float new_N = current_N + net_rate * dt;

            lamellae_fractional[idx] = fminf(fmaxf(new_N, 1.0f), 160.0f);
            num_lamellae[idx] = (uint32_t)roundf(lamellae_fractional[idx]);
        }
    }
}

#endif /* NIMCP_ENABLE_CUDA */
