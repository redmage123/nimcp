/**
 * @file nimcp_surface_quantum_bridge.h
 * @brief Surface Geometry Quantum Integration Bridge
 *
 * WHAT: Quantum-enhanced optimization for surface geometry
 * WHY:  Uses QMC for surface area estimation and QMCTS for topology search
 * HOW:  Integrates NIMCP quantum modules with surface optimization
 *
 * QUANTUM METHODS:
 * - QMC Amplitude Estimation: Accurate surface area computation
 * - Quantum Annealing: Escape local minima in parameter space
 * - QMCTS: Discrete topology search for branch placement
 * - Importance Sampling: Focus on high-impact configurations
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_QUANTUM_BRIDGE_H
#define NIMCP_SURFACE_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/geometry/nimcp_surface_geometry_types.h"

//=============================================================================
// MODULE IDENTIFIER
//=============================================================================

#define BIO_MODULE_SURFACE_QUANTUM      0x1420

//=============================================================================
// QUANTUM METHOD SELECTION
//=============================================================================

/**
 * @brief Quantum methods available for surface optimization
 */
typedef enum surface_quantum_method_enum {
    SURFACE_QUANTUM_NONE = 0,               /**< No quantum enhancement */
    SURFACE_QUANTUM_QMC_AMPLITUDE,          /**< QMC amplitude estimation */
    SURFACE_QUANTUM_QMC_ANNEAL,             /**< Quantum annealing */
    SURFACE_QUANTUM_MCTS,                   /**< Quantum MCTS for topology */
    SURFACE_QUANTUM_IMPORTANCE_SAMPLING,    /**< Quantum importance sampling */
    SURFACE_QUANTUM_HYBRID                  /**< Combined methods */
} surface_quantum_method_t;

//=============================================================================
// QMC CONFIGURATION
//=============================================================================

/**
 * @brief QMC amplitude estimation configuration
 */
typedef struct surface_qmc_amplitude_config_struct {
    uint32_t num_qubits;                    /**< Number of qubits */
    uint32_t num_shots;                     /**< Measurement shots */
    float precision_target;                 /**< Target precision */
    uint32_t max_iterations;                /**< Max iterations */
    float confidence_level;                 /**< Confidence level (default: 0.95) */
} surface_qmc_amplitude_config_t;

/**
 * @brief QMC annealing configuration
 */
typedef struct surface_qmc_anneal_config_struct {
    float transverse_field_initial;         /**< Initial transverse field */
    float transverse_field_final;           /**< Final transverse field */
    uint32_t num_sweeps;                    /**< Number of sweeps */
    float beta;                             /**< Inverse temperature */
    uint32_t trotter_slices;                /**< Trotter slices */
} surface_qmc_anneal_config_t;

/**
 * @brief QMCTS configuration
 */
typedef struct surface_qmcts_config_struct {
    uint32_t num_iterations;                /**< MCTS iterations */
    uint32_t max_depth;                     /**< Max tree depth */
    float exploration_constant;             /**< UCB exploration constant */
    uint32_t rollout_count;                 /**< Rollouts per node */
    bool use_quantum_rollout;               /**< Use quantum for rollouts */
    float quantum_enhancement_factor;       /**< Quantum boost factor */
} surface_qmcts_config_t;

//=============================================================================
// QUANTUM RESULTS
//=============================================================================

/**
 * @brief QMC amplitude estimation result
 */
typedef struct surface_qmc_amplitude_result_struct {
    float estimated_area;                   /**< Estimated surface area */
    float uncertainty;                      /**< Estimation uncertainty */
    float confidence;                       /**< Confidence level achieved */
    uint32_t iterations_used;               /**< Iterations performed */
    uint32_t measurements;                  /**< Total measurements */
    float quantum_advantage;                /**< Speedup vs classical */
} surface_qmc_amplitude_result_t;

/**
 * @brief QMCTS result
 */
typedef struct surface_qmcts_result_struct {
    surface_branch_point_t* optimal_topology;   /**< Best topology found */
    uint32_t num_branch_points;             /**< Number of points */
    float best_score;                       /**< Best score achieved */
    uint32_t nodes_explored;                /**< MCTS nodes explored */
    uint32_t rollouts_performed;            /**< Total rollouts */
    float avg_depth;                        /**< Average tree depth */
} surface_qmcts_result_t;

//=============================================================================
// BRIDGE CONFIGURATION
//=============================================================================

/**
 * @brief Quantum bridge configuration
 */
typedef struct surface_quantum_bridge_config_struct {
    /* Method selection */
    surface_quantum_method_t default_method;
    bool enable_hybrid;                     /**< Allow combining methods */

    /* QMC settings */
    surface_qmc_amplitude_config_t qmc_amplitude;
    surface_qmc_anneal_config_t qmc_anneal;
    surface_qmcts_config_t qmcts;

    /* Performance */
    float classical_fallback_threshold;     /**< When to fall back to classical */
    uint32_t timeout_ms;                    /**< Quantum operation timeout */
    bool enable_caching;                    /**< Cache quantum results */
} surface_quantum_bridge_config_t;

//=============================================================================
// BRIDGE STATISTICS
//=============================================================================

/**
 * @brief Quantum bridge statistics
 */
typedef struct surface_quantum_bridge_stats_struct {
    /* Usage counts */
    uint64_t qmc_amplitude_calls;
    uint64_t qmc_anneal_calls;
    uint64_t qmcts_calls;
    uint64_t classical_fallbacks;

    /* Performance */
    float avg_quantum_time_ms;
    float avg_classical_time_ms;
    float total_quantum_advantage;

    /* Accuracy */
    float avg_amplitude_uncertainty;
    float best_mcts_score;
} surface_quantum_bridge_stats_t;

//=============================================================================
// BRIDGE STRUCTURE
//=============================================================================

/**
 * @brief Surface geometry quantum integration bridge
 */
typedef struct surface_quantum_bridge_struct {
    /* Base bridge (MUST be first) */
    bridge_base_t base;

    /* Connected systems */
    void* geometry_ctx;                     /**< Surface geometry context */
    void* qmc_state;                        /**< QMC state (if available) */

    /* Configuration */
    surface_quantum_bridge_config_t config;

    /* Statistics */
    surface_quantum_bridge_stats_t stats;

    /* State */
    bool quantum_available;                 /**< Quantum backend available */
    bool simulation_mode;                   /**< Simulated quantum */
} surface_quantum_bridge_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Initialize configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_quantum_bridge_default_config(surface_quantum_bridge_config_t* config);

/**
 * @brief Create quantum bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Created bridge or NULL on failure
 */
surface_quantum_bridge_t* surface_quantum_bridge_create(
    const surface_quantum_bridge_config_t* config
);

/**
 * @brief Destroy quantum bridge
 *
 * @param bridge Bridge to destroy
 */
void surface_quantum_bridge_destroy(surface_quantum_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_quantum_bridge_reset(surface_quantum_bridge_t* bridge);

//=============================================================================
// CONNECTION
//=============================================================================

/**
 * @brief Connect to surface geometry context
 *
 * @param bridge Bridge
 * @param ctx Geometry context
 * @return 0 on success, -1 on error
 */
int surface_quantum_bridge_connect_geometry(
    surface_quantum_bridge_t* bridge,
    void* ctx
);

/**
 * @brief Connect to QMC state
 *
 * @param bridge Bridge
 * @param qmc_state QMC state
 * @return 0 on success, -1 on error
 */
int surface_quantum_bridge_connect_qmc(
    surface_quantum_bridge_t* bridge,
    void* qmc_state
);

/**
 * @brief Check if quantum is available
 *
 * @param bridge Bridge
 * @return true if quantum methods available
 */
bool surface_quantum_bridge_is_quantum_available(
    const surface_quantum_bridge_t* bridge
);

//=============================================================================
// QMC AMPLITUDE ESTIMATION
//=============================================================================

/**
 * @brief Estimate surface area using QMC amplitude estimation
 *
 * Uses quantum amplitude estimation to compute surface area
 * with potential quadratic speedup over classical Monte Carlo.
 *
 * @param bridge Bridge
 * @param branch_points Branch points
 * @param num_points Number of points
 * @param min_circumference Minimum circumference
 * @param result Output: estimation result
 * @return 0 on success, -1 on error
 */
int surface_quantum_estimate_area(
    surface_quantum_bridge_t* bridge,
    const surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    surface_qmc_amplitude_result_t* result
);

//=============================================================================
// QUANTUM ANNEALING
//=============================================================================

/**
 * @brief Optimize parameters using quantum annealing
 *
 * Uses quantum annealing to escape local minima in the
 * continuous parameter space.
 *
 * @param bridge Bridge
 * @param initial_params Initial parameters
 * @param optimized_params Output: optimized parameters
 * @return 0 on success, -1 on error
 */
int surface_quantum_anneal_params(
    surface_quantum_bridge_t* bridge,
    const surface_geometry_params_t* initial_params,
    surface_geometry_params_t* optimized_params
);

/**
 * @brief Optimize branch positions using quantum annealing
 *
 * @param bridge Bridge
 * @param branch_points Branch points (modified in place)
 * @param num_points Number of points
 * @param min_circumference Minimum circumference
 * @param final_area Output: final surface area
 * @return 0 on success, -1 on error
 */
int surface_quantum_anneal_positions(
    surface_quantum_bridge_t* bridge,
    surface_branch_point_t* branch_points,
    uint32_t num_points,
    float min_circumference,
    float* final_area
);

//=============================================================================
// QUANTUM MCTS
//=============================================================================

/**
 * @brief Search for optimal topology using QMCTS
 *
 * Uses quantum-enhanced MCTS to search the discrete space
 * of possible branch topologies.
 *
 * @param bridge Bridge
 * @param terminals Terminal positions [n][3]
 * @param num_terminals Number of terminals
 * @param min_circumference Minimum circumference
 * @param result Output: QMCTS result
 * @return 0 on success, -1 on error
 */
int surface_quantum_mcts_optimize(
    surface_quantum_bridge_t* bridge,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_qmcts_result_t* result
);

/**
 * @brief Free QMCTS result resources
 *
 * @param result Result to free
 */
void surface_qmcts_result_free(surface_qmcts_result_t* result);

//=============================================================================
// HYBRID OPTIMIZATION
//=============================================================================

/**
 * @brief Full hybrid quantum-classical optimization
 *
 * Combines multiple quantum methods:
 * 1. QMCTS for topology search
 * 2. Quantum annealing for position refinement
 * 3. QMC amplitude estimation for accurate area
 *
 * @param bridge Bridge
 * @param terminals Terminal positions
 * @param num_terminals Number of terminals
 * @param min_circumference Minimum circumference
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int surface_quantum_hybrid_optimize(
    surface_quantum_bridge_t* bridge,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_optimization_result_t* result
);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int surface_quantum_bridge_get_stats(
    const surface_quantum_bridge_t* bridge,
    surface_quantum_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_quantum_bridge_reset_stats(surface_quantum_bridge_t* bridge);

//=============================================================================
// UTILITY
//=============================================================================

/**
 * @brief Get quantum method name
 *
 * @param method Method
 * @return Human-readable name
 */
const char* surface_quantum_method_name(surface_quantum_method_t method);

/**
 * @brief Check if method is available
 *
 * @param bridge Bridge
 * @param method Method to check
 * @return true if available
 */
bool surface_quantum_method_available(
    const surface_quantum_bridge_t* bridge,
    surface_quantum_method_t method
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_QUANTUM_BRIDGE_H */
