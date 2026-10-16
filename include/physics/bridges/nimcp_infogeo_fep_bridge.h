/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_fep_bridge.h - Information Geometry to Free Energy Principle Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_fep_bridge.h
 * @brief Bridge connecting Information Geometry with Free Energy Principle
 *
 * WHAT: Provides deep integration between Information Geometry and the
 *       Free Energy Principle, leveraging their mathematical kinship.
 *
 * WHY:  Information geometry and FEP share fundamental connections:
 *       - Both operate on statistical manifolds of probability distributions
 *       - KL divergence is central to both frameworks
 *       - Natural gradient descent minimizes variational free energy
 *       - Fisher information defines the metric for FEP dynamics
 *
 * HOW:  Unified geometric-energetic framework:
 *       1. Fisher metric provides Riemannian structure for FEP manifold
 *       2. Natural gradient descent = gradient descent on free energy
 *       3. Belief updating follows geodesics on probability manifold
 *       4. Precision weighting derived from Fisher information
 *
 * MATHEMATICAL FOUNDATION:
 * ```
 * INFORMATION GEOMETRY                    FREE ENERGY PRINCIPLE
 * -----------------------------------------------------------------------
 * Fisher Information Matrix G         =   Precision matrix of beliefs
 * Natural Gradient G^(-1) * grad      =   Optimal belief update direction
 * KL Divergence D_KL(q||p)            =   Variational free energy (part)
 * Geodesic Distance                   =   Shortest path in belief space
 * Riemannian Manifold                 =   Statistical manifold of beliefs
 * Ricci Curvature                     =   Complexity of belief landscape
 *
 * Free Energy: F = E_q[log q(s) - log p(o,s)]
 *            = KL(q||p) - log p(o)  (evidence lower bound)
 *
 * Natural Gradient of F: G^(-1) * dF/dmu = optimal belief update
 * ```
 *
 * KEY INSIGHT:
 * Natural gradient descent on variational free energy IS the optimal
 * Bayesian inference algorithm when beliefs form a statistical manifold.
 * This bridge unifies learning (InfoGeo) with inference (FEP).
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_FEP_BRIDGE_H
#define NIMCP_INFOGEO_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_FEP_MODULE_NAME          "infogeo_fep_bridge"

/** Maximum state dimensions for belief manifold */
#define INFOGEO_FEP_MAX_STATE_DIM        128

/** Maximum observation dimensions */
#define INFOGEO_FEP_MAX_OBS_DIM          256

/** Default precision floor (prevents numerical issues) */
#define INFOGEO_FEP_PRECISION_FLOOR      1e-6f

/** Default free energy threshold for convergence */
#define INFOGEO_FEP_FE_THRESHOLD         1e-4f

/** Maximum belief update iterations */
#define INFOGEO_FEP_MAX_ITERATIONS       100

/** Default natural gradient step size */
#define INFOGEO_FEP_STEP_SIZE            0.1f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Belief update method
 */
typedef enum {
    INFOGEO_FEP_UPDATE_NATURAL = 0,     /**< Natural gradient descent */
    INFOGEO_FEP_UPDATE_STANDARD,        /**< Standard gradient descent */
    INFOGEO_FEP_UPDATE_LAPLACE,         /**< Laplace approximation */
    INFOGEO_FEP_UPDATE_VARIATIONAL      /**< Full variational inference */
} infogeo_fep_update_method_t;

/**
 * @brief Precision estimation method
 */
typedef enum {
    INFOGEO_FEP_PREC_FISHER = 0,        /**< From Fisher information */
    INFOGEO_FEP_PREC_HESSIAN,           /**< From Hessian of log-likelihood */
    INFOGEO_FEP_PREC_EMPIRICAL,         /**< Empirical precision */
    INFOGEO_FEP_PREC_FIXED              /**< Fixed precision values */
} infogeo_fep_precision_method_t;

/**
 * @brief Free energy decomposition type
 */
typedef enum {
    INFOGEO_FEP_DECOMP_STANDARD = 0,    /**< F = E_q[-log p(o|s)] + KL(q||p) */
    INFOGEO_FEP_DECOMP_COMPLEXITY,      /**< F = Accuracy - Complexity */
    INFOGEO_FEP_DECOMP_SURPRISE         /**< F = Surprise + KL divergence */
} infogeo_fep_decomposition_t;

/**
 * @brief Active inference action selection
 */
typedef enum {
    INFOGEO_FEP_ACTION_EFE = 0,         /**< Expected free energy */
    INFOGEO_FEP_ACTION_PRAGMATIC,       /**< Pragmatic (goal-directed) only */
    INFOGEO_FEP_ACTION_EPISTEMIC,       /**< Epistemic (info-seeking) only */
    INFOGEO_FEP_ACTION_GEODESIC         /**< Geodesic path to goal state */
} infogeo_fep_action_method_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Information Geometry-FEP bridge
 */
typedef struct {
    /** Belief update settings */
    infogeo_fep_update_method_t update_method;   /**< Belief update method */
    float step_size;                              /**< Natural gradient step */
    uint32_t max_iterations;                      /**< Max update iterations */
    float convergence_threshold;                  /**< Free energy threshold */
    bool enable_line_search;                      /**< Adaptive step size */

    /** Precision settings */
    infogeo_fep_precision_method_t precision_method; /**< How to get precision */
    float precision_floor;                        /**< Minimum precision value */
    float precision_ceiling;                      /**< Maximum precision value */
    bool enable_precision_learning;               /**< Learn precision online */
    float precision_learning_rate;                /**< Precision update rate */

    /** Free energy settings */
    infogeo_fep_decomposition_t decomposition;   /**< FE decomposition type */
    bool track_components;                        /**< Track FE components */
    bool enable_complexity_penalty;               /**< Penalize model complexity */
    float complexity_weight;                      /**< Weight for complexity */

    /** Active inference settings */
    infogeo_fep_action_method_t action_method;   /**< Action selection method */
    float epistemic_weight;                       /**< Weight for epistemic value */
    float pragmatic_weight;                       /**< Weight for pragmatic value */
    bool use_geodesic_planning;                   /**< Plan along geodesics */
    uint32_t planning_horizon;                    /**< Temporal planning depth */

    /** Manifold settings */
    uint32_t state_dim;                          /**< State space dimension */
    uint32_t obs_dim;                            /**< Observation dimension */
    bool compute_curvature;                       /**< Compute belief curvature */
    bool enable_geodesic_integration;             /**< Geodesic belief updates */

    /** General settings */
    float update_interval_ms;                    /**< Bridge update interval */
    bool enable_logging;                          /**< Enable logging */
} infogeo_fep_config_t;

/**
 * @brief Belief state on statistical manifold
 */
typedef struct {
    float* mean;                        /**< Belief mean (sufficient statistic) */
    float* precision;                   /**< Precision matrix (from Fisher) */
    uint32_t dim;                       /**< State dimensionality */
    float entropy;                      /**< Belief entropy */
    float log_evidence;                 /**< Log model evidence estimate */
} infogeo_fep_belief_t;

/**
 * @brief Free energy and its components
 */
typedef struct {
    float total_free_energy;            /**< Total variational free energy */
    float accuracy;                     /**< -E_q[log p(o|s)] (expected log-lik) */
    float complexity;                   /**< KL(q||p) divergence from prior */
    float surprise;                     /**< -log p(o) (self-information) */
    float kl_divergence;                /**< KL(q||posterior) approximation error */
    float elbo;                         /**< Evidence lower bound */
    float dFdt;                         /**< Rate of free energy change */
} infogeo_fep_free_energy_t;

/**
 * @brief Natural gradient belief update
 */
typedef struct {
    float* natural_gradient;            /**< Natural gradient direction */
    float* update;                      /**< Actual belief update applied */
    uint32_t dim;                       /**< Update dimensionality */
    float gradient_norm;                /**< Standard gradient norm */
    float natural_norm;                 /**< Natural gradient norm */
    float step_size_used;               /**< Actual step size (after line search) */
    float fe_before;                    /**< Free energy before update */
    float fe_after;                     /**< Free energy after update */
    uint32_t iterations;                /**< Iterations to converge */
    bool converged;                     /**< Whether update converged */
} infogeo_fep_update_t;

/**
 * @brief Expected free energy for action selection
 */
typedef struct {
    float expected_free_energy;         /**< Total EFE for action */
    float pragmatic_value;              /**< Goal-directed value */
    float epistemic_value;              /**< Information gain value */
    float geodesic_cost;                /**< Cost of geodesic to goal */
    uint32_t action_id;                 /**< Action identifier */
    float* predicted_state;             /**< Predicted state after action */
    uint32_t state_dim;                 /**< State dimensionality */
} infogeo_fep_efe_t;

/**
 * @brief Precision weighted prediction error
 */
typedef struct {
    float* prediction_error;            /**< Observation - prediction */
    float* precision_weighted_error;    /**< Precision * error */
    uint32_t obs_dim;                   /**< Observation dimensionality */
    float total_precision;              /**< Sum of precisions */
    float weighted_error_norm;          /**< Norm of weighted error */
} infogeo_fep_prediction_error_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t belief_updates;            /**< Total belief updates */
    uint64_t natural_grad_updates;      /**< Natural gradient updates */
    uint64_t fisher_computations;       /**< Fisher matrix computations */
    uint64_t action_selections;         /**< Action selections */
    float avg_free_energy;              /**< Average free energy */
    float avg_kl_divergence;            /**< Average KL divergence */
    float avg_convergence_iters;        /**< Average iterations to converge */
    float total_fe_reduction;           /**< Total free energy reduced */
    float last_update_ms;               /**< Last update timestamp */
} infogeo_fep_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_fep_bridge_struct infogeo_fep_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_default_config(infogeo_fep_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-FEP bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_fep_bridge_t* infogeo_fep_bridge_create(
    const infogeo_fep_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_fep_bridge_destroy(infogeo_fep_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_reset(infogeo_fep_bridge_t* bridge);

//=============================================================================
// Belief Initialization API
//=============================================================================

/**
 * @brief Initialize belief state
 *
 * WHAT: Sets initial belief distribution on manifold
 * WHY:  Starting point for free energy minimization
 * HOW:  Initializes mean and precision (from Fisher or prior)
 *
 * @param bridge Bridge handle
 * @param prior_mean Prior belief mean
 * @param prior_precision Prior precision (NULL for Fisher-based)
 * @param dim State dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_init_belief(
    infogeo_fep_bridge_t* bridge,
    const float* prior_mean,
    const float* prior_precision,
    uint32_t dim
);

/**
 * @brief Get current belief state
 *
 * @param bridge Bridge handle
 * @param belief Output belief state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_get_belief(
    const infogeo_fep_bridge_t* bridge,
    infogeo_fep_belief_t* belief
);

//=============================================================================
// Free Energy API
//=============================================================================

/**
 * @brief Compute variational free energy
 *
 * WHAT: Computes free energy of current belief given observation
 * WHY:  Free energy is the objective function for inference
 * HOW:  F = -E_q[log p(o|s)] + KL(q||prior)
 *
 * @param bridge Bridge handle
 * @param observation Current observation
 * @param obs_dim Observation dimensionality
 * @param free_energy Output free energy components
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_compute_free_energy(
    infogeo_fep_bridge_t* bridge,
    const float* observation,
    uint32_t obs_dim,
    infogeo_fep_free_energy_t* free_energy
);

/**
 * @brief Compute precision-weighted prediction error
 *
 * WHAT: Computes prediction error weighted by precision (Fisher)
 * WHY:  Precision weighting is core to FEP inference
 * HOW:  error = precision * (observation - prediction)
 *
 * @param bridge Bridge handle
 * @param observation Actual observation
 * @param prediction Predicted observation
 * @param obs_dim Observation dimensionality
 * @param error Output prediction error
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_prediction_error(
    infogeo_fep_bridge_t* bridge,
    const float* observation,
    const float* prediction,
    uint32_t obs_dim,
    infogeo_fep_prediction_error_t* error
);

//=============================================================================
// Natural Gradient Belief Update API
//=============================================================================

/**
 * @brief Update belief using natural gradient descent
 *
 * WHAT: Updates belief state via natural gradient on free energy
 * WHY:  Natural gradient is optimal for probability distributions
 * HOW:  belief += step_size * Fisher^(-1) * grad_F
 *
 * @param bridge Bridge handle
 * @param observation Current observation
 * @param obs_dim Observation dimensionality
 * @param update Output update details
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_natural_gradient_update(
    infogeo_fep_bridge_t* bridge,
    const float* observation,
    uint32_t obs_dim,
    infogeo_fep_update_t* update
);

/**
 * @brief Perform full inference (iterate until convergence)
 *
 * WHAT: Iterates natural gradient updates until free energy converges
 * WHY:  Complete inference given observation
 * HOW:  Repeat natural gradient until FE change < threshold
 *
 * @param bridge Bridge handle
 * @param observation Current observation
 * @param obs_dim Observation dimensionality
 * @param final_update Output final update state
 * @return Number of iterations, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_infer(
    infogeo_fep_bridge_t* bridge,
    const float* observation,
    uint32_t obs_dim,
    infogeo_fep_update_t* final_update
);

//=============================================================================
// Precision API
//=============================================================================

/**
 * @brief Update precision from Fisher information
 *
 * WHAT: Computes belief precision from Fisher information matrix
 * WHY:  Fisher information = inverse covariance = precision
 * HOW:  Extracts precision from information geometry module
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_update_precision_from_fisher(
    infogeo_fep_bridge_t* bridge
);

/**
 * @brief Set precision directly
 *
 * @param bridge Bridge handle
 * @param precision Precision matrix (diagonal or full)
 * @param dim Dimension
 * @param is_diagonal Whether precision is diagonal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_set_precision(
    infogeo_fep_bridge_t* bridge,
    const float* precision,
    uint32_t dim,
    bool is_diagonal
);

/**
 * @brief Learn precision from prediction errors
 *
 * WHAT: Updates precision based on accumulated prediction errors
 * WHY:  Adaptive precision weighting improves inference
 * HOW:  precision += lr * (error * error - 1/precision)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_learn_precision(infogeo_fep_bridge_t* bridge);

//=============================================================================
// Active Inference API
//=============================================================================

/**
 * @brief Compute expected free energy for action
 *
 * WHAT: Evaluates expected free energy if action is taken
 * WHY:  Action selection in active inference
 * HOW:  EFE = pragmatic (goal) + epistemic (info gain)
 *
 * @param bridge Bridge handle
 * @param action Action to evaluate
 * @param action_dim Action dimensionality
 * @param goal_state Desired goal state
 * @param state_dim State dimensionality
 * @param efe Output expected free energy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_compute_efe(
    infogeo_fep_bridge_t* bridge,
    const float* action,
    uint32_t action_dim,
    const float* goal_state,
    uint32_t state_dim,
    infogeo_fep_efe_t* efe
);

/**
 * @brief Select action minimizing expected free energy
 *
 * WHAT: Chooses action with lowest expected free energy
 * WHY:  Active inference action selection
 * HOW:  Evaluates EFE for all actions, selects minimum
 *
 * @param bridge Bridge handle
 * @param actions Array of candidate actions
 * @param num_actions Number of actions
 * @param action_dim Action dimensionality
 * @param goal_state Desired goal state
 * @param state_dim State dimensionality
 * @param selected_action Output selected action index
 * @param selected_efe Output EFE of selected action
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_select_action(
    infogeo_fep_bridge_t* bridge,
    const float* actions,
    uint32_t num_actions,
    uint32_t action_dim,
    const float* goal_state,
    uint32_t state_dim,
    uint32_t* selected_action,
    infogeo_fep_efe_t* selected_efe
);

/**
 * @brief Compute geodesic path to goal state
 *
 * WHAT: Finds geodesic on belief manifold toward goal
 * WHY:  Geodesic is shortest path in probability space
 * HOW:  Solves geodesic equation on Fisher manifold
 *
 * @param bridge Bridge handle
 * @param goal_state Target state
 * @param state_dim State dimensionality
 * @param path Output geodesic path (array of states)
 * @param num_steps Number of steps in path
 * @return Geodesic length, -1 on error
 */
NIMCP_EXPORT float infogeo_fep_geodesic_to_goal(
    infogeo_fep_bridge_t* bridge,
    const float* goal_state,
    uint32_t state_dim,
    float* path,
    uint32_t num_steps
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Maintains running averages, updates precision
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_update(
    infogeo_fep_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_fep_get_stats(
    const infogeo_fep_bridge_t* bridge,
    infogeo_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy, or INFINITY on error
 */
NIMCP_EXPORT float infogeo_fep_get_free_energy(
    const infogeo_fep_bridge_t* bridge
);

/**
 * @brief Get current KL divergence from prior
 *
 * @param bridge Bridge handle
 * @return KL divergence, or -1.0 on error
 */
NIMCP_EXPORT float infogeo_fep_get_kl_from_prior(
    const infogeo_fep_bridge_t* bridge
);

/**
 * @brief Check if beliefs have converged
 *
 * @param bridge Bridge handle
 * @return true if free energy is stable
 */
NIMCP_EXPORT bool infogeo_fep_is_converged(
    const infogeo_fep_bridge_t* bridge
);

/**
 * @brief Get belief entropy
 *
 * @param bridge Bridge handle
 * @return Belief entropy, or -1.0 on error
 */
NIMCP_EXPORT float infogeo_fep_get_entropy(
    const infogeo_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_FEP_BRIDGE_H */