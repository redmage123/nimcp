/**
 * @file nimcp_myelin_gpu.h
 * @brief GPU-Accelerated Myelin Sheath Module
 *
 * WHAT: C API for GPU-accelerated myelin sheath operations using tensors
 * WHY:  GPU acceleration for parallel myelin biophysics computation
 * HOW:  Wraps CUDA kernels for batch G-ratio, cable theory, saltatory conduction
 *
 * BIOLOGICAL BASIS:
 * =================
 * Myelin sheaths wrap around axons to enable saltatory conduction:
 * - G-ratio optimization: Rushton's theory for optimal myelination
 * - Cable theory: Space and time constants for passive propagation
 * - Saltatory conduction: Jumping action potentials between nodes
 * - Activity-dependent plasticity: Neural activity modulates myelination
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Myelin network simulation involves:
 * - Thousands of axons with multiple segments each
 * - Per-segment biophysical calculations (g-ratio, velocity, block)
 * - Network-wide statistical aggregations
 * - Activity-dependent plasticity updates
 *
 * TENSOR-BASED DESIGN:
 * ====================
 * All data stored in nimcp_gpu_tensor_t for:
 * - Efficient memory coalescing
 * - Seamless integration with GPU tensor operations
 * - Batch processing of all segments/axons in parallel
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * myelin_gpu_context_t* myelin_gpu = myelin_gpu_create(ctx, &config);
 *
 * // Initialize myelin network from CPU data
 * myelin_gpu_init_from_cpu(myelin_gpu, &cpu_network);
 *
 * // Run GPU-accelerated batch computations
 * myelin_gpu_compute_g_ratios(myelin_gpu);
 * myelin_gpu_compute_velocities(myelin_gpu);
 * myelin_gpu_apply_plasticity(myelin_gpu, activity_tensor, dt);
 *
 * myelin_gpu_destroy(myelin_gpu);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_MYELIN_GPU_H
#define NIMCP_MYELIN_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

/** @brief Opaque GPU myelin context */
typedef struct myelin_gpu_context myelin_gpu_context_t;

/** @brief Forward declaration of CPU myelin network */
typedef struct myelin_sheath_network myelin_sheath_network_t;

//=============================================================================
// Constants
//=============================================================================

/** @name GPU Myelin Defaults */
///@{
#define MYELIN_GPU_DEFAULT_MAX_AXONS        8192    /**< Default max axons */
#define MYELIN_GPU_DEFAULT_MAX_INTERNODES   32      /**< Default max internodes per axon */
#define MYELIN_GPU_DEFAULT_BLOCK_SIZE       256     /**< Default CUDA block size */
///@}

/** @name Biophysical Constants (GPU-side) */
///@{
#define MYELIN_GPU_G_RATIO_OPTIMAL          0.77f   /**< Optimal CNS g-ratio */
#define MYELIN_GPU_G_RATIO_ALPHA            0.08f   /**< Small axon correction */
#define MYELIN_GPU_D_CRITICAL               0.5f    /**< Critical diameter (um) */
#define MYELIN_GPU_INTERNODE_ALPHA          150.0f  /**< Internode scaling */
#define MYELIN_GPU_INTERNODE_BETA           0.9f    /**< Internode power */
#define MYELIN_GPU_VELOCITY_COEFF           6.0f    /**< Hursh's law coefficient */
#define MYELIN_GPU_TAU_NODE_MS              0.03f   /**< Node delay (ms) */
///@}

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief GPU myelin configuration
 */
typedef struct {
    uint32_t max_axons;              /**< Maximum number of axons */
    uint32_t max_internodes;         /**< Maximum internodes per axon */
    uint32_t max_nodes;              /**< Maximum nodes (internodes - 1) */
    float target_g_ratio;            /**< Target g-ratio for optimization */
    float temperature_c;             /**< Temperature for block modeling (C) */
    float myelination_rate_max;      /**< Max myelination rate (lamellae/s) */
    float demyelination_rate;        /**< Demyelination rate constant */
    float hill_coefficient;          /**< Hill coefficient for kinetics */
    bool enable_stochastic;          /**< Enable stochastic variability */
    bool enable_block_modeling;      /**< Enable conduction block */
    bool enable_async_transfer;      /**< Enable async CPU-GPU transfers */
} myelin_gpu_config_t;

/**
 * @brief Get default GPU myelin configuration
 */
NIMCP_EXPORT myelin_gpu_config_t myelin_gpu_default_config(void);

//=============================================================================
// GPU Tensor-Based Context Structure
//=============================================================================

/**
 * @brief GPU myelin context with tensor-based data
 *
 * All myelin data stored in GPU tensors for efficient parallel processing.
 * Layout optimized for coalesced memory access patterns.
 */
struct myelin_gpu_context {
    // === GPU Context ===
    nimcp_gpu_context_t* gpu_ctx;    /**< Parent GPU context */
    myelin_gpu_config_t config;       /**< Configuration */

    // === Network Dimensions ===
    uint32_t n_axons;                 /**< Current number of axons */
    uint32_t n_internodes;            /**< Internodes per axon (padded) */
    uint32_t n_nodes;                 /**< Nodes per axon */

    // === Primary Tensor Data ===
    nimcp_gpu_tensor_t* g_ratios;            /**< [N_axons] Rushton G-ratio */
    nimcp_gpu_tensor_t* sheath_thickness;    /**< [N_axons, N_internodes] */
    nimcp_gpu_tensor_t* internode_lengths;   /**< [N_axons, N_internodes] */
    nimcp_gpu_tensor_t* node_widths;         /**< [N_axons, N_nodes] Ranvier nodes */
    nimcp_gpu_tensor_t* conduction_velocities; /**< [N_axons] */
    nimcp_gpu_tensor_t* capacitance;         /**< [N_axons, N_internodes] */

    // === Structural Properties ===
    nimcp_gpu_tensor_t* axon_diameters;      /**< [N_axons] Inner diameter (um) */
    nimcp_gpu_tensor_t* outer_diameters;     /**< [N_axons] Fiber diameter (um) */
    nimcp_gpu_tensor_t* num_lamellae;        /**< [N_axons, N_internodes] */
    nimcp_gpu_tensor_t* compaction_scores;   /**< [N_axons, N_internodes] (0-1) */

    // === Cable Theory Parameters ===
    nimcp_gpu_tensor_t* space_constants;     /**< [N_axons, N_internodes] lambda (um) */
    nimcp_gpu_tensor_t* time_constants;      /**< [N_axons, N_internodes] tau (ms) */
    nimcp_gpu_tensor_t* membrane_resistance; /**< [N_axons, N_internodes] r_m */
    nimcp_gpu_tensor_t* axial_resistance;    /**< [N_axons, N_internodes] r_a */

    // === Conduction Properties ===
    nimcp_gpu_tensor_t* segment_velocities;  /**< [N_axons, N_internodes] (m/s) */
    nimcp_gpu_tensor_t* propagation_delays;  /**< [N_axons, N_internodes] (ms) */
    nimcp_gpu_tensor_t* block_probabilities; /**< [N_axons, N_internodes] (0-1) */
    nimcp_gpu_tensor_t* is_conducting;       /**< [N_axons, N_internodes] (bool as float) */

    // === Health/Integrity ===
    nimcp_gpu_tensor_t* integrity;           /**< [N_axons, N_internodes] (0-1) */
    nimcp_gpu_tensor_t* damage_accumulated;  /**< [N_axons, N_internodes] */

    // === Plasticity State ===
    nimcp_gpu_tensor_t* activity_ema;        /**< [N_axons] Exponential moving average */
    nimcp_gpu_tensor_t* lamellae_fractional; /**< [N_axons, N_internodes] for accumulation */

    // === Temporary Buffers ===
    nimcp_gpu_tensor_t* temp_buffer_1d;      /**< [N_axons] temp workspace */
    nimcp_gpu_tensor_t* temp_buffer_2d;      /**< [N_axons, N_internodes] temp workspace */

    // === Statistics (GPU-side) ===
    nimcp_gpu_tensor_t* mean_g_ratio;        /**< [1] Network mean g-ratio */
    nimcp_gpu_tensor_t* mean_velocity;       /**< [1] Network mean velocity */
    nimcp_gpu_tensor_t* mean_integrity;      /**< [1] Network mean integrity */

    // === CUDA Stream ===
#ifdef NIMCP_ENABLE_CUDA
    cudaStream_t stream;                     /**< CUDA stream for async ops */
#else
    void* stream;
#endif

    // === Performance Counters ===
    uint64_t kernel_launches;                /**< Total kernel launches */
    uint64_t g_ratio_computations;           /**< G-ratio kernel calls */
    uint64_t velocity_computations;          /**< Velocity kernel calls */
    uint64_t plasticity_updates;             /**< Plasticity kernel calls */
    float total_kernel_time_ms;              /**< Total GPU kernel time */
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU myelin context
 *
 * WHAT: Initialize GPU resources for myelin acceleration
 * WHY:  Allocate GPU tensors for all myelin properties
 * HOW:  Create tensors with specified dimensions
 *
 * @param gpu_ctx GPU context (required)
 * @param config Configuration (NULL for defaults)
 * @return New GPU myelin context, or NULL on failure
 */
NIMCP_EXPORT myelin_gpu_context_t* myelin_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const myelin_gpu_config_t* config
);

/**
 * @brief Destroy GPU myelin context
 *
 * @param ctx GPU myelin context to destroy
 */
NIMCP_EXPORT void myelin_gpu_destroy(myelin_gpu_context_t* ctx);

/**
 * @brief Synchronize GPU myelin operations
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_synchronize(myelin_gpu_context_t* ctx);

/**
 * @brief Reset GPU myelin context to initial state
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_reset(myelin_gpu_context_t* ctx);

//=============================================================================
// Data Upload/Download Functions
//=============================================================================

/**
 * @brief Initialize GPU myelin from CPU network
 *
 * WHAT: Upload CPU myelin network data to GPU tensors
 * WHY:  Populate GPU with existing simulation state
 * HOW:  Extract data from CPU structures, upload to tensors
 *
 * @param ctx GPU myelin context
 * @param cpu_network CPU myelin network
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_init_from_cpu(
    myelin_gpu_context_t* ctx,
    const myelin_sheath_network_t* cpu_network
);

/**
 * @brief Upload axon properties to GPU
 *
 * WHAT: Upload axon diameters and segment counts
 * WHY:  Initialize structural properties
 * HOW:  Copy host arrays to GPU tensors
 *
 * @param ctx GPU myelin context
 * @param axon_diameters Host array of axon diameters (um)
 * @param internode_lengths Host array of internode lengths [n_axons * max_internodes]
 * @param n_axons Number of axons
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_upload_axon_properties(
    myelin_gpu_context_t* ctx,
    const float* axon_diameters,
    const float* internode_lengths,
    uint32_t n_axons
);

/**
 * @brief Upload lamellae counts to GPU
 *
 * @param ctx GPU myelin context
 * @param lamellae Host array [n_axons * max_internodes]
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_upload_lamellae(
    myelin_gpu_context_t* ctx,
    const uint32_t* lamellae
);

/**
 * @brief Upload integrity values to GPU
 *
 * @param ctx GPU myelin context
 * @param integrity Host array [n_axons * max_internodes]
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_upload_integrity(
    myelin_gpu_context_t* ctx,
    const float* integrity
);

/**
 * @brief Download computed velocities to host
 *
 * @param ctx GPU myelin context
 * @param velocities Host array [n_axons]
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_download_velocities(
    const myelin_gpu_context_t* ctx,
    float* velocities
);

/**
 * @brief Download all results to CPU network
 *
 * @param ctx GPU myelin context
 * @param cpu_network CPU myelin network to update
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_download_to_cpu(
    const myelin_gpu_context_t* ctx,
    myelin_sheath_network_t* cpu_network
);

//=============================================================================
// Core Kernel Functions: G-Ratio Computation
//=============================================================================

/**
 * @brief Compute optimal G-ratios (GPU-accelerated)
 *
 * WHAT: Calculate Rushton optimal g-ratio for each axon
 * WHY:  G-ratio optimization is fundamental to myelination
 * HOW:  GPU kernel applies diameter-dependent formula
 *
 * FORMULA:
 *   g_opt(d) = g_base + alpha * exp(-d / d_critical)
 *   g_base = 0.77 (large axon optimal)
 *   alpha = 0.08 (small axon correction)
 *   d_critical = 0.5 um
 *
 * BIOLOGICAL BASIS:
 * - Rushton (1951): Optimal g-ratio theory
 * - Chomiak & Bhm (2009): Diameter dependence
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_g_ratios(myelin_gpu_context_t* ctx);

/**
 * @brief Compute g-ratio efficiency factors (GPU)
 *
 * WHAT: Calculate efficiency based on deviation from optimal
 * WHY:  Suboptimal g-ratio reduces conduction efficiency
 * HOW:  Parabolic efficiency centered on optimal
 *
 * @param ctx GPU myelin context
 * @param efficiency_out Output tensor [N_axons] (can be NULL to use internal)
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_g_efficiency(
    myelin_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* efficiency_out
);

//=============================================================================
// Core Kernel Functions: Cable Theory
//=============================================================================

/**
 * @brief Compute cable theory parameters (GPU-accelerated)
 *
 * WHAT: Calculate space constant lambda and time constant tau
 * WHY:  Required for accurate passive signal propagation
 * HOW:  GPU kernel computes for all segments in parallel
 *
 * FORMULAS:
 *   r_m = r_m_base + r_m_per_lamella * n_lamellae
 *   c_m = c_m_base * (c_m_reduction ^ n_lamellae)
 *   lambda = sqrt(r_m * d / (4 * r_a))
 *   tau = r_m * c_m
 *
 * BIOLOGICAL BASIS:
 * - Hodgkin & Rushton (1946): Cable theory
 * - Jack, Noble & Tsien (1983): Myelin modifications
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_cable_params(myelin_gpu_context_t* ctx);

/**
 * @brief Compute signal attenuation (GPU)
 *
 * WHAT: Calculate voltage decay along each segment
 * WHY:  Passive signals attenuate exponentially
 * HOW:  exp(-L/lambda) decay formula
 *
 * @param ctx GPU myelin context
 * @param attenuation_out Output tensor [N_axons, N_internodes]
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_attenuation(
    myelin_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* attenuation_out
);

//=============================================================================
// Core Kernel Functions: Saltatory Conduction
//=============================================================================

/**
 * @brief Compute saltatory conduction velocities (GPU-accelerated)
 *
 * WHAT: Calculate action potential propagation velocity
 * WHY:  Primary metric for myelination effectiveness
 * HOW:  Combined node and internode delay model
 *
 * FORMULA:
 *   v = L_internode / (tau_node + tau_internode)
 *   tau_internode = L^2 / (lambda^2 * v_passive)
 *   v_final = v * g_efficiency * compaction * integrity
 *
 * BIOLOGICAL BASIS:
 * - Waxman & Bennett (1972): Saltatory conduction theory
 * - Ritchie (1995): Node delay measurements
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_velocities(myelin_gpu_context_t* ctx);

/**
 * @brief Compute segment-level velocities (GPU)
 *
 * WHAT: Calculate velocity for each segment independently
 * WHY:  Some analyses need per-segment detail
 * HOW:  Same formula as above, stored per segment
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_segment_velocities(myelin_gpu_context_t* ctx);

/**
 * @brief Compute propagation delays (GPU)
 *
 * WHAT: Calculate time for AP to traverse each segment
 * WHY:  Required for timing-sensitive neural circuits
 * HOW:  Length divided by velocity plus node delay
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_delays(myelin_gpu_context_t* ctx);

/**
 * @brief Compute total axon delays (GPU)
 *
 * WHAT: Sum segment delays for total axon delay
 * WHY:  Network-level timing calculations
 * HOW:  GPU reduction along internode dimension
 *
 * @param ctx GPU myelin context
 * @param total_delays_out Output tensor [N_axons]
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_total_delays(
    myelin_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* total_delays_out
);

//=============================================================================
// Core Kernel Functions: Myelin Plasticity
//=============================================================================

/**
 * @brief Apply activity-dependent myelination (GPU-accelerated)
 *
 * WHAT: Update lamellae based on neural activity
 * WHY:  Activity-dependent plasticity is biological reality
 * HOW:  Hill-function kinetics with saturation
 *
 * FORMULA:
 *   rate_myelin = k_max * (A^n / (K^n + A^n)) * (1 - N/N_max)
 *   rate_demyelin = k_demyelin * (K_d / (K_d + A))
 *   net_rate = rate_myelin - rate_demyelin
 *
 * BIOLOGICAL BASIS:
 * - Fields (2015): Activity-dependent myelination
 * - Gibson et al. (2014): Neural activity regulation
 *
 * @param ctx GPU myelin context
 * @param activity Activity levels per axon [N_axons] (0-1 normalized)
 * @param dt Time step (seconds)
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_apply_plasticity(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    float dt
);

/**
 * @brief Update activity EMA (GPU)
 *
 * WHAT: Update exponential moving average of activity
 * WHY:  Smooth activity signal for plasticity decisions
 * HOW:  EMA formula with configurable time constant
 *
 * @param ctx GPU myelin context
 * @param activity Current activity levels [N_axons]
 * @param dt Time step (seconds)
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_update_activity_ema(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    float dt
);

/**
 * @brief Commit lamellae changes (GPU)
 *
 * WHAT: Convert fractional lamellae accumulation to integers
 * WHY:  Lamellae are discrete but changes are continuous
 * HOW:  Round fractional values and clamp to valid range
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_commit_lamellae(myelin_gpu_context_t* ctx);

//=============================================================================
// Core Kernel Functions: Conduction Block
//=============================================================================

/**
 * @brief Compute conduction block probabilities (GPU-accelerated)
 *
 * WHAT: Calculate probability of signal failure
 * WHY:  Model pathological conditions (MS, temperature effects)
 * HOW:  Sigmoid function with temperature modulation
 *
 * FORMULA:
 *   P_base = 1 / (1 + exp((I - I_crit) / sigma))
 *   T_factor = 1 + k_T * max(0, T - T_ref)
 *   P_block = P_base * T_factor
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_block_probabilities(myelin_gpu_context_t* ctx);

/**
 * @brief Apply conduction blocks (GPU)
 *
 * WHAT: Stochastically determine blocked segments
 * WHY:  Apply probabilities to individual segments
 * HOW:  Compare random values to block probabilities
 *
 * @param ctx GPU myelin context
 * @param seed Random seed for reproducibility
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_apply_blocks(
    myelin_gpu_context_t* ctx,
    uint64_t seed
);

/**
 * @brief Set temperature for block modeling
 *
 * @param ctx GPU myelin context
 * @param temperature_c Temperature in Celsius
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_set_temperature(
    myelin_gpu_context_t* ctx,
    float temperature_c
);

//=============================================================================
// Damage and Repair Functions
//=============================================================================

/**
 * @brief Apply damage to myelin (GPU)
 *
 * WHAT: Reduce integrity of specified segments
 * WHY:  Model demyelination and pathology
 * HOW:  Subtract damage from integrity, clamp to [0,1]
 *
 * @param ctx GPU myelin context
 * @param damage Damage tensor [N_axons, N_internodes] or [N_axons]
 * @param broadcast_axon If true, damage is [N_axons] and applies to all segments
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_apply_damage(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* damage,
    bool broadcast_axon
);

/**
 * @brief Apply repair to myelin (GPU)
 *
 * WHAT: Increase integrity of damaged segments
 * WHY:  Model remyelination
 * HOW:  Add repair amount, clamp to [0,1]
 *
 * @param ctx GPU myelin context
 * @param repair_rate Repair rate per second
 * @param dt Time step (seconds)
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_apply_repair(
    myelin_gpu_context_t* ctx,
    float repair_rate,
    float dt
);

/**
 * @brief Apply natural degradation (GPU)
 *
 * WHAT: Gradual integrity loss without maintenance
 * WHY:  Myelin requires metabolic support
 * HOW:  Small per-step integrity reduction
 *
 * @param ctx GPU myelin context
 * @param decay_rate Decay rate per second
 * @param dt Time step (seconds)
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_apply_decay(
    myelin_gpu_context_t* ctx,
    float decay_rate,
    float dt
);

//=============================================================================
// Batch Computation Functions
//=============================================================================

/**
 * @brief Run full myelin simulation step (GPU)
 *
 * WHAT: Execute complete update cycle on GPU
 * WHY:  Convenience function for common use case
 * HOW:  Chains: cable -> velocity -> block -> plasticity
 *
 * @param ctx GPU myelin context
 * @param activity Activity levels per axon [N_axons] (can be NULL)
 * @param dt Time step (seconds)
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_step(
    myelin_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    float dt
);

/**
 * @brief Compute all biophysical properties (GPU)
 *
 * WHAT: Recalculate all derived properties
 * WHY:  After structural changes, update all calculations
 * HOW:  Chains: g-ratio -> cable -> velocity
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_recompute_all(myelin_gpu_context_t* ctx);

//=============================================================================
// Statistics and Aggregation Functions
//=============================================================================

/**
 * @brief Compute network statistics (GPU)
 *
 * WHAT: Calculate mean values across network
 * WHY:  Monitor network-wide myelin health
 * HOW:  GPU reductions to compute means
 *
 * @param ctx GPU myelin context
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_compute_statistics(myelin_gpu_context_t* ctx);

/**
 * @brief Get network mean g-ratio
 *
 * @param ctx GPU myelin context
 * @return Mean g-ratio (downloads from GPU)
 */
NIMCP_EXPORT float myelin_gpu_get_mean_g_ratio(myelin_gpu_context_t* ctx);

/**
 * @brief Get network mean velocity
 *
 * @param ctx GPU myelin context
 * @return Mean velocity in m/s (downloads from GPU)
 */
NIMCP_EXPORT float myelin_gpu_get_mean_velocity(myelin_gpu_context_t* ctx);

/**
 * @brief Get network mean integrity
 *
 * @param ctx GPU myelin context
 * @return Mean integrity (0-1)
 */
NIMCP_EXPORT float myelin_gpu_get_mean_integrity(myelin_gpu_context_t* ctx);

/**
 * @brief GPU myelin statistics structure
 */
typedef struct {
    uint64_t kernel_launches;        /**< Total kernel launches */
    uint64_t g_ratio_computations;   /**< G-ratio kernel calls */
    uint64_t velocity_computations;  /**< Velocity kernel calls */
    uint64_t plasticity_updates;     /**< Plasticity kernel calls */
    float total_kernel_time_ms;      /**< Total GPU kernel time */
    float mean_g_ratio;              /**< Network mean g-ratio */
    float mean_velocity;             /**< Network mean velocity (m/s) */
    float mean_integrity;            /**< Network mean integrity */
    uint32_t n_axons;                /**< Number of axons */
    uint32_t n_internodes;           /**< Internodes per axon */
    size_t gpu_memory_used;          /**< GPU memory in use (bytes) */
} myelin_gpu_stats_t;

/**
 * @brief Get GPU myelin statistics
 *
 * @param ctx GPU myelin context
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool myelin_gpu_get_stats(
    const myelin_gpu_context_t* ctx,
    myelin_gpu_stats_t* stats
);

/**
 * @brief Reset GPU myelin statistics
 *
 * @param ctx GPU myelin context
 */
NIMCP_EXPORT void myelin_gpu_reset_stats(myelin_gpu_context_t* ctx);

//=============================================================================
// Direct Tensor Access
//=============================================================================

/**
 * @brief Get g-ratios tensor (read-only)
 *
 * @param ctx GPU myelin context
 * @return GPU tensor [N_axons] or NULL
 */
NIMCP_EXPORT const nimcp_gpu_tensor_t* myelin_gpu_get_g_ratios(
    const myelin_gpu_context_t* ctx
);

/**
 * @brief Get velocities tensor (read-only)
 *
 * @param ctx GPU myelin context
 * @return GPU tensor [N_axons] or NULL
 */
NIMCP_EXPORT const nimcp_gpu_tensor_t* myelin_gpu_get_velocities(
    const myelin_gpu_context_t* ctx
);

/**
 * @brief Get integrity tensor (read-only)
 *
 * @param ctx GPU myelin context
 * @return GPU tensor [N_axons, N_internodes] or NULL
 */
NIMCP_EXPORT const nimcp_gpu_tensor_t* myelin_gpu_get_integrity(
    const myelin_gpu_context_t* ctx
);

/**
 * @brief Get space constants tensor (read-only)
 *
 * @param ctx GPU myelin context
 * @return GPU tensor [N_axons, N_internodes] or NULL
 */
NIMCP_EXPORT const nimcp_gpu_tensor_t* myelin_gpu_get_space_constants(
    const myelin_gpu_context_t* ctx
);

//=============================================================================
// CPU Reference Implementations (for testing)
//=============================================================================

/**
 * @brief CPU reference implementation of g-ratio computation
 *
 * WHAT: CPU equivalent for GPU/CPU equivalence testing
 * WHY:  Verify GPU results match CPU
 */
NIMCP_EXPORT void myelin_cpu_compute_g_ratios(
    const float* axon_diameters,
    float* g_ratios,
    uint32_t n_axons
);

/**
 * @brief CPU reference implementation of cable parameters
 */
NIMCP_EXPORT void myelin_cpu_compute_cable_params(
    const float* axon_diameters,
    const uint32_t* num_lamellae,
    float* space_constants,
    float* time_constants,
    uint32_t n_axons,
    uint32_t n_internodes
);

/**
 * @brief CPU reference implementation of saltatory velocity
 */
NIMCP_EXPORT void myelin_cpu_compute_velocities(
    const float* axon_diameters,
    const float* internode_lengths,
    const uint32_t* num_lamellae,
    const float* g_ratios,
    const float* compaction_scores,
    const float* integrity,
    float* velocities,
    uint32_t n_axons,
    uint32_t n_internodes
);

/**
 * @brief CPU reference implementation of plasticity update
 */
NIMCP_EXPORT void myelin_cpu_apply_plasticity(
    const float* activity,
    float* lamellae_fractional,
    uint32_t* num_lamellae,
    float k_max,
    float k_half,
    float hill_n,
    float dt,
    uint32_t n_axons,
    uint32_t n_internodes
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MYELIN_GPU_H */
