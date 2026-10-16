/**
 * @file nimcp_free_energy.h
 * @brief Free Energy Principle (FEP) Module
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of Karl Friston's Free Energy Principle for cognitive modeling
 * WHY:  FEP provides a unifying framework for understanding perception, action, and
 *       learning as processes that minimize prediction error (variational free energy).
 * HOW:  Track generative models, compute prediction errors, perform belief updates,
 *       and guide active inference for action selection.
 *
 * THEORETICAL FOUNDATION:
 * ==================================================================================
 *
 * FREE ENERGY PRINCIPLE (Friston, 2006-2024):
 * ------------------------------------------
 * The brain minimizes variational free energy F, which bounds surprise:
 *
 *   F = E_q[ln q(s) - ln p(o,s)] ≥ -ln p(o)
 *
 * Where:
 *   F = Free energy (upper bound on surprise)
 *   q(s) = Variational density (beliefs about hidden states)
 *   p(o,s) = Generative model (joint probability of observations and states)
 *   p(o) = Evidence (marginal likelihood of observations)
 *   s = Hidden states
 *   o = Observations
 *
 * FREE ENERGY DECOMPOSITION:
 * --------------------------
 * F can be decomposed as:
 *
 *   F = Complexity + Inaccuracy
 *     = KL[q(s)||p(s)] + E_q[-ln p(o|s)]
 *
 * Or equivalently:
 *   F = Energy - Entropy
 *     = E_q[-ln p(o,s)] + E_q[ln q(s)]
 *
 * PREDICTION ERROR MINIMIZATION:
 * ------------------------------
 * - Sensory prediction errors: ε_s = o - g(μ)
 * - State prediction errors: ε_x = μ - f(μ)
 *
 * Where:
 *   g(μ) = Generative model's predicted observations
 *   f(μ) = Generative model's predicted state transitions
 *   μ = Expected states (mode of variational density)
 *
 * ACTIVE INFERENCE:
 * -----------------
 * Actions minimize expected free energy (EFE):
 *
 *   G(π) = E_q(o,s|π)[ln q(s|π) - ln p(o,s|π)]
 *        = Risk + Ambiguity
 *
 * Where:
 *   π = Policy (sequence of actions)
 *   Risk = KL[q(o|π)||p(o)] = Deviation from preferred observations
 *   Ambiguity = E_q[H[p(o|s)]] = Expected uncertainty about observations
 *
 * PRECISION-WEIGHTED PREDICTION ERRORS:
 * ------------------------------------
 * Prediction errors are weighted by precision (inverse variance):
 *
 *   ε_weighted = Π * ε
 *
 * Where Π is the precision matrix. High precision errors have more influence.
 *
 * HIERARCHICAL PREDICTIVE PROCESSING:
 * -----------------------------------
 *   Level N+1: μ_(n+1)  ←─  ε_n (bottom-up prediction errors)
 *        ↓                   ↑
 *   Level N:   μ_n     ─→  g_n(μ_n) (top-down predictions)
 *        ↓                   ↑
 *   Level N-1: μ_(n-1) ←─  ε_(n-1)
 *
 * REFERENCES:
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Friston et al. (2017) "Active inference: A process theory"
 * - Parr, Pezzulo, Friston (2022) "Active Inference: The Free Energy Principle"
 * - Buckley et al. (2017) "The free energy principle for action and perception"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FREE ENERGY PRINCIPLE MODULE                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   GENERATIVE MODEL                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │                HIERARCHICAL LEVELS                           │ │  ║
 * ║   │   │                                                              │ │  ║
 * ║   │   │   Level 2: Abstract/Conceptual                               │ │  ║
 * ║   │   │      μ₂ = Expected abstract states                           │ │  ║
 * ║   │   │        ↓ predictions  ↑ prediction errors                   │ │  ║
 * ║   │   │   Level 1: Feature/Object                                    │ │  ║
 * ║   │   │      μ₁ = Expected features                                  │ │  ║
 * ║   │   │        ↓ predictions  ↑ prediction errors                   │ │  ║
 * ║   │   │   Level 0: Sensory                                           │ │  ║
 * ║   │   │      o = Observations                                        │ │  ║
 * ║   │   └──────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 BELIEF DYNAMICS                                     │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ Prior μ(t-1) │ → │  Prediction  │ → │ Posterior    │          │  ║
 * ║   │   │              │   │  Error ε     │   │ μ(t)         │          │  ║
 * ║   │   └──────────────┘   └──────────────┘   └──────────────┘          │  ║
 * ║   │                             ↑                                      │  ║
 * ║   │                      Observations o                                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 ACTIVE INFERENCE                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   Policy π → Expected Free Energy G(π) → Action Selection          │  ║
 * ║   │      ↓                                                             │  ║
 * ║   │   Action a → Environment → New Observation o'                      │  ║
 * ║   │                    ↓                                               │  ║
 * ║   │            Belief Update μ' = f(μ, o', a)                         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FREE_ENERGY_H
#define NIMCP_FREE_ENERGY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Hierarchy configuration */
#define FEP_MAX_HIERARCHY_LEVELS       8        /**< Maximum hierarchy depth */
#define FEP_MAX_STATE_DIM              256      /**< Maximum state dimension */
#define FEP_MAX_OBSERVATION_DIM        256      /**< Maximum observation dimension */
#define FEP_MAX_ACTION_DIM             64       /**< Maximum action dimension */
#define FEP_MAX_POLICIES               32       /**< Maximum policies to evaluate */

/* Default learning rates */
#define FEP_DEFAULT_BELIEF_LR          0.1f     /**< Belief update learning rate */
#define FEP_DEFAULT_PRECISION_LR       0.05f    /**< Precision learning rate */
#define FEP_DEFAULT_ACTION_LR          0.2f     /**< Action selection learning rate */

/* Precision bounds */
#define FEP_MIN_PRECISION              0.01f    /**< Minimum precision */
#define FEP_MAX_PRECISION              100.0f   /**< Maximum precision */
#define FEP_DEFAULT_PRECISION          1.0f     /**< Default precision */

/* Free energy thresholds */
#define FEP_SURPRISE_THRESHOLD         10.0f    /**< High surprise threshold */
#define FEP_CONVERGENCE_THRESHOLD      0.001f   /**< Belief convergence threshold */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Belief update modes
 */
typedef enum {
    FEP_UPDATE_GRADIENT_DESCENT = 0, /**< Standard gradient descent */
    FEP_UPDATE_PREDICTIVE_CODING,    /**< Predictive coding updates */
    FEP_UPDATE_VARIATIONAL_MESSAGE,  /**< Variational message passing */
    FEP_UPDATE_KALMAN_FILTER         /**< Kalman-like updates */
} fep_update_mode_t;

/**
 * @brief Action selection modes
 */
typedef enum {
    FEP_ACTION_SOFTMAX = 0,          /**< Softmax over EFE */
    FEP_ACTION_GREEDY,               /**< Greedy (min EFE) */
    FEP_ACTION_THOMPSON              /**< Thompson sampling */
} fep_action_mode_t;

/**
 * @brief Free energy components
 */
typedef enum {
    FEP_COMPONENT_COMPLEXITY = 0,    /**< KL divergence (complexity) */
    FEP_COMPONENT_INACCURACY,        /**< Expected log-likelihood (accuracy) */
    FEP_COMPONENT_ENERGY,            /**< Energy term */
    FEP_COMPONENT_ENTROPY            /**< Entropy term */
} fep_component_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Variational density (beliefs about hidden states)
 */
typedef struct {
    float* mean;                  /**< Mean μ (expected state) */
    float* variance;              /**< Variance σ² (uncertainty) */
    float* precision;             /**< Precision Π = 1/σ² */
    uint32_t dim;                 /**< Dimensionality */
} fep_belief_t;

/**
 * @brief Prediction error
 */
typedef struct {
    float* error;                 /**< Raw prediction error ε = o - g(μ) */
    float* weighted_error;        /**< Precision-weighted error Π*ε */
    float* precision;             /**< Precision weights */
    float magnitude;              /**< L2 norm of error */
    float weighted_magnitude;     /**< Weighted error magnitude */
    uint32_t dim;                 /**< Dimensionality */
} fep_prediction_error_t;

/**
 * @brief Single level in hierarchical generative model
 */
typedef struct {
    uint32_t level_id;            /**< Level index (0 = lowest) */

    /* Beliefs at this level */
    fep_belief_t beliefs;

    /* Predictions to lower level */
    float* predictions;           /**< g(μ) - predicted observations */
    uint32_t prediction_dim;

    /* Prediction errors from lower level */
    fep_prediction_error_t errors;

    /* Generative model parameters */
    float* transition_matrix;     /**< A: State transition */
    float* likelihood_matrix;     /**< B: Observation likelihood */
    float* prior_mean;            /**< Prior state mean */
    float* prior_precision;       /**< Prior precision */
} fep_hierarchy_level_t;

/**
 * @brief Policy (sequence of actions)
 */
typedef struct {
    uint32_t policy_id;           /**< Policy identifier */
    float* actions;               /**< Action sequence */
    uint32_t num_actions;         /**< Number of actions */
    uint32_t action_dim;          /**< Action dimensionality */
    float expected_free_energy;   /**< G(π) - EFE for this policy */
    float probability;            /**< P(π) - policy probability */
} fep_policy_t;

/**
 * @brief Expected Free Energy components for a policy
 */
typedef struct {
    float total;                  /**< Total EFE G(π) */
    float risk;                   /**< Risk component */
    float ambiguity;              /**< Ambiguity component */
    float intrinsic_value;        /**< Information gain */
    float extrinsic_value;        /**< Goal-directed value */
} fep_efe_t;

/**
 * @brief Free energy decomposition
 */
typedef struct {
    float total;                  /**< Total free energy F */
    float complexity;             /**< KL[q||p] - complexity cost */
    float inaccuracy;             /**< -E[ln p(o|s)] - accuracy cost */
    float energy;                 /**< E[-ln p(o,s)] */
    float entropy;                /**< -E[ln q(s)] */
    float surprise;               /**< -ln p(o) (lower bound) */
} fep_free_energy_t;

/**
 * @brief FEP statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t belief_updates;
    uint64_t action_selections;
    float avg_prediction_error;
    float avg_free_energy;
    float min_free_energy;
    float max_surprise;
    uint32_t convergence_failures;
} fep_stats_t;

/**
 * @brief FEP configuration
 */
typedef struct {
    /* Hierarchy configuration */
    uint32_t num_levels;
    uint32_t* level_dims;         /**< State dimension per level */

    /* Learning rates */
    float belief_learning_rate;
    float precision_learning_rate;
    float action_learning_rate;

    /* Update modes */
    fep_update_mode_t update_mode;
    fep_action_mode_t action_mode;

    /* Precision settings */
    float initial_precision;
    bool learn_precision;

    /* Active inference settings */
    bool enable_active_inference;
    uint32_t planning_horizon;
    float action_temperature;     /**< Softmax temperature for action selection */

    /* Convergence */
    uint32_t max_iterations;
    float convergence_threshold;
} fep_config_t;

/**
 * @brief Complete FEP system state
 */
typedef struct {
    /* Hierarchical generative model */
    fep_hierarchy_level_t* levels;
    uint32_t num_levels;

    /* Current observations */
    float* observations;
    uint32_t observation_dim;

    /* Action space */
    float* action_space;          /**< Available actions */
    uint32_t action_dim;
    uint32_t num_actions;

    /* Policy evaluation */
    fep_policy_t* policies;
    uint32_t num_policies;
    uint32_t selected_policy;

    /* Free energy tracking */
    fep_free_energy_t free_energy;
    fep_efe_t expected_free_energy;

    /* Configuration */
    fep_config_t config;

    /* Statistics */
    fep_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} fep_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard parameters
 * HOW:  Set biologically-plausible defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int fep_default_config(fep_config_t* config);

/**
 * @brief Create FEP system
 *
 * WHAT: Initialize Free Energy Principle system
 * WHY:  Enable predictive processing and active inference
 * HOW:  Allocate hierarchy, initialize beliefs
 *
 * @param config Configuration
 * @param observation_dim Observation dimensionality
 * @param action_dim Action dimensionality
 * @return New FEP system or NULL on failure
 */
fep_system_t* fep_create(
    const fep_config_t* config,
    uint32_t observation_dim,
    uint32_t action_dim
);

/**
 * @brief Destroy FEP system
 *
 * @param fep FEP system (NULL safe)
 */
void fep_destroy(fep_system_t* fep);

/**
 * @brief Reset FEP system to initial state
 *
 * @param fep FEP system
 * @return 0 on success
 */
int fep_reset(fep_system_t* fep);

/* ============================================================================
 * Observation Processing API
 * ============================================================================ */

/**
 * @brief Process new observation
 *
 * WHAT: Update beliefs given new observation
 * WHY:  Core of perception - minimize prediction error
 * HOW:  Compute error, update beliefs via gradient descent on F
 *
 * @param fep FEP system
 * @param observation Observation vector
 * @param observation_dim Observation dimensionality
 * @return 0 on success
 */
int fep_process_observation(
    fep_system_t* fep,
    const float* observation,
    uint32_t observation_dim
);

/**
 * @brief Compute prediction for current beliefs
 *
 * WHAT: Generate predicted observation from beliefs
 * WHY:  Core of generative model - predict sensory input
 * HOW:  Apply likelihood model g(μ)
 *
 * @param fep FEP system
 * @param prediction Output prediction vector
 * @param prediction_dim Max dimension
 * @return Actual dimension of prediction
 */
uint32_t fep_compute_prediction(
    const fep_system_t* fep,
    float* prediction,
    uint32_t prediction_dim
);

/**
 * @brief Compute prediction error
 *
 * WHAT: Calculate difference between observation and prediction
 * WHY:  Prediction error drives belief updates
 * HOW:  ε = o - g(μ)
 *
 * @param fep FEP system
 * @param error Output prediction error structure
 * @return 0 on success
 */
int fep_compute_prediction_error(
    const fep_system_t* fep,
    fep_prediction_error_t* error
);

/* ============================================================================
 * Belief Update API
 * ============================================================================ */

/**
 * @brief Update beliefs to minimize free energy
 *
 * WHAT: Gradient descent on variational free energy
 * WHY:  Perception as inference
 * HOW:  μ' = μ - lr * ∂F/∂μ
 *
 * @param fep FEP system
 * @return 0 on success
 */
int fep_update_beliefs(fep_system_t* fep);

/**
 * @brief Update precision estimates
 *
 * WHAT: Learn optimal precision (inverse variance)
 * WHY:  Attention as precision optimization
 * HOW:  Update Π based on prediction error statistics
 *
 * @param fep FEP system
 * @return 0 on success
 */
int fep_update_precision(fep_system_t* fep);

/**
 * @brief Propagate beliefs through hierarchy
 *
 * WHAT: Pass predictions down, errors up
 * WHY:  Hierarchical predictive coding
 * HOW:  Top-down predictions, bottom-up errors
 *
 * @param fep FEP system
 * @return 0 on success
 */
int fep_propagate_hierarchy(fep_system_t* fep);

/* ============================================================================
 * Free Energy Computation API
 * ============================================================================ */

/**
 * @brief Compute variational free energy
 *
 * WHAT: Calculate current free energy F
 * WHY:  Objective function for perception
 * HOW:  F = Complexity + Inaccuracy
 *
 * @param fep FEP system
 * @param fe Output free energy structure
 * @return 0 on success
 */
int fep_compute_free_energy(
    const fep_system_t* fep,
    fep_free_energy_t* fe
);

/**
 * @brief Compute specific free energy component
 *
 * @param fep FEP system
 * @param component Component to compute
 * @return Component value
 */
float fep_compute_component(
    const fep_system_t* fep,
    fep_component_t component
);

/**
 * @brief Compute surprise (negative log evidence)
 *
 * WHAT: Estimate -ln p(o)
 * WHY:  Free energy bounds surprise
 * HOW:  Use variational bound approximation
 *
 * @param fep FEP system
 * @return Surprise estimate
 */
float fep_compute_surprise(const fep_system_t* fep);

/* ============================================================================
 * Active Inference API
 * ============================================================================ */

/**
 * @brief Compute expected free energy for policy
 *
 * WHAT: Calculate G(π) for action selection
 * WHY:  Actions minimize expected free energy
 * HOW:  G(π) = Risk + Ambiguity
 *
 * @param fep FEP system
 * @param policy Policy to evaluate
 * @param efe Output EFE structure
 * @return 0 on success
 */
int fep_compute_efe(
    const fep_system_t* fep,
    const fep_policy_t* policy,
    fep_efe_t* efe
);

/**
 * @brief Evaluate all policies
 *
 * WHAT: Compute EFE for all available policies
 * WHY:  Policy selection requires comparison
 * HOW:  Evaluate each policy, compute probabilities
 *
 * @param fep FEP system
 * @return 0 on success
 */
int fep_evaluate_policies(fep_system_t* fep);

/**
 * @brief Select action via active inference
 *
 * WHAT: Choose action that minimizes EFE
 * WHY:  Core of active inference
 * HOW:  Softmax over -G(π) or greedy selection
 *
 * @param fep FEP system
 * @param action Output action vector
 * @param action_dim Max action dimension
 * @return Selected policy index, -1 on error
 */
int fep_select_action(
    fep_system_t* fep,
    float* action,
    uint32_t action_dim
);

/**
 * @brief Set preferred observations (goals)
 *
 * WHAT: Define what observations are desirable
 * WHY:  Goals encoded as preferred observations in FEP
 * HOW:  Set target distribution for risk computation
 *
 * @param fep FEP system
 * @param preferred Preferred observation vector
 * @param precision Goal precision (how important)
 * @param dim Dimensionality
 * @return 0 on success
 */
int fep_set_preferences(
    fep_system_t* fep,
    const float* preferred,
    float precision,
    uint32_t dim
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current beliefs at level
 *
 * @param fep FEP system
 * @param level Hierarchy level
 * @param beliefs Output beliefs
 * @return 0 on success
 */
int fep_get_beliefs(
    const fep_system_t* fep,
    uint32_t level,
    fep_belief_t* beliefs
);

/**
 * @brief Get current free energy
 *
 * @param fep FEP system
 * @return Current total free energy
 */
float fep_get_free_energy(const fep_system_t* fep);

/**
 * @brief Get prediction error magnitude
 *
 * @param fep FEP system
 * @param level Hierarchy level
 * @return Prediction error magnitude
 */
float fep_get_prediction_error(const fep_system_t* fep, uint32_t level);

/**
 * @brief Get selected policy
 *
 * @param fep FEP system
 * @param policy Output policy
 * @return 0 on success
 */
int fep_get_selected_policy(const fep_system_t* fep, fep_policy_t* policy);

/**
 * @brief Get statistics
 *
 * @param fep FEP system
 * @param stats Output statistics
 * @return 0 on success
 */
int fep_get_stats(const fep_system_t* fep, fep_stats_t* stats);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert update mode to string
 *
 * @param mode Update mode
 * @return Human-readable string
 */
const char* fep_update_mode_to_string(fep_update_mode_t mode);

/**
 * @brief Convert action mode to string
 *
 * @param mode Action mode
 * @return Human-readable string
 */
const char* fep_action_mode_to_string(fep_action_mode_t mode);

/**
 * @brief Convert component to string
 *
 * @param component Free energy component
 * @return Human-readable string
 */
const char* fep_component_to_string(fep_component_t component);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FREE_ENERGY_H */
