/**
 * @file nimcp_surface_optimization.h
 * @brief Surface Optimization Algorithms
 *
 * WHAT: Algorithms for finding minimal-surface network configurations
 * WHY:  Given terminal positions and circumference constraints,
 *       find the surface-minimizing branching geometry
 * HOW:  Multiple methods:
 *       - Gradient descent on surface area
 *       - Monte Carlo integration for area estimation
 *       - QMCTS for topology search (discrete)
 *       - Quantum annealing for escaping local minima
 *
 * RETURN VALUE CONVENTIONS:
 * - All int-returning functions: return 0 on success, -1 on error
 * - Float-returning functions (e.g., surface_optimizer_get_area):
 *   return -1.0f on error (invalid optimizer), otherwise valid float value
 * - Pointer-returning functions: return NULL on error
 * - Bool-returning functions: return false on error or invalid input
 *
 * This follows the FEP bridges convention (0 success, -1 error) rather than
 * the global NIMCP_OK/NIMCP_ERROR_* codes, for consistency with other
 * geometry and optimization modules.
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_OPTIMIZATION_H
#define NIMCP_SURFACE_OPTIMIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/geometry/nimcp_surface_manifold.h"

//=============================================================================
// OPTIMIZATION CONFIGURATION
//=============================================================================

/**
 * @brief Gradient descent optimizer configuration
 */
typedef struct surface_gradient_config_struct {
    float learning_rate;            /**< Step size (default: 0.01) */
    float momentum;                 /**< Momentum coefficient (default: 0.9) */
    float decay;                    /**< Learning rate decay (default: 0.999) */
    uint32_t max_iterations;        /**< Max iterations (default: 1000) */
    float tolerance;                /**< Convergence tolerance (default: 1e-6) */
    bool use_adam;                  /**< Use Adam optimizer (default: true) */
    float beta1, beta2;             /**< Adam parameters */
    float epsilon;                  /**< Adam epsilon */
} surface_gradient_config_t;

/**
 * @brief Monte Carlo optimizer configuration
 */
typedef struct surface_monte_carlo_config_struct {
    uint32_t num_samples;           /**< Number of samples (default: 10000) */
    bool use_importance_sampling;   /**< Importance sampling (default: true) */
    bool use_stratified;            /**< Stratified sampling (default: false) */
    uint32_t strata_per_dim;        /**< Strata per dimension (default: 10) */
    uint64_t seed;                  /**< Random seed */
} surface_monte_carlo_config_t;

/**
 * @brief Quantum annealing optimizer configuration
 */
typedef struct surface_annealing_config_struct {
    float temperature_initial;      /**< Initial temperature (default: 10.0) */
    float temperature_final;        /**< Final temperature (default: 0.01) */
    float cooling_rate;             /**< Cooling rate (default: 0.95) */
    float quantum_strength;         /**< Quantum tunneling strength (default: 1.0) */
    uint32_t steps_per_temperature; /**< Steps per temp (default: 100) */
    uint32_t max_iterations;        /**< Max total iterations */
    float acceptance_target;        /**< Target acceptance rate (default: 0.234) */
} surface_annealing_config_t;

/**
 * @brief MCTS optimizer configuration for topology search
 */
typedef struct surface_mcts_config_struct {
    uint32_t num_iterations;        /**< MCTS iterations (default: 1000) */
    uint32_t max_depth;             /**< Max tree depth (default: 20) */
    float exploration_constant;     /**< UCB exploration (default: sqrt(2)) */
    uint32_t rollout_count;         /**< Rollouts per node (default: 10) */
    bool use_virtual_loss;          /**< Virtual loss for parallelization */
} surface_mcts_config_t;

//=============================================================================
// OPTIMIZATION STATE
//=============================================================================

/**
 * @brief Optimizer state for gradient descent
 */
typedef struct surface_gradient_state_struct {
    /* Current solution */
    surface_branch_point_t* branch_points;
    uint32_t num_branch_points;

    /* Gradients */
    float* position_gradients;      /**< d(area)/d(position) */
    float* diameter_gradients;      /**< d(area)/d(diameter) */

    /* Momentum (for momentum/Adam) */
    float* velocity_pos;
    float* velocity_diam;

    /* Adam state */
    float* m_pos, *v_pos;           /**< First/second moment estimates */
    float* m_diam, *v_diam;

    /* Iteration */
    uint32_t iteration;
    float current_area;
    float previous_area;
    bool converged;
} surface_gradient_state_t;

/**
 * @brief Optimizer state for Monte Carlo
 */
typedef struct surface_mc_state_struct {
    /* Sample statistics */
    float area_estimate;
    float area_variance;
    float area_std_error;
    uint32_t samples_taken;
    float effective_sample_size;

    /* RNG state */
    uint64_t rng_state;
} surface_mc_state_t;

/**
 * @brief Optimizer state for annealing
 */
typedef struct surface_annealing_state_struct {
    /* Current solution */
    surface_branch_point_t* current;
    uint32_t num_points;
    float current_energy;

    /* Best solution found */
    surface_branch_point_t* best;
    float best_energy;

    /* Temperature */
    float temperature;
    uint32_t step;
    uint32_t total_steps;

    /* Statistics */
    uint32_t accepted;
    uint32_t rejected;
    uint32_t tunneling_events;
    float acceptance_rate;
} surface_annealing_state_t;

//=============================================================================
// OPTIMIZER INTERFACE
//=============================================================================

/**
 * @brief Generic optimizer handle
 */
typedef struct surface_optimizer_struct surface_optimizer_t;

/**
 * @brief Create optimizer
 *
 * @param method Optimization method
 * @param config Method-specific configuration
 * @return Created optimizer or NULL on failure
 */
surface_optimizer_t* surface_optimizer_create(
    surface_optimization_method_t method,
    const void* config
);

/**
 * @brief Destroy optimizer
 *
 * @param optimizer Optimizer to destroy
 */
void surface_optimizer_destroy(surface_optimizer_t* optimizer);

/**
 * @brief Initialize optimization from terminals
 *
 * @param optimizer Optimizer
 * @param terminals Terminal positions [n][3]
 * @param num_terminals Number of terminals
 * @param min_circumference Minimum link circumference
 * @return 0 on success, -1 on error
 */
int surface_optimizer_init(
    surface_optimizer_t* optimizer,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference
);

/**
 * @brief Perform single optimization step
 *
 * @param optimizer Optimizer
 * @param improved Output: true if solution improved
 * @return 0 on success, -1 on error
 */
int surface_optimizer_step(
    surface_optimizer_t* optimizer,
    bool* improved
);

/**
 * @brief Run optimization until convergence or max iterations
 *
 * @param optimizer Optimizer
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int surface_optimizer_run(
    surface_optimizer_t* optimizer,
    surface_optimization_result_t* result
);

/**
 * @brief Check if optimizer has converged
 *
 * @param optimizer Optimizer
 * @return true if converged
 */
bool surface_optimizer_converged(const surface_optimizer_t* optimizer);

/**
 * @brief Get current solution
 *
 * @param optimizer Optimizer
 * @param branch_points Output: current branch points
 * @param max_points Maximum points to return
 * @param num_points Output: actual number of points
 * @return 0 on success, -1 on error
 */
int surface_optimizer_get_solution(
    const surface_optimizer_t* optimizer,
    surface_branch_point_t* branch_points,
    uint32_t max_points,
    uint32_t* num_points
);

/**
 * @brief Get current surface area
 *
 * @param optimizer Optimizer
 * @return Current surface area (or -1 on error)
 */
float surface_optimizer_get_area(const surface_optimizer_t* optimizer);

//=============================================================================
// GRADIENT DESCENT OPTIMIZATION
//=============================================================================

/**
 * @brief Initialize gradient descent configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_gradient_default_config(surface_gradient_config_t* config);

/**
 * @brief Compute gradient of surface area
 *
 * @param branch_points Current branch points
 * @param num_points Number of points
 * @param min_circumference Minimum circumference
 * @param position_gradients Output: position gradients [n*3]
 * @param diameter_gradients Output: diameter gradients [n*k]
 * @return 0 on success, -1 on error
 */
int surface_compute_area_gradient(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    float* position_gradients,
    float* diameter_gradients
);

//=============================================================================
// MONTE CARLO OPTIMIZATION
//=============================================================================

/**
 * @brief Initialize Monte Carlo configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_mc_default_config(surface_monte_carlo_config_t* config);

/**
 * @brief Estimate surface area via Monte Carlo integration
 *
 * @param branch_points Branch points
 * @param num_points Number of points
 * @param min_circumference Minimum circumference
 * @param config MC configuration
 * @param area_estimate Output: estimated area
 * @param variance Output: variance estimate
 * @return 0 on success, -1 on error
 */
int surface_mc_estimate_area(
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    const surface_monte_carlo_config_t* config,
    float* area_estimate,
    float* variance
);

//=============================================================================
// QUANTUM ANNEALING OPTIMIZATION
//=============================================================================

/**
 * @brief Initialize annealing configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_annealing_default_config(surface_annealing_config_t* config);

/**
 * @brief Create annealing state
 *
 * @param config Annealing configuration
 * @return Created state or NULL on failure
 */
surface_annealing_state_t* surface_annealing_state_create(
    const surface_annealing_config_t* config
);

/**
 * @brief Destroy annealing state
 *
 * @param state State to destroy
 */
void surface_annealing_state_destroy(surface_annealing_state_t* state);

/**
 * @brief Perform single annealing step
 *
 * @param state Annealing state
 * @param config Configuration
 * @param accepted Output: true if move accepted
 * @return 0 on success, -1 on error
 */
int surface_annealing_step(
    surface_annealing_state_t* state,
    const surface_annealing_config_t* config,
    bool* accepted
);

//=============================================================================
// MCTS TOPOLOGY OPTIMIZATION
//=============================================================================

/**
 * @brief Initialize MCTS configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_mcts_default_config(surface_mcts_config_t* config);

/**
 * @brief MCTS action: Add branch point at position
 */
typedef struct surface_mcts_action_struct {
    uint32_t action_type;           /**< 0=add bifurcation, 1=add trifurcation */
    float position[3];              /**< Position for new point */
    uint32_t connect_to[4];         /**< IDs to connect to */
    uint32_t num_connections;
} surface_mcts_action_t;

/**
 * @brief MCTS state for topology search
 */
typedef struct surface_mcts_state_struct {
    surface_branch_point_t* branch_points;
    uint32_t num_branch_points;
    uint32_t* terminal_ids;         /**< Which points are terminals */
    uint32_t num_terminals;
    float current_area;
    bool is_complete;               /**< All terminals connected */
} surface_mcts_state_t;

/**
 * @brief Get available actions from MCTS state
 *
 * @param state Current state
 * @param actions Output: available actions
 * @param max_actions Maximum actions to return
 * @param num_actions Output: actual number of actions
 * @return 0 on success, -1 on error
 */
int surface_mcts_get_actions(
    const surface_mcts_state_t* state,
    surface_mcts_action_t* actions,
    uint32_t max_actions,
    uint32_t* num_actions
);

/**
 * @brief Apply action to MCTS state
 *
 * @param state State to modify
 * @param action Action to apply
 * @return 0 on success, -1 on error
 */
int surface_mcts_apply_action(
    surface_mcts_state_t* state,
    const surface_mcts_action_t* action
);

/**
 * @brief Evaluate MCTS state (negative surface area)
 *
 * @param state State to evaluate
 * @return Evaluation score (higher is better)
 */
float surface_mcts_evaluate(const surface_mcts_state_t* state);

//=============================================================================
// TETRAHEDRAL OPTIMIZATION (Special Case)
//=============================================================================

/**
 * @brief Optimize tetrahedral configuration
 *
 * Paper's main example: 4 terminals at tetrahedron corners.
 * Demonstrates transition from 2 bifurcations to 1 trifurcation
 * as chi increases past 0.83.
 *
 * @param terminals 4 terminal positions
 * @param min_circumference Minimum link circumference
 * @param method Optimization method
 * @param method_config Method-specific configuration
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int surface_optimize_tetrahedron_internal(
    const float terminals[4][3],
    float min_circumference,
    surface_optimization_method_t method,
    const void* method_config,
    surface_optimization_result_t* result
);

/**
 * @brief Compute chi for tetrahedral configuration
 *
 * chi = w/r where r is tetrahedron "radius" (center to vertex)
 *
 * @param terminals 4 terminal positions
 * @param circumference Link circumference w
 * @param chi Output: chi value
 * @return 0 on success, -1 on error
 */
int surface_tetrahedron_chi(
    const float terminals[4][3],
    float circumference,
    float* chi
);

/**
 * @brief Compute lambda (separation) for tetrahedral solution
 *
 * lambda = l/w where l is distance between intermediate nodes
 *
 * @param solution Optimized branch points
 * @param num_points Number of points (should be 6: 4 terminals + 2 intermediate)
 * @param circumference Link circumference
 * @param lambda Output: lambda value
 * @return 0 on success, -1 on error
 */
int surface_tetrahedron_lambda(
    const surface_branch_point_t* solution,
    uint32_t num_points,
    float circumference,
    float* lambda
);

//=============================================================================
// INITIALIZATION HELPERS
//=============================================================================

/**
 * @brief Create initial topology from terminals (Steiner-like)
 *
 * @param terminals Terminal positions
 * @param num_terminals Number of terminals
 * @param branch_points Output: initial branch points
 * @param max_points Maximum points
 * @param num_points Output: actual number of points
 * @return 0 on success, -1 on error
 */
int surface_create_initial_topology(
    const float (*terminals)[3],
    uint32_t num_terminals,
    surface_branch_point_t* branch_points,
    uint32_t max_points,
    uint32_t* num_points
);

/**
 * @brief Compute centroid of terminal positions
 *
 * @param terminals Terminal positions
 * @param num_terminals Number of terminals
 * @param centroid Output: centroid position
 * @return 0 on success, -1 on error
 */
int surface_compute_centroid(
    const float (*terminals)[3],
    uint32_t num_terminals,
    float centroid[3]
);

/**
 * @brief Compute characteristic distance for terminal set
 *
 * r = average distance from centroid to terminals
 *
 * @param terminals Terminal positions
 * @param num_terminals Number of terminals
 * @param distance Output: characteristic distance
 * @return 0 on success, -1 on error
 */
int surface_compute_characteristic_distance(
    const float (*terminals)[3],
    uint32_t num_terminals,
    float* distance
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_OPTIMIZATION_H */
