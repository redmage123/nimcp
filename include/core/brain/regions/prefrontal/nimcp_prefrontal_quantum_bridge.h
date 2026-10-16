/**
 * @file nimcp_prefrontal_quantum_bridge.h
 * @brief Quantum Acceleration Bridge for Prefrontal Cortex
 *
 * WHAT: Integrates quantum algorithms with prefrontal cortex processing
 * WHY:  Decision-making and planning can benefit from quantum speedup
 * HOW:  Uses quantum superposition for parallel option evaluation
 *
 * QUANTUM APPLICATIONS:
 * - Decision Trees: Grover-accelerated search through decision space
 * - Option Evaluation: Parallel utility computation via superposition
 * - Conflict Resolution: Quantum annealing for multi-objective optimization
 * - Planning: QAOA for constraint satisfaction in action sequences
 *
 * BIOLOGICAL ANALOGY:
 * - Prefrontal cortex simultaneously maintains multiple potential actions
 * - Neural populations encode probability distributions over options
 * - Quantum superposition mirrors "fuzzy" pre-decision neural states
 * - Measurement/collapse analogous to decision commitment
 *
 * INTEGRATION:
 * - Connects to prefrontal adapter for decision-making enhancement
 * - Uses quantum reasoner infrastructure (qreason_t)
 * - Provides speedup metrics and statistics
 * - Falls back to classical when quantum unavailable
 *
 * @version Phase PFC-Q1: Quantum-Prefrontal Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_PREFRONTAL_QUANTUM_BRIDGE_H
#define NIMCP_PREFRONTAL_QUANTUM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Quantum reasoning infrastructure */
#include "core/brain/inference/nimcp_brain_quantum_reasoning.h"

/* Forward declarations */
typedef struct prefrontal_quantum_bridge prefrontal_quantum_bridge_t;
struct prefrontal_adapter;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Quantum bridge configuration
 */
typedef struct {
    bool enabled;                       /**< Enable quantum acceleration */
    uint32_t max_decision_qubits;       /**< Qubits for decision encoding */
    uint32_t max_planning_qubits;       /**< Qubits for planning optimization */
    uint32_t max_grover_iterations;     /**< Max Grover iterations per search */
    float min_decision_confidence;      /**< Min confidence to accept decision */
    float min_speedup_threshold;        /**< Min speedup to use quantum */
    bool enable_superposition_eval;     /**< Parallel option evaluation */
    bool enable_quantum_annealing;      /**< Use QA for optimization */
    bool use_amplitude_estimation;      /**< Use AE for probability estimation */
    uint32_t seed;                      /**< RNG seed for simulation */
} prefrontal_quantum_config_t;

/*=============================================================================
 * QUANTUM DECISION STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum decision candidate
 *
 * Represents a decision option in quantum superposition
 */
typedef struct {
    uint32_t option_id;                 /**< Option identifier */
    float amplitude;                    /**< Quantum amplitude |psi|^2 */
    float classical_utility;            /**< Classical utility value */
    float quantum_adjusted_utility;     /**< Quantum-adjusted utility */
    float interference_contribution;    /**< Interference term contribution */
    bool in_superposition;              /**< Part of current superposition */
} quantum_decision_candidate_t;

/**
 * @brief Quantum decision evaluation result
 */
typedef struct {
    quantum_decision_candidate_t* best_candidate; /**< Selected option */
    uint32_t candidates_evaluated;      /**< Total candidates in superposition */
    float satisfaction_probability;     /**< Probability of optimal solution */
    float quantum_speedup;              /**< Achieved speedup over classical */
    uint32_t grover_iterations_used;    /**< Grover iterations performed */
    float coherence_time_used;          /**< Quantum coherence consumed */
    bool used_quantum;                  /**< Whether quantum was actually used */
} quantum_decision_result_t;

/*=============================================================================
 * QUANTUM PLANNING STRUCTURES
 *===========================================================================*/

/**
 * @brief Quantum plan candidate
 *
 * Represents a complete action plan in quantum state
 */
typedef struct {
    uint32_t plan_id;                   /**< Plan identifier */
    uint32_t* action_sequence;          /**< Encoded action IDs */
    uint32_t action_count;              /**< Number of actions */
    float amplitude;                    /**< Quantum amplitude */
    float constraint_satisfaction;      /**< How well constraints are met */
    float expected_value;               /**< Total expected plan value */
    float feasibility;                  /**< Implementation feasibility */
} quantum_plan_candidate_t;

/**
 * @brief Quantum planning optimization result
 */
typedef struct {
    quantum_plan_candidate_t* best_plan; /**< Optimal plan found */
    uint32_t plans_explored;            /**< Plans in search space */
    float optimization_quality;         /**< Solution quality [0, 1] */
    float quantum_speedup;              /**< Speedup over classical */
    uint32_t qaoa_layers_used;          /**< QAOA circuit depth */
    bool constraints_satisfied;         /**< All constraints met */
} quantum_planning_result_t;

/*=============================================================================
 * QUANTUM CONFLICT RESOLUTION
 *===========================================================================*/

/**
 * @brief Conflict resolution via quantum annealing
 */
typedef struct {
    uint32_t conflict_id;               /**< Conflict identifier */
    float* goal_weights;                /**< Weights for each goal */
    uint32_t num_goals;                 /**< Number of conflicting goals */
    float resolution_quality;           /**< Quality of resolution */
    uint32_t* selected_priorities;      /**< Final priority ordering */
} quantum_conflict_result_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Quantum bridge statistics
 */
typedef struct {
    /* Decision acceleration */
    uint64_t decisions_accelerated;     /**< Quantum-accelerated decisions */
    float avg_decision_speedup;         /**< Average speedup factor */
    float total_coherence_used;         /**< Total coherence consumed */

    /* Planning optimization */
    uint64_t plans_optimized;           /**< Quantum-optimized plans */
    float avg_planning_speedup;         /**< Average planning speedup */
    float avg_optimization_quality;     /**< Average solution quality */

    /* Conflict resolution */
    uint64_t conflicts_resolved;        /**< Quantum-resolved conflicts */
    float avg_resolution_quality;       /**< Average resolution quality */

    /* Fallback tracking */
    uint64_t classical_fallbacks;       /**< Times fell back to classical */
    float avg_reason_for_fallback;      /**< Common fallback reasons */
} prefrontal_quantum_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default quantum bridge configuration
 *
 * WHAT: Returns sensible defaults for quantum acceleration
 * WHY:  Provide working configuration for common use cases
 * HOW:  Initialize with proven parameter values
 *
 * @return Default configuration
 */
prefrontal_quantum_config_t prefrontal_quantum_default_config(void);

/**
 * @brief Create quantum bridge for prefrontal cortex
 *
 * WHAT: Initialize quantum acceleration infrastructure
 * WHY:  Enable quantum speedup for decision and planning
 * HOW:  Create quantum reasoner, allocate state buffers
 *
 * @param prefrontal Prefrontal adapter handle (void* for flexibility)
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance, or NULL on failure
 */
prefrontal_quantum_bridge_t* prefrontal_quantum_bridge_create(
    void* prefrontal,
    const prefrontal_quantum_config_t* config
);

/**
 * @brief Destroy quantum bridge
 *
 * WHAT: Free all quantum bridge resources
 * WHY:  Clean resource management
 * HOW:  Destroy quantum reasoner, free buffers
 *
 * @param bridge Bridge to destroy
 */
void prefrontal_quantum_bridge_destroy(prefrontal_quantum_bridge_t* bridge);

/**
 * @brief Check if quantum acceleration is enabled
 *
 * @param bridge Bridge instance
 * @return true if enabled
 */
bool prefrontal_quantum_bridge_is_enabled(const prefrontal_quantum_bridge_t* bridge);

/**
 * @brief Enable/disable quantum acceleration
 *
 * @param bridge Bridge instance
 * @param enabled Enable state
 */
void prefrontal_quantum_bridge_set_enabled(prefrontal_quantum_bridge_t* bridge, bool enabled);

/*=============================================================================
 * QUANTUM DECISION ACCELERATION
 *===========================================================================*/

/**
 * @brief Accelerate decision evaluation using quantum search
 *
 * WHAT: Use Grover search to find optimal decision option
 * WHY:  O(sqrt(N)) speedup over classical exhaustive search
 * HOW:  Encode options in superposition, apply Grover, measure
 *
 * ALGORITHM:
 * 1. Encode N options as quantum states |0>, |1>, ..., |N-1>
 * 2. Prepare uniform superposition sum_i |i>
 * 3. Apply Grover oracle marking high-utility options
 * 4. Apply Grover diffusion operator
 * 5. Repeat O(sqrt(N)) times
 * 6. Measure to obtain selected option
 *
 * @param bridge Quantum bridge
 * @param utilities Classical utility values for each option
 * @param num_options Number of options to evaluate
 * @param min_utility Minimum acceptable utility (oracle threshold)
 * @param result Output result structure
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_accelerate_decision(
    prefrontal_quantum_bridge_t* bridge,
    const float* utilities,
    uint32_t num_options,
    float min_utility,
    quantum_decision_result_t* result
);

/**
 * @brief Parallel option evaluation via superposition
 *
 * WHAT: Evaluate multiple decision options simultaneously
 * WHY:  Exponential parallelism for multi-criteria evaluation
 * HOW:  Encode options and criteria in quantum state, apply operators
 *
 * @param bridge Quantum bridge
 * @param options Array of option feature vectors
 * @param option_dim Dimension of each option vector
 * @param num_options Number of options
 * @param criteria Evaluation criteria weights
 * @param num_criteria Number of criteria
 * @param result Output result
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_parallel_eval(
    prefrontal_quantum_bridge_t* bridge,
    const float** options,
    uint32_t option_dim,
    uint32_t num_options,
    const float* criteria,
    uint32_t num_criteria,
    quantum_decision_result_t* result
);

/*=============================================================================
 * QUANTUM PLANNING OPTIMIZATION
 *===========================================================================*/

/**
 * @brief Optimize action plan using quantum algorithms
 *
 * WHAT: Find optimal action sequence satisfying constraints
 * WHY:  Planning is NP-hard, quantum provides polynomial speedup
 * HOW:  QAOA for constraint satisfaction + value optimization
 *
 * @param bridge Quantum bridge
 * @param available_actions Array of available action IDs
 * @param num_actions Number of available actions
 * @param constraints Constraint matrix (action dependencies)
 * @param values Expected value of each action
 * @param max_plan_length Maximum actions in plan
 * @param result Output planning result
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_optimize_plan(
    prefrontal_quantum_bridge_t* bridge,
    const uint32_t* available_actions,
    uint32_t num_actions,
    const float* constraints,
    const float* values,
    uint32_t max_plan_length,
    quantum_planning_result_t* result
);

/**
 * @brief Quantum tree search for hierarchical planning
 *
 * WHAT: Search decision tree using quantum walk
 * WHY:  Quadratic speedup for tree-structured search spaces
 * HOW:  Quantum walk on decision tree graph
 *
 * @param bridge Quantum bridge
 * @param tree_adjacency Adjacency matrix of decision tree
 * @param tree_size Number of nodes in tree
 * @param node_values Value at each tree node
 * @param goal_nodes Goal node indicators
 * @param result Output planning result
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_tree_search(
    prefrontal_quantum_bridge_t* bridge,
    const uint8_t* tree_adjacency,
    uint32_t tree_size,
    const float* node_values,
    const bool* goal_nodes,
    quantum_planning_result_t* result
);

/*=============================================================================
 * QUANTUM CONFLICT RESOLUTION
 *===========================================================================*/

/**
 * @brief Resolve goal conflicts using quantum annealing
 *
 * WHAT: Find optimal priority ordering for conflicting goals
 * WHY:  Multi-objective optimization is combinatorially hard
 * HOW:  Encode as QUBO, solve via simulated quantum annealing
 *
 * @param bridge Quantum bridge
 * @param goal_values Value of each goal
 * @param goal_conflicts Pairwise conflict matrix
 * @param num_goals Number of conflicting goals
 * @param result Output conflict resolution
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_resolve_conflict(
    prefrontal_quantum_bridge_t* bridge,
    const float* goal_values,
    const float* goal_conflicts,
    uint32_t num_goals,
    quantum_conflict_result_t* result
);

/*=============================================================================
 * PROBABILITY ESTIMATION
 *===========================================================================*/

/**
 * @brief Estimate decision success probability via amplitude estimation
 *
 * WHAT: Precisely estimate probability of successful outcome
 * WHY:  Quadratic speedup over classical Monte Carlo
 * HOW:  Quantum amplitude estimation algorithm
 *
 * @param bridge Quantum bridge
 * @param action_id Action to estimate
 * @param context Context features
 * @param context_size Size of context
 * @param probability Output probability estimate
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_estimate_probability(
    prefrontal_quantum_bridge_t* bridge,
    uint32_t action_id,
    const float* context,
    uint32_t context_size,
    float* probability
);

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get quantum bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output statistics
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_get_stats(
    const prefrontal_quantum_bridge_t* bridge,
    prefrontal_quantum_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge instance
 */
void prefrontal_quantum_reset_stats(prefrontal_quantum_bridge_t* bridge);

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge instance
 * @param config Output configuration
 * @return 0 on success, -1 on failure
 */
int prefrontal_quantum_get_config(
    const prefrontal_quantum_bridge_t* bridge,
    prefrontal_quantum_config_t* config
);

/**
 * @brief Check quantum resource availability
 *
 * @param bridge Bridge instance
 * @param qubits_available Output: available qubits
 * @param coherence_remaining Output: remaining coherence time
 * @return true if quantum resources available
 */
bool prefrontal_quantum_check_resources(
    const prefrontal_quantum_bridge_t* bridge,
    uint32_t* qubits_available,
    float* coherence_remaining
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREFRONTAL_QUANTUM_BRIDGE_H */
