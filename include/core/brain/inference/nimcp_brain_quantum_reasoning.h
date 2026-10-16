//=============================================================================
// nimcp_brain_quantum_reasoning.h - Brain-Level Quantum Reasoning Integration
//=============================================================================
/**
 * @file nimcp_brain_quantum_reasoning.h
 * @brief Quantum-accelerated reasoning integrated into brain architecture
 *
 * WHAT: Brain-level integration of quantum reasoning algorithms
 * WHY:  Enables O(√N) SAT solving for logical inference in the brain
 * HOW:  Wraps nimcp_quantum_reasoning.h with brain-aware interface
 *
 * BIOLOGICAL RATIONALE:
 * The prefrontal cortex explores multiple hypotheses simultaneously before
 * committing to a decision. Quantum superposition models this parallel
 * exploration, while quantum interference eliminates contradictory paths.
 *
 * KEY CAPABILITIES:
 * 1. Quantum SAT Solving:
 *    - Grover's algorithm for O(√N) search over logical assignments
 *    - Brain-level queries like "can goal X be achieved given constraints?"
 *
 * 2. Ternary Logic:
 *    - TRUE (+1), FALSE (-1), UNKNOWN (0)
 *    - Models uncertainty in reasoning (incomplete information)
 *    - Kleene 3-valued logic semantics
 *
 * 3. Quantum Interference:
 *    - Destructive interference cancels contradictory inference paths
 *    - Constructive interference amplifies consistent conclusions
 *
 * 4. Knowledge Base Integration:
 *    - Facts stored with confidence levels
 *    - Rules for forward/backward chaining
 *    - Quantum-accelerated rule matching
 *
 * INTEGRATION POINTS:
 * - Executive Controller: Goal feasibility checking
 * - Working Memory: Constraint satisfaction for plans
 * - Neural Logic Gates: Quantum-accelerated gate evaluation
 * - Reasoning Learning: Accelerated rule induction
 * - Parietal Lobe: Mathematical proof verification
 *
 * PERFORMANCE:
 * - Classical SAT: O(2^N) worst case
 * - Quantum SAT: O(√(2^N)) = O(2^(N/2)) via Grover's algorithm
 * - For N=20 variables: 1M → 1K iterations (1000x speedup)
 *
 * @author NIMCP Development Team
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_QUANTUM_REASONING_H
#define NIMCP_BRAIN_QUANTUM_REASONING_H

#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module identifier for bio-async */
#define BIO_MODULE_BRAIN_QUANTUM_REASONING  0x0B20

/** Maximum variables in brain-level SAT queries */
#define BRAIN_QREASON_MAX_VARIABLES  QREASON_MAX_VARIABLES

/** Maximum clauses in brain-level CNF formulas */
#define BRAIN_QREASON_MAX_CLAUSES    QREASON_MAX_CLAUSES

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Brain quantum reasoning configuration
 *
 * WHAT: Configuration for brain-level quantum reasoning
 * WHY:  Allows tuning based on brain's current state
 */
typedef struct brain_qreason_config_s {
    bool enabled;                    /**< Enable quantum reasoning */

    /* Algorithm settings */
    uint32_t max_grover_iterations;  /**< Max Grover iterations (0=auto) */
    uint32_t max_inference_depth;    /**< Max inference chain depth */
    float min_confidence;            /**< Minimum confidence threshold [0,1] */

    /* Integration settings */
    bool use_ternary_logic;          /**< Enable TRUE/FALSE/UNKNOWN */
    bool enable_interference;         /**< Enable quantum interference */
    bool integrate_with_executive;   /**< Connect to executive controller */
    bool integrate_with_parietal;    /**< Connect to parietal lobe */

    /* Modulation */
    float fatigue_sensitivity;       /**< How fatigue affects reasoning [0,1] */
    float stress_sensitivity;        /**< How stress affects confidence [0,1] */
} brain_qreason_config_t;

/**
 * @brief Brain-level reasoning query
 *
 * WHAT: A logical query to be solved by the brain
 * WHY:  High-level interface for brain subsystems
 */
typedef struct brain_reasoning_query_s {
    char description[128];           /**< Human-readable query description */
    qreason_cnf_t cnf;               /**< CNF formula to solve */
    float urgency;                   /**< Query urgency [0,1] */
    uint32_t timeout_ms;             /**< Maximum time for query (0=unlimited) */
} brain_reasoning_query_t;

/**
 * @brief Brain-level reasoning result
 *
 * WHAT: Result of a brain-level reasoning query
 * WHY:  Provides rich output for decision making
 */
typedef struct brain_reasoning_result_s {
    bool satisfiable;                /**< Query is satisfiable */
    float confidence;                /**< Result confidence [0,1] */
    float satisfaction_probability;  /**< Probability of satisfaction */

    /* Assignment (if satisfiable) */
    qreason_truth_t assignment[BRAIN_QREASON_MAX_VARIABLES];
    uint32_t num_variables;

    /* Performance metrics */
    uint32_t grover_iterations;      /**< Grover iterations used */
    uint32_t inference_depth;        /**< Inference chain depth */
    float quantum_speedup;           /**< Estimated speedup over classical */
    uint64_t solve_time_us;          /**< Solve time in microseconds */
} brain_reasoning_result_t;

/**
 * @brief Brain quantum reasoning statistics
 */
typedef struct brain_qreason_stats_s {
    uint64_t total_queries;          /**< Total queries processed */
    uint64_t satisfiable_count;      /**< Satisfiable queries */
    uint64_t unsatisfiable_count;    /**< Unsatisfiable queries */
    uint64_t timeout_count;          /**< Timed out queries */
    float avg_grover_iterations;     /**< Average Grover iterations */
    float avg_solve_time_us;         /**< Average solve time */
    float avg_confidence;            /**< Average result confidence */
    uint64_t total_grover_iterations;/**< Total Grover iterations */
} brain_qreason_stats_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default brain quantum reasoning configuration
 *
 * @return Default configuration
 */
brain_qreason_config_t brain_qreason_default_config(void);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Initialize quantum reasoning subsystem for brain
 *
 * WHAT: Creates and integrates quantum reasoning with the brain
 * WHY:  Enables quantum-accelerated logical inference
 * HOW:  Creates reasoner, connects to brain subsystems
 *
 * INITIALIZATION ORDER:
 * 1. Create qreason_t with brain-appropriate config
 * 2. Connect to executive controller if present
 * 3. Connect to parietal lobe if present
 * 4. Register with FEP orchestrator if enabled
 *
 * @param brain The brain to initialize quantum reasoning for
 * @param config Configuration (NULL for defaults)
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_quantum_reasoning(brain_t brain,
                                                  const brain_qreason_config_t* config);

/**
 * @brief Destroy quantum reasoning subsystem
 *
 * @param brain The brain containing quantum reasoning
 */
void nimcp_brain_qreason_destroy(brain_t brain);

/**
 * @brief Check if quantum reasoning is enabled
 *
 * @param brain The brain to check
 * @return true if quantum reasoning is available
 */
bool nimcp_brain_qreason_is_enabled(brain_t brain);

/**
 * @brief Enable/disable quantum reasoning
 *
 * @param brain The brain
 * @param enabled Enable state
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_set_enabled(brain_t brain, bool enabled);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Solve SAT problem using brain's quantum reasoning
 *
 * WHAT: Solves CNF satisfiability using Grover's algorithm
 * WHY:  Core quantum reasoning capability
 * HOW:  Uses brain's qreason_t with modulation from brain state
 *
 * @param brain The brain
 * @param query The reasoning query
 * @param result Output result
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_solve_sat(brain_t brain,
                                   const brain_reasoning_query_t* query,
                                   brain_reasoning_result_t* result);

/**
 * @brief Check if goal is achievable given constraints
 *
 * WHAT: High-level goal feasibility check
 * WHY:  Executive controller uses this for planning
 * HOW:  Converts goal+constraints to SAT and solves
 *
 * @param brain The brain
 * @param goal_id Goal identifier
 * @param constraints Array of constraint IDs
 * @param num_constraints Number of constraints
 * @param feasible Output: true if goal is feasible
 * @param confidence Output: confidence in result
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_check_goal_feasibility(brain_t brain,
                                                 uint32_t goal_id,
                                                 const uint32_t* constraints,
                                                 uint32_t num_constraints,
                                                 bool* feasible,
                                                 float* confidence);

/**
 * @brief Query with ternary logic (TRUE/FALSE/UNKNOWN)
 *
 * WHAT: Evaluates logical expression with uncertainty
 * WHY:  Real-world reasoning often has incomplete information
 * HOW:  Uses Kleene 3-valued logic
 *
 * @param brain The brain
 * @param variable Variable to query
 * @param value Output: ternary truth value
 * @param confidence Output: confidence in value
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_query_ternary(brain_t brain,
                                        uint32_t variable,
                                        qreason_truth_t* value,
                                        float* confidence);

//=============================================================================
// Knowledge Base API
//=============================================================================

/**
 * @brief Set a fact in the brain's knowledge base
 *
 * @param brain The brain
 * @param variable Variable index
 * @param value Truth value
 * @param confidence Confidence [0,1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_set_fact(brain_t brain,
                                   uint32_t variable,
                                   qreason_truth_t value,
                                   float confidence);

/**
 * @brief Get a fact from the brain's knowledge base
 *
 * @param brain The brain
 * @param variable Variable index
 * @param value Output: truth value
 * @param confidence Output: confidence
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_get_fact(brain_t brain,
                                   uint32_t variable,
                                   qreason_truth_t* value,
                                   float* confidence);

/**
 * @brief Add inference rule to brain's knowledge base
 *
 * @param brain The brain
 * @param antecedents Array of antecedent variable indices
 * @param num_antecedents Number of antecedents
 * @param consequent Consequent variable index
 * @param confidence Rule confidence [0,1]
 * @return Rule index on success, -1 on error
 */
int nimcp_brain_qreason_add_rule(brain_t brain,
                                   const uint32_t* antecedents,
                                   uint32_t num_antecedents,
                                   uint32_t consequent,
                                   float confidence);

/**
 * @brief Clear all facts and rules
 *
 * @param brain The brain
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_clear_kb(brain_t brain);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Apply fatigue modulation to reasoning
 *
 * WHAT: Reduces reasoning capacity when fatigued
 * WHY:  Biological realism - tired brains reason slower
 * HOW:  Reduces max iterations, increases confidence threshold
 *
 * @param brain The brain
 * @param fatigue_level Fatigue level [0,1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_set_fatigue(brain_t brain, float fatigue_level);

/**
 * @brief Apply stress modulation to reasoning
 *
 * WHAT: Affects reasoning under stress
 * WHY:  Stress impairs logical reasoning
 * HOW:  Reduces max depth, increases urgency weighting
 *
 * @param brain The brain
 * @param stress_level Stress level [0,1]
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_set_stress(brain_t brain, float stress_level);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get quantum reasoning statistics
 *
 * @param brain The brain
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_get_stats(brain_t brain, brain_qreason_stats_t* stats);

/**
 * @brief Reset quantum reasoning statistics
 *
 * @param brain The brain
 */
void nimcp_brain_qreason_reset_stats(brain_t brain);

//=============================================================================
// Internal Access (for other brain modules)
//=============================================================================

/**
 * @brief Get the underlying qreason_t handle
 *
 * WHAT: Direct access to quantum reasoner
 * WHY:  Other brain modules may need low-level access
 * HOW:  Returns opaque handle to qreason_internal_t
 *
 * @param brain The brain
 * @return qreason_t handle or NULL if not available
 */
qreason_t nimcp_brain_qreason_get_handle(brain_t brain);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/**
 * @brief Solve SAT using MCTS-guided search
 *
 * WHAT: Use Monte Carlo Tree Search for SAT solving
 * WHY:  MCTS can find solutions faster via learned exploration (2-10x speedup)
 * HOW:  Use qmc_solve_sat_mcts with variable-branching tree
 *
 * @param brain Brain handle
 * @param query Reasoning query with CNF formula
 * @param result Output: reasoning result
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_solve_sat_mcts(brain_t brain,
                                        const brain_reasoning_query_t* query,
                                        brain_reasoning_result_t* result);

/**
 * @brief Estimate satisfiability probability via Monte Carlo
 *
 * WHAT: Estimate P(SAT) using random sampling
 * WHY:  Quick estimate without full solving
 * HOW:  Sample random assignments, check clause satisfaction
 *
 * @param brain Brain handle
 * @param query Reasoning query
 * @param num_samples Number of MC samples
 * @param probability_out Output: estimated P(SAT)
 * @param variance_out Output: estimate variance (optional)
 * @return 0 on success
 */
int nimcp_brain_qreason_estimate_sat_prob_mc(brain_t brain,
                                              const brain_reasoning_query_t* query,
                                              uint32_t num_samples,
                                              float* probability_out,
                                              float* variance_out);

/**
 * @brief Get thread-local MC seed for brain quantum reasoning
 *
 * WHAT: Access the thread-local RNG seed
 * WHY:  Allow external seeding for reproducibility
 * HOW:  Return pointer to thread-local seed variable
 *
 * @return Pointer to thread-local seed
 */
uint32_t* nimcp_brain_qreason_get_mc_seed(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_QUANTUM_REASONING_H */
