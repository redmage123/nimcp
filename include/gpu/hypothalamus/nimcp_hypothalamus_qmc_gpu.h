/**
 * @file nimcp_hypothalamus_qmc_gpu.h
 * @brief GPU-Accelerated Quantum Monte Carlo for Hypothalamus EFE Computation
 *
 * WHAT: CUDA-accelerated Monte Carlo for Expected Free Energy (EFE) in drive system
 * WHY:  FEP-based drive optimization requires sampling over possible futures
 * HOW:  GPU-parallel MC sampling, EFE computation, policy optimization
 *
 * FREE ENERGY PRINCIPLE (FEP) APPLICATION:
 * ========================================
 * The hypothalamus as "steering subsystem" (Byrnes) can be viewed through FEP:
 *
 * Expected Free Energy (EFE) = Risk + Ambiguity
 *   G(π) = E_Q(o,s|π) [ log Q(s|π) - log P(o,s) ]
 *
 * For drives:
 * - Risk: Expected deviation from setpoints (homeostatic cost)
 * - Ambiguity: Uncertainty about future states
 * - Policy π: Drive-directed behavior selection
 *
 * GPU ACCELERATION:
 * =================
 * 1. Parallel MC sampling of future drive trajectories
 * 2. Batch EFE computation across policies
 * 3. Policy gradient estimation via REINFORCE
 * 4. Multi-agent parallel optimization
 *
 * INTEGRATION WITH OMNI GPU:
 * ==========================
 * Connects to nimcp_omni_gpu.h precision weighting for:
 * - Drive-biased attention allocation
 * - Precision-weighted prediction errors
 * - Active inference policy selection
 *
 * @version Phase 18: GPU Acceleration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_QMC_GPU_H
#define NIMCP_HYPOTHALAMUS_QMC_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/hypothalamus/nimcp_hypothalamus_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Maximum planning horizon for EFE */
#define NIMCP_HYPO_QMC_MAX_HORIZON         64

/** @brief Maximum number of policies to evaluate */
#define NIMCP_HYPO_QMC_MAX_POLICIES        256

/** @brief Default number of MC samples */
#define NIMCP_HYPO_QMC_DEFAULT_SAMPLES     1024

/** @brief Default CUDA block size for QMC */
#define NIMCP_HYPO_QMC_BLOCK_SIZE          256

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief EFE decomposition mode
 */
typedef enum {
    NIMCP_HYPO_EFE_RISK_ONLY = 0,     /**< Risk term only (homeostatic cost) */
    NIMCP_HYPO_EFE_AMBIGUITY_ONLY,    /**< Ambiguity term only */
    NIMCP_HYPO_EFE_FULL,              /**< Full EFE = Risk + Ambiguity */
    NIMCP_HYPO_EFE_PRAGMATIC          /**< Pragmatic value (instrumental) */
} nimcp_hypo_efe_mode_t;

/**
 * @brief Policy optimization method
 */
typedef enum {
    NIMCP_HYPO_OPT_SOFTMAX = 0,       /**< Softmax policy selection */
    NIMCP_HYPO_OPT_REINFORCE,         /**< REINFORCE gradient */
    NIMCP_HYPO_OPT_EXPECTED_SARSA,    /**< Expected SARSA */
    NIMCP_HYPO_OPT_MCTS               /**< Monte Carlo Tree Search */
} nimcp_hypo_opt_method_t;

/**
 * @brief Sampling strategy
 */
typedef enum {
    NIMCP_HYPO_SAMPLE_UNIFORM = 0,    /**< Uniform random sampling */
    NIMCP_HYPO_SAMPLE_IMPORTANCE,     /**< Importance sampling */
    NIMCP_HYPO_SAMPLE_STRATIFIED,     /**< Stratified sampling */
    NIMCP_HYPO_SAMPLE_SOBOL           /**< Quasi-random Sobol */
} nimcp_hypo_sample_method_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *===========================================================================*/

/**
 * @brief QMC EFE computation configuration
 */
typedef struct {
    nimcp_hypo_efe_mode_t efe_mode;          /**< EFE computation mode */
    nimcp_hypo_sample_method_t sample_method; /**< Sampling strategy */
    uint32_t num_samples;                     /**< Number of MC samples */
    uint32_t horizon;                         /**< Planning horizon (time steps) */
    float gamma;                              /**< Temporal discount factor */
    float risk_weight;                        /**< Weight for risk term */
    float ambiguity_weight;                   /**< Weight for ambiguity term */
    float precision_prior;                    /**< Prior precision for beliefs */
    uint32_t threads_per_block;              /**< CUDA threads per block */
} nimcp_hypo_qmc_efe_config_t;

/**
 * @brief Policy optimization configuration
 */
typedef struct {
    nimcp_hypo_opt_method_t method;          /**< Optimization method */
    float learning_rate;                      /**< Policy learning rate */
    float temperature;                        /**< Softmax temperature */
    float entropy_bonus;                      /**< Entropy regularization */
    uint32_t num_policies;                   /**< Number of policies to evaluate */
    uint32_t num_iterations;                 /**< Optimization iterations */
    bool use_baseline;                        /**< Use baseline for variance reduction */
} nimcp_hypo_qmc_opt_config_t;

/**
 * @brief Drive transition model configuration
 */
typedef struct {
    float process_noise;                     /**< Process noise variance */
    float observation_noise;                 /**< Observation noise variance */
    bool use_learned_dynamics;               /**< Use learned vs. fixed dynamics */
    float dynamics_uncertainty;              /**< Uncertainty in dynamics model */
} nimcp_hypo_qmc_dynamics_config_t;

/*=============================================================================
 * GPU STATE STRUCTURES
 *===========================================================================*/

/**
 * @brief GPU EFE computation state
 */
typedef struct {
    nimcp_gpu_tensor_t* sampled_trajectories; /**< [samples, horizon, drives] */
    nimcp_gpu_tensor_t* expected_observations; /**< [samples, horizon] */
    nimcp_gpu_tensor_t* risk_terms;           /**< [samples] */
    nimcp_gpu_tensor_t* ambiguity_terms;      /**< [samples] */
    nimcp_gpu_tensor_t* efe_values;           /**< [policies] */
    qmc_gpu_rng_t rng;                        /**< GPU RNG state */
    size_t num_samples;
    size_t horizon;
} nimcp_hypo_qmc_efe_state_t;

/**
 * @brief GPU policy state
 */
typedef struct {
    nimcp_gpu_tensor_t* policy_params;       /**< Policy parameters */
    nimcp_gpu_tensor_t* policy_probs;        /**< Policy probabilities [policies] */
    nimcp_gpu_tensor_t* action_values;       /**< Q-values [policies, actions] */
    nimcp_gpu_tensor_t* gradients;           /**< Policy gradients */
    nimcp_gpu_tensor_t* baseline;            /**< Value baseline */
    size_t num_policies;
} nimcp_hypo_qmc_policy_state_t;

/**
 * @brief GPU drive dynamics model
 */
typedef struct {
    nimcp_gpu_tensor_t* transition_mean;     /**< Mean transition [drives, drives] */
    nimcp_gpu_tensor_t* transition_cov;      /**< Covariance [drives, drives] */
    nimcp_gpu_tensor_t* observation_model;   /**< Observation mapping */
    nimcp_gpu_tensor_t* noise_samples;       /**< Pre-sampled noise */
} nimcp_hypo_qmc_dynamics_t;

/*=============================================================================
 * CONFIGURATION DEFAULTS
 *===========================================================================*/

/**
 * @brief Get default EFE configuration
 */
NIMCP_EXPORT nimcp_hypo_qmc_efe_config_t nimcp_hypo_qmc_efe_config_default(void);

/**
 * @brief Get default optimization configuration
 */
NIMCP_EXPORT nimcp_hypo_qmc_opt_config_t nimcp_hypo_qmc_opt_config_default(void);

/**
 * @brief Get default dynamics configuration
 */
NIMCP_EXPORT nimcp_hypo_qmc_dynamics_config_t nimcp_hypo_qmc_dynamics_config_default(void);

/*=============================================================================
 * STATE LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create GPU EFE computation state
 *
 * @param ctx GPU context
 * @param num_samples Number of MC samples
 * @param horizon Planning horizon
 * @param seed RNG seed (0 = random)
 * @return EFE state handle or NULL
 */
NIMCP_EXPORT nimcp_hypo_qmc_efe_state_t* nimcp_hypo_qmc_efe_state_create(
    nimcp_gpu_context_t* ctx,
    size_t num_samples,
    size_t horizon,
    uint64_t seed);

/**
 * @brief Destroy GPU EFE state
 */
NIMCP_EXPORT void nimcp_hypo_qmc_efe_state_destroy(
    nimcp_hypo_qmc_efe_state_t* state);

/**
 * @brief Create GPU policy state
 */
NIMCP_EXPORT nimcp_hypo_qmc_policy_state_t* nimcp_hypo_qmc_policy_state_create(
    nimcp_gpu_context_t* ctx,
    size_t num_policies);

/**
 * @brief Destroy GPU policy state
 */
NIMCP_EXPORT void nimcp_hypo_qmc_policy_state_destroy(
    nimcp_hypo_qmc_policy_state_t* state);

/**
 * @brief Create GPU dynamics model
 */
NIMCP_EXPORT nimcp_hypo_qmc_dynamics_t* nimcp_hypo_qmc_dynamics_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_hypo_qmc_dynamics_config_t* config);

/**
 * @brief Destroy GPU dynamics model
 */
NIMCP_EXPORT void nimcp_hypo_qmc_dynamics_destroy(
    nimcp_hypo_qmc_dynamics_t* dynamics);

/*=============================================================================
 * EFE COMPUTATION KERNELS
 *===========================================================================*/

/**
 * @brief Sample drive trajectories
 *
 * GPU-parallel sampling of future drive state trajectories.
 *
 * @param ctx GPU context
 * @param state EFE state
 * @param initial_state Current drive state [batch, drives]
 * @param dynamics Dynamics model
 * @param config Sampling configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_sample_trajectories(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_gpu_tensor_t* initial_state,
    const nimcp_hypo_qmc_dynamics_t* dynamics,
    const nimcp_hypo_qmc_efe_config_t* config);

/**
 * @brief Compute risk term (expected homeostatic cost)
 *
 * Risk = E_Q [ log Q(s|π) - log P(s|setpoint) ]
 *      ≈ Expected deviation from setpoints
 *
 * @param ctx GPU context
 * @param state EFE state with sampled trajectories
 * @param setpoints Setpoint configuration
 * @param config EFE configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_compute_risk(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    const nimcp_hypo_qmc_efe_config_t* config);

/**
 * @brief Compute ambiguity term (expected entropy)
 *
 * Ambiguity = E_Q [ H(P(o|s)) ]
 *           = Expected uncertainty about observations
 *
 * @param ctx GPU context
 * @param state EFE state
 * @param dynamics Dynamics model (for observation uncertainty)
 * @param config EFE configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_compute_ambiguity(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_qmc_dynamics_t* dynamics,
    const nimcp_hypo_qmc_efe_config_t* config);

/**
 * @brief Compute full Expected Free Energy
 *
 * G(π) = risk_weight * Risk + ambiguity_weight * Ambiguity
 *
 * @param ctx GPU context
 * @param state EFE state (risk and ambiguity must be computed)
 * @param efe_out Output EFE values [policies]
 * @param config EFE configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_compute_efe(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    nimcp_gpu_tensor_t* efe_out,
    const nimcp_hypo_qmc_efe_config_t* config);

/*=============================================================================
 * POLICY OPTIMIZATION KERNELS
 *===========================================================================*/

/**
 * @brief Select policy via softmax over negative EFE
 *
 * P(π) ∝ exp(-G(π) / temperature)
 *
 * @param ctx GPU context
 * @param efe_values EFE values for each policy [policies]
 * @param policy_probs Output policy probabilities [policies]
 * @param temperature Softmax temperature
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_softmax_policy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* efe_values,
    nimcp_gpu_tensor_t* policy_probs,
    float temperature);

/**
 * @brief Compute policy gradient via REINFORCE
 *
 * ∇J(θ) ≈ -G(π) * ∇log π(a|s,θ)
 *
 * @param ctx GPU context
 * @param policy_state Policy state
 * @param efe_values EFE values
 * @param config Optimization configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_policy_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_gpu_tensor_t* efe_values,
    const nimcp_hypo_qmc_opt_config_t* config);

/**
 * @brief Update policy parameters
 *
 * θ ← θ - α * ∇J(θ)
 *
 * @param ctx GPU context
 * @param policy_state Policy state with computed gradients
 * @param config Optimization configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_policy_update(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_hypo_qmc_opt_config_t* config);

/**
 * @brief Update baseline for variance reduction
 *
 * @param ctx GPU context
 * @param policy_state Policy state
 * @param returns Observed returns
 * @param learning_rate Baseline learning rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_update_baseline(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_policy_state_t* policy_state,
    const nimcp_gpu_tensor_t* returns,
    float learning_rate);

/*=============================================================================
 * ACTIVE INFERENCE INTEGRATION
 *===========================================================================*/

/**
 * @brief Compute precision-weighted prediction errors
 *
 * Integration with FEP precision weighting.
 * ε = Π * (o - g(s))
 *
 * @param ctx GPU context
 * @param predicted Predicted observations
 * @param actual Actual observations
 * @param precision Precision weights
 * @param error_out Weighted prediction error
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_precision_error(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* predicted,
    const nimcp_gpu_tensor_t* actual,
    const nimcp_gpu_tensor_t* precision,
    nimcp_gpu_tensor_t* error_out);

/**
 * @brief Update beliefs about drive states
 *
 * Belief update under active inference:
 * Q(s) ← Q(s) + learning_rate * precision_error
 *
 * @param ctx GPU context
 * @param beliefs Current beliefs [drives]
 * @param precision_error Precision-weighted errors
 * @param learning_rate Belief update rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_belief_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* beliefs,
    const nimcp_gpu_tensor_t* precision_error,
    float learning_rate);

/*=============================================================================
 * ALIGNMENT-AWARE EFE
 *===========================================================================*/

/**
 * @brief Compute alignment-constrained EFE
 *
 * Adds alignment terms to EFE computation:
 * G_aligned(π) = G(π) + λ * alignment_penalty(π)
 *
 * @param ctx GPU context
 * @param state EFE state
 * @param setpoints Setpoints with alignment weights
 * @param alignment_weight Weight for alignment penalty
 * @param efe_out Output aligned EFE [policies]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_aligned_efe(
    nimcp_gpu_context_t* ctx,
    nimcp_hypo_qmc_efe_state_t* state,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    float alignment_weight,
    nimcp_gpu_tensor_t* efe_out);

/**
 * @brief Check alignment constraint satisfaction
 *
 * @param ctx GPU context
 * @param policy_probs Current policy distribution
 * @param setpoints Alignment constraints
 * @param satisfied_out Output boolean mask [policies]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_hypo_qmc_check_alignment(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* policy_probs,
    const nimcp_hypo_gpu_setpoints_t* setpoints,
    nimcp_gpu_tensor_t* satisfied_out);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_QMC_GPU_H */
