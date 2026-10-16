/**
 * @file nimcp_cerebellum_quantum_bridge.h
 * @brief Quantum-inspired motor coordination optimization
 *
 * WHAT: Integrates quantum algorithms with the Cerebellum
 * WHY: Explore multiple motor programs simultaneously for optimal execution
 * HOW: Quantum reasoning for timing optimization, parallel motor program evaluation
 *
 * BIOLOGICAL INSPIRATION:
 * - Cerebellum explores multiple motor trajectories in parallel
 * - Purkinje cells maintain superposition of timing patterns
 * - Motor program selection resembles quantum collapse to optimal trajectory
 * - Error-based learning benefits from parallel path evaluation
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Explore multiple motor programs simultaneously
 * - Grover search: Find optimal timing in O(sqrt(N))
 * - Interference: Cancel suboptimal motor trajectories
 * - Amplitude amplification: Boost well-coordinated movements
 *
 * APPLICATIONS:
 * - Motor timing: Find optimal execution timing
 * - Trajectory optimization: Evaluate multiple paths in parallel
 * - Error correction: Quantum-accelerated error signal processing
 * - Gain adaptation: Optimize motor gains across multiple DOF
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_CEREBELLUM_QUANTUM_BRIDGE_H
#define NIMCP_CEREBELLUM_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Types
 *===========================================================================*/

typedef struct cerebellum_quantum_bridge cerebellum_quantum_bridge_t;

/**
 * @brief Quantum Cerebellum configuration
 */
typedef struct {
    bool enabled;                      /**< Enable quantum optimization */
    uint32_t timing_search_depth;      /**< Max timing search depth (default: 1000) */
    uint32_t trajectory_alternatives;  /**< Max parallel trajectories (default: 16) */
    uint32_t max_grover_iterations;    /**< Max Grover iterations (default: 10) */
    float min_timing_confidence;       /**< Min confidence for timing (default: 0.5) */
    bool enable_interference;          /**< Enable quantum interference (default: true) */
    bool use_superposition;            /**< Use superposition for alternatives (default: true) */
    uint32_t seed;                     /**< Random seed (default: 42) */
} cerebellum_quantum_config_t;

/**
 * @brief Timing candidate from quantum search
 */
typedef struct {
    uint32_t timing_id;                /**< Timing identifier */
    float timing_ms;                   /**< Timing value in milliseconds */
    float amplitude;                   /**< Quantum amplitude [0, 1] */
    float precision_score;             /**< Timing precision [0, 1] */
    float energy_cost;                 /**< Motor energy cost [0, 1] */
    float combined_score;              /**< Combined optimization score */
} quantum_timing_candidate_t;

/**
 * @brief Timing optimization result
 */
typedef struct {
    quantum_timing_candidate_t* best_timing;  /**< Best timing found */
    uint32_t candidates_evaluated;             /**< Total candidates */
    float satisfaction_probability;             /**< Search success probability */
    uint32_t grover_iterations_used;           /**< Grover iterations */
    float search_speedup;                       /**< Speedup vs linear search */
} quantum_timing_result_t;

/**
 * @brief Motor trajectory candidate
 */
typedef struct {
    uint32_t trajectory_id;            /**< Trajectory identifier */
    float waypoints[32];               /**< Trajectory waypoints (up to 32) */
    uint32_t num_waypoints;            /**< Number of waypoints */
    float amplitude;                   /**< Quantum amplitude [0, 1] */
    float smoothness_score;            /**< Trajectory smoothness [0, 1] */
    float energy_efficiency;           /**< Energy efficiency [0, 1] */
    float duration_ms;                 /**< Total duration */
    bool is_feasible;                  /**< Kinematic constraints satisfied */
} quantum_trajectory_candidate_t;

/**
 * @brief Trajectory optimization result
 */
typedef struct {
    quantum_trajectory_candidate_t* best_trajectory; /**< Best trajectory found */
    uint32_t trajectories_evaluated;                 /**< Total trajectories */
    float satisfaction_probability;                   /**< Optimization success */
    uint32_t grover_iterations_used;                 /**< Grover iterations */
} quantum_trajectory_result_t;

/**
 * @brief Motor gain candidate
 */
typedef struct {
    uint32_t gain_set_id;              /**< Gain set identifier */
    float gains[8];                    /**< Gain values per DOF (up to 8) */
    uint32_t num_gains;                /**< Number of gains */
    float amplitude;                   /**< Quantum amplitude */
    float stability_score;             /**< Control stability [0, 1] */
    float responsiveness;              /**< Response speed [0, 1] */
} quantum_gain_candidate_t;

/**
 * @brief Gain optimization result
 */
typedef struct {
    quantum_gain_candidate_t* best_gains;  /**< Best gain set found */
    uint32_t candidates_evaluated;          /**< Total candidates */
    float optimization_score;               /**< Overall quality */
} quantum_gain_result_t;

/**
 * @brief Statistics for quantum cerebellum operations
 */
typedef struct {
    uint64_t timing_optimizations;     /**< Total timing optimizations */
    uint64_t trajectory_optimizations; /**< Total trajectory optimizations */
    uint64_t gain_optimizations;       /**< Total gain optimizations */
    float avg_timing_speedup;          /**< Average timing search speedup */
    float avg_trajectory_speedup;      /**< Average trajectory optimization speedup */
    float avg_satisfaction_prob;       /**< Average success probability */
    uint64_t successful_optimizations; /**< Optimizations with high confidence */
    uint64_t failed_optimizations;     /**< Optimizations with low confidence */
} cerebellum_quantum_stats_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * @brief Get default quantum cerebellum configuration
 * @return Default configuration
 */
cerebellum_quantum_config_t cerebellum_quantum_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create quantum cerebellum bridge
 * @param cerebellum Cerebellum adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
cerebellum_quantum_bridge_t* cerebellum_quantum_bridge_create(
    void* cerebellum,
    const cerebellum_quantum_config_t* config
);

/**
 * @brief Destroy quantum cerebellum bridge
 * @param bridge Bridge to destroy
 */
void cerebellum_quantum_bridge_destroy(cerebellum_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool cerebellum_quantum_bridge_is_enabled(const cerebellum_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void cerebellum_quantum_bridge_set_enabled(cerebellum_quantum_bridge_t* bridge, bool enabled);

/*=============================================================================
 * Timing Optimization API
 *===========================================================================*/

/**
 * @brief Optimize motor timing using quantum search
 * @param bridge Quantum bridge
 * @param target_timing_ms Target timing in milliseconds
 * @param timing_range_ms Acceptable timing range
 * @param num_alternatives Number of timing alternatives to evaluate
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) linear search
 */
int cerebellum_quantum_optimize_timing(
    cerebellum_quantum_bridge_t* bridge,
    float target_timing_ms,
    float timing_range_ms,
    uint32_t num_alternatives,
    quantum_timing_result_t* result
);

/*=============================================================================
 * Trajectory Optimization API
 *===========================================================================*/

/**
 * @brief Optimize motor trajectory using quantum search
 * @param bridge Quantum bridge
 * @param start_state Starting motor state
 * @param end_state Target motor state
 * @param num_dims Number of dimensions
 * @param max_duration_ms Maximum trajectory duration
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int cerebellum_quantum_optimize_trajectory(
    cerebellum_quantum_bridge_t* bridge,
    const float* start_state,
    const float* end_state,
    uint32_t num_dims,
    float max_duration_ms,
    quantum_trajectory_result_t* result
);

/*=============================================================================
 * Gain Optimization API
 *===========================================================================*/

/**
 * @brief Optimize motor gains using quantum search
 * @param bridge Quantum bridge
 * @param current_gains Current gain values
 * @param num_gains Number of gain dimensions
 * @param error_signal Current error signal
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int cerebellum_quantum_optimize_gains(
    cerebellum_quantum_bridge_t* bridge,
    const float* current_gains,
    uint32_t num_gains,
    float error_signal,
    quantum_gain_result_t* result
);

/*=============================================================================
 * Parallel Motor Program Evaluation API
 *===========================================================================*/

/**
 * @brief Evaluate multiple motor programs in parallel
 * @param bridge Quantum bridge
 * @param programs Array of motor program vectors
 * @param num_programs Number of programs
 * @param program_dims Dimensions per program
 * @param scores Output: scores for each program
 * @return 0 on success, -1 on error
 *
 * Uses quantum parallelism to evaluate all programs simultaneously.
 */
int cerebellum_quantum_evaluate_programs(
    cerebellum_quantum_bridge_t* bridge,
    const float** programs,
    uint32_t num_programs,
    uint32_t program_dims,
    float* scores
);

/**
 * @brief Select best motor program using quantum algorithm
 * @param bridge Quantum bridge
 * @param programs Array of motor program vectors
 * @param num_programs Number of programs
 * @param program_dims Dimensions per program
 * @param best_program_idx Output: index of best program
 * @param confidence Output: selection confidence
 * @return 0 on success, -1 on error
 */
int cerebellum_quantum_select_program(
    cerebellum_quantum_bridge_t* bridge,
    const float** programs,
    uint32_t num_programs,
    uint32_t program_dims,
    uint32_t* best_program_idx,
    float* confidence
);

/*=============================================================================
 * Statistics API
 *===========================================================================*/

/**
 * @brief Get quantum cerebellum statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int cerebellum_quantum_get_stats(
    const cerebellum_quantum_bridge_t* bridge,
    cerebellum_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void cerebellum_quantum_reset_stats(cerebellum_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int cerebellum_quantum_get_config(
    const cerebellum_quantum_bridge_t* bridge,
    cerebellum_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CEREBELLUM_QUANTUM_BRIDGE_H */
