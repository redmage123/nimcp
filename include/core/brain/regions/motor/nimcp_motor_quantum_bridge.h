/**
 * @file nimcp_motor_quantum_bridge.h
 * @brief Quantum-inspired motor trajectory optimization
 *
 * WHAT: Integrates quantum algorithms with Motor Cortex
 * WHY: Explore multiple motor trajectories simultaneously for optimal execution
 * HOW: Quantum reasoning for trajectory selection, motor program optimization
 *
 * BIOLOGICAL INSPIRATION:
 * - Motor cortex explores multiple movement options in parallel
 * - Premotor areas evaluate alternative action sequences
 * - Movement selection resembles quantum collapse to optimal trajectory
 * - Motor learning benefits from parallel evaluation of variations
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Explore multiple trajectories simultaneously
 * - Grover search: Find optimal motor program in O(sqrt(N))
 * - Interference: Cancel suboptimal movement strategies
 * - Amplitude amplification: Boost high-efficiency trajectories
 *
 * APPLICATIONS:
 * - Trajectory optimization: Find smoothest/fastest path
 * - Motor program selection: Choose most appropriate skill
 * - Movement timing: Optimize temporal coordination
 * - Error recovery: Find alternative movements when blocked
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#ifndef NIMCP_MOTOR_QUANTUM_BRIDGE_H
#define NIMCP_MOTOR_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct motor_quantum_bridge motor_quantum_bridge_t;

/**
 * @brief 3D vector for motor quantum computations
 */
typedef struct {
    float x;
    float y;
    float z;
} quantum_motor_vec3_t;

/**
 * @brief Quantum Motor configuration
 */
typedef struct {
    bool enabled;                    /**< Enable quantum optimization */
    uint32_t trajectory_alternatives; /**< Max parallel trajectories (default: 16) */
    uint32_t program_search_depth;   /**< Max program search depth (default: 100) */
    uint32_t max_grover_iterations;  /**< Max Grover iterations (default: 10) */
    float min_trajectory_confidence; /**< Min confidence for trajectory (default: 0.5) */
    bool enable_interference;        /**< Enable quantum interference (default: true) */
    bool use_superposition;          /**< Use superposition for alternatives (default: true) */
    float energy_weight;             /**< Weight for energy cost (default: 0.3) */
    float time_weight;               /**< Weight for time cost (default: 0.3) */
    float accuracy_weight;           /**< Weight for accuracy (default: 0.4) */
    uint32_t seed;                   /**< Random seed (default: 42) */
} motor_quantum_config_t;

/**
 * @brief Trajectory waypoint for quantum evaluation
 */
typedef struct {
    quantum_motor_vec3_t position;   /**< Waypoint position */
    quantum_motor_vec3_t velocity;   /**< Velocity at waypoint */
    float time_ms;                   /**< Time from start */
} quantum_trajectory_waypoint_t;

/**
 * @brief Trajectory candidate from quantum search
 */
typedef struct {
    uint32_t trajectory_id;          /**< Trajectory identifier */
    quantum_trajectory_waypoint_t* waypoints; /**< Waypoint sequence */
    uint32_t num_waypoints;          /**< Number of waypoints */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float energy_cost;               /**< Estimated energy cost [0, 1] */
    float time_cost;                 /**< Time to complete [0, 1] normalized */
    float accuracy_score;            /**< Predicted accuracy [0, 1] */
    float combined_score;            /**< Combined optimization score */
    bool is_feasible;                /**< Physical constraints satisfied */
} quantum_trajectory_candidate_t;

/**
 * @brief Trajectory optimization result
 */
typedef struct {
    quantum_trajectory_candidate_t* best_trajectory; /**< Best trajectory found */
    uint32_t trajectories_evaluated;  /**< Total trajectories evaluated */
    float satisfaction_probability;   /**< Optimization success probability */
    uint32_t grover_iterations_used;  /**< Grover iterations used */
    float optimization_speedup;       /**< Speedup vs exhaustive search */
} quantum_trajectory_result_t;

/**
 * @brief Motor program candidate
 */
typedef struct {
    uint32_t program_id;             /**< Program identifier */
    char program_name[64];           /**< Program name */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float skill_match;               /**< Match to required skill [0, 1] */
    float complexity;                /**< Program complexity [0, 1] */
    float combined_score;            /**< Combined selection score */
} quantum_program_candidate_t;

/**
 * @brief Motor program selection result
 */
typedef struct {
    quantum_program_candidate_t* best_program; /**< Best program found */
    uint32_t programs_evaluated;     /**< Total programs evaluated */
    float satisfaction_probability;  /**< Selection success probability */
    uint32_t grover_iterations_used; /**< Grover iterations used */
} quantum_program_result_t;

/**
 * @brief Timing candidate for movement coordination
 */
typedef struct {
    uint32_t timing_id;              /**< Timing pattern identifier */
    float* time_offsets;             /**< Time offsets for each phase */
    uint32_t num_phases;             /**< Number of timing phases */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float coordination_score;        /**< Movement coordination quality */
    float rhythm_score;              /**< Rhythmic consistency */
} quantum_timing_candidate_t;

/**
 * @brief Timing optimization result
 */
typedef struct {
    quantum_timing_candidate_t* best_timing; /**< Best timing pattern */
    uint32_t patterns_evaluated;     /**< Total patterns evaluated */
    float optimization_score;        /**< Overall timing optimization */
} quantum_timing_result_t;

/**
 * @brief Statistics for quantum motor operations
 */
typedef struct {
    uint64_t trajectory_optimizations; /**< Total trajectory optimizations */
    uint64_t program_selections;       /**< Total program selections */
    uint64_t timing_optimizations;     /**< Total timing optimizations */
    float avg_trajectory_speedup;      /**< Average trajectory speedup */
    float avg_program_speedup;         /**< Average program selection speedup */
    float avg_satisfaction_prob;       /**< Average success probability */
    uint64_t successful_optimizations; /**< High confidence optimizations */
    uint64_t failed_optimizations;     /**< Low confidence optimizations */
} motor_quantum_stats_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default quantum motor configuration
 * @return Default configuration
 */
motor_quantum_config_t motor_quantum_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create quantum motor bridge
 * @param motor Motor adapter handle
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
motor_quantum_bridge_t* motor_quantum_bridge_create(
    void* motor,
    const motor_quantum_config_t* config
);

/**
 * @brief Destroy quantum motor bridge
 * @param bridge Bridge to destroy
 */
void motor_quantum_bridge_destroy(motor_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum optimization is enabled
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool motor_quantum_bridge_is_enabled(const motor_quantum_bridge_t* bridge);

/**
 * @brief Enable or disable quantum optimization
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void motor_quantum_bridge_set_enabled(motor_quantum_bridge_t* bridge, bool enabled);

//=============================================================================
// Trajectory Optimization API
//=============================================================================

/**
 * @brief Optimize trajectory using quantum search
 * @param bridge Quantum bridge
 * @param start_position Movement start position
 * @param end_position Movement end position
 * @param max_duration Maximum allowed duration (ms)
 * @param num_alternatives Number of trajectory alternatives to consider
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) exhaustive search
 */
int motor_quantum_optimize_trajectory(
    motor_quantum_bridge_t* bridge,
    const quantum_motor_vec3_t* start_position,
    const quantum_motor_vec3_t* end_position,
    float max_duration,
    uint32_t num_alternatives,
    quantum_trajectory_result_t* result
);

/**
 * @brief Optimize multi-waypoint trajectory
 * @param bridge Quantum bridge
 * @param waypoints Required waypoints to pass through
 * @param num_waypoints Number of waypoints
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int motor_quantum_optimize_path(
    motor_quantum_bridge_t* bridge,
    const quantum_trajectory_waypoint_t* waypoints,
    uint32_t num_waypoints,
    quantum_trajectory_result_t* result
);

//=============================================================================
// Motor Program Selection API
//=============================================================================

/**
 * @brief Select motor program using quantum search
 * @param bridge Quantum bridge
 * @param skill_requirements Required skill characteristics (float array)
 * @param skill_dim Dimension of skill requirement vector
 * @param num_programs Number of candidate programs
 * @param result Output: selection result
 * @return 0 on success, -1 on error
 */
int motor_quantum_select_program(
    motor_quantum_bridge_t* bridge,
    const float* skill_requirements,
    uint32_t skill_dim,
    uint32_t num_programs,
    quantum_program_result_t* result
);

//=============================================================================
// Timing Optimization API
//=============================================================================

/**
 * @brief Optimize movement timing using quantum search
 * @param bridge Quantum bridge
 * @param base_timing Base timing pattern to optimize
 * @param num_phases Number of timing phases
 * @param result Output: timing optimization result
 * @return 0 on success, -1 on error
 */
int motor_quantum_optimize_timing(
    motor_quantum_bridge_t* bridge,
    const float* base_timing,
    uint32_t num_phases,
    quantum_timing_result_t* result
);

//=============================================================================
// Parallel Evaluation API
//=============================================================================

/**
 * @brief Evaluate multiple motor programs in quantum superposition
 * @param bridge Quantum bridge
 * @param program_ids Array of program IDs to evaluate
 * @param num_programs Number of programs
 * @param goal_position Target end position
 * @param result Output: best program result
 * @return 0 on success, -1 on error
 */
int motor_quantum_parallel_evaluate(
    motor_quantum_bridge_t* bridge,
    const uint32_t* program_ids,
    uint32_t num_programs,
    const quantum_motor_vec3_t* goal_position,
    quantum_program_result_t* result
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get quantum motor statistics
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int motor_quantum_get_stats(
    const motor_quantum_bridge_t* bridge,
    motor_quantum_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Quantum bridge
 */
void motor_quantum_reset_stats(motor_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int motor_quantum_get_config(
    const motor_quantum_bridge_t* bridge,
    motor_quantum_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MOTOR_QUANTUM_BRIDGE_H */
