/**
 * @file nimcp_mesh_sat_solver.h
 * @brief DPLL-based SAT Solver for Endorsement Constraint Satisfaction
 *
 * WHAT: Boolean satisfiability solver for endorsement policies
 * WHY:  Complex endorsement constraints need formal verification
 * HOW:  DPLL algorithm with unit propagation and pure literal elimination
 *
 * BRAIN ANALOGY:
 * ```
 *   Prefrontal Cortex constraint checking:
 *   "Can I perform this action given current state?"
 *
 *   Constraints as clauses:
 *   (motor_cortex ∨ premotor) ∧ (¬amygdala_veto ∨ override)
 *
 *   SAT solver finds valid assignment or proves unsatisfiable
 * ```
 *
 * INTEGRATION:
 * - Immune System: Constraint violations → antigens
 * - BBB: Validates constraint integrity
 * - KG Wiring: Constraint graph representation
 * - Logging: Full audit trail
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_SAT_SOLVER_H
#define NIMCP_MESH_SAT_SOLVER_H

#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum variables in a SAT instance */
#define SAT_MAX_VARIABLES           256

/** @brief Maximum clauses in a SAT instance */
#define SAT_MAX_CLAUSES             1024

/** @brief Maximum literals per clause */
#define SAT_MAX_LITERALS_PER_CLAUSE 16

/** @brief Maximum solving depth (recursion limit) */
#define SAT_MAX_DEPTH               512

/** @brief SAT solver magic number */
#define SAT_SOLVER_MAGIC            0x53415453  /* "SATS" */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Variable assignment state
 */
typedef enum sat_value {
    SAT_VALUE_UNASSIGNED = 0,   /**< Not yet assigned */
    SAT_VALUE_TRUE,             /**< Assigned TRUE */
    SAT_VALUE_FALSE             /**< Assigned FALSE */
} sat_value_t;

/**
 * @brief Solver result
 */
typedef enum sat_result {
    SAT_RESULT_UNKNOWN = 0,     /**< Not yet solved */
    SAT_RESULT_SATISFIABLE,     /**< Solution found */
    SAT_RESULT_UNSATISFIABLE,   /**< No solution exists */
    SAT_RESULT_TIMEOUT,         /**< Solving timed out */
    SAT_RESULT_ERROR            /**< Error during solving */
} sat_result_t;

/**
 * @brief Literal (variable or its negation)
 *
 * Encoding: positive = variable, negative = negation
 * Variable 1 → literal 1, ¬1 → literal -1
 */
typedef int32_t sat_literal_t;

/**
 * @brief Clause (disjunction of literals)
 */
typedef struct sat_clause {
    sat_literal_t literals[SAT_MAX_LITERALS_PER_CLAUSE];
    size_t literal_count;
    bool satisfied;             /**< True if clause is satisfied */
    bool active;                /**< False if removed during solving */
    float weight;               /**< For weighted MAX-SAT (optional) */
} sat_clause_t;

/**
 * @brief Variable metadata
 */
typedef struct sat_variable {
    mesh_participant_id_t module_id;    /**< Associated module (0 if none) */
    char name[64];                       /**< Human-readable name */
    sat_value_t value;                   /**< Current assignment */
    int decision_level;                  /**< When assigned (-1 = unassigned) */
    bool is_decision;                    /**< True if decision, false if propagated */
    float activity;                      /**< VSIDS activity score */
} sat_variable_t;

/**
 * @brief Assignment (variable → value)
 */
typedef struct sat_assignment {
    uint32_t variable;
    sat_value_t value;
} sat_assignment_t;

/**
 * @brief SAT solver statistics
 */
typedef struct sat_stats {
    uint64_t decisions;         /**< Number of decisions made */
    uint64_t propagations;      /**< Unit propagations */
    uint64_t conflicts;         /**< Conflicts encountered */
    uint64_t backtracks;        /**< Backtracks performed */
    uint64_t restarts;          /**< Solver restarts */
    uint64_t learned_clauses;   /**< Clauses learned from conflicts */
    float solve_time_ms;        /**< Total solving time */
} sat_stats_t;

/**
 * @brief SAT solver configuration
 */
typedef struct sat_solver_config {
    float timeout_ms;           /**< Solving timeout (0 = no limit) */
    size_t max_conflicts;       /**< Max conflicts before restart */
    bool enable_learning;       /**< Enable conflict-driven clause learning */
    bool enable_pure_literal;   /**< Enable pure literal elimination */
    bool enable_vsids;          /**< Enable VSIDS variable ordering */
    float vsids_decay;          /**< VSIDS decay factor */
    bool enable_logging;        /**< Enable detailed logging */
} sat_solver_config_t;

/**
 * @brief SAT solver instance (opaque)
 */
typedef struct sat_solver sat_solver_t;

/**
 * @brief Endorsement constraint
 *
 * Maps endorsement requirements to SAT clauses
 */
typedef struct endorsement_constraint {
    char name[64];                      /**< Constraint name */
    sat_clause_t* clauses;              /**< CNF clauses */
    size_t clause_count;                /**< Number of clauses */
    mesh_participant_id_t* required;    /**< Required endorsers (must be TRUE) */
    size_t required_count;
    mesh_participant_id_t* forbidden;   /**< Forbidden endorsers (must be FALSE) */
    size_t forbidden_count;
    float priority;                     /**< Constraint priority */
} endorsement_constraint_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default solver configuration
 */
nimcp_error_t sat_solver_default_config(sat_solver_config_t* config);

/**
 * @brief Create SAT solver
 *
 * @param config Configuration (NULL for defaults)
 * @return Solver instance or NULL on failure
 */
sat_solver_t* sat_solver_create(const sat_solver_config_t* config);

/**
 * @brief Destroy SAT solver
 */
void sat_solver_destroy(sat_solver_t* solver);

/**
 * @brief Reset solver for new problem
 */
nimcp_error_t sat_solver_reset(sat_solver_t* solver);

/* ============================================================================
 * Variable API
 * ============================================================================ */

/**
 * @brief Add variable to solver
 *
 * @param solver SAT solver
 * @param name Variable name (for debugging)
 * @param module_id Associated module (0 if none)
 * @param var_out Output: variable ID (1-based)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t sat_solver_add_variable(
    sat_solver_t* solver,
    const char* name,
    mesh_participant_id_t module_id,
    uint32_t* var_out
);

/**
 * @brief Get variable for module
 *
 * @param solver SAT solver
 * @param module_id Module participant ID
 * @return Variable ID or 0 if not found
 */
uint32_t sat_solver_get_variable_for_module(
    sat_solver_t* solver,
    mesh_participant_id_t module_id
);

/**
 * @brief Get module for variable
 */
mesh_participant_id_t sat_solver_get_module_for_variable(
    sat_solver_t* solver,
    uint32_t variable
);

/* ============================================================================
 * Clause API
 * ============================================================================ */

/**
 * @brief Add clause to solver
 *
 * @param solver SAT solver
 * @param literals Array of literals (positive = var, negative = ¬var)
 * @param count Number of literals
 * @param weight Clause weight (for MAX-SAT, 1.0 for standard)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t sat_solver_add_clause(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    float weight
);

/**
 * @brief Add unit clause (single literal)
 */
nimcp_error_t sat_solver_add_unit(
    sat_solver_t* solver,
    sat_literal_t literal
);

/**
 * @brief Add binary clause (two literals)
 */
nimcp_error_t sat_solver_add_binary(
    sat_solver_t* solver,
    sat_literal_t lit1,
    sat_literal_t lit2
);

/**
 * @brief Add implication (a → b, equivalent to ¬a ∨ b)
 */
nimcp_error_t sat_solver_add_implication(
    sat_solver_t* solver,
    sat_literal_t antecedent,
    sat_literal_t consequent
);

/**
 * @brief Add at-least-k constraint
 *
 * At least k of the given literals must be true
 */
nimcp_error_t sat_solver_add_at_least_k(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    size_t k
);

/**
 * @brief Add at-most-k constraint
 *
 * At most k of the given literals can be true
 */
nimcp_error_t sat_solver_add_at_most_k(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    size_t k
);

/**
 * @brief Add exactly-k constraint
 */
nimcp_error_t sat_solver_add_exactly_k(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    size_t k
);

/* ============================================================================
 * Solving API
 * ============================================================================ */

/**
 * @brief Solve the SAT instance
 *
 * @param solver SAT solver
 * @return SAT_RESULT_SATISFIABLE or SAT_RESULT_UNSATISFIABLE
 */
sat_result_t sat_solver_solve(sat_solver_t* solver);

/**
 * @brief Solve with assumptions
 *
 * Temporarily assume certain literals are true
 */
sat_result_t sat_solver_solve_with_assumptions(
    sat_solver_t* solver,
    const sat_literal_t* assumptions,
    size_t assumption_count
);

/**
 * @brief Get variable value after solving
 *
 * @param solver SAT solver (after successful solve)
 * @param variable Variable ID
 * @return SAT_VALUE_TRUE, SAT_VALUE_FALSE, or SAT_VALUE_UNASSIGNED
 */
sat_value_t sat_solver_get_value(sat_solver_t* solver, uint32_t variable);

/**
 * @brief Get all satisfying assignments
 *
 * @param solver SAT solver (after successful solve)
 * @param assignments Output array
 * @param max_assignments Maximum assignments to return
 * @param count_out Actual count returned
 */
nimcp_error_t sat_solver_get_assignments(
    sat_solver_t* solver,
    sat_assignment_t* assignments,
    size_t max_assignments,
    size_t* count_out
);

/**
 * @brief Get solver statistics
 */
nimcp_error_t sat_solver_get_stats(sat_solver_t* solver, sat_stats_t* stats);

/* ============================================================================
 * Endorsement Integration API
 * ============================================================================ */

/**
 * @brief Create constraint from endorsement policy expression
 *
 * Parses expressions like:
 *   "motor_cortex AND (cerebellum OR basal_ganglia)"
 *   "NOT amygdala_veto OR emergency_override"
 *
 * @param solver SAT solver
 * @param expression Boolean expression string
 * @param registry Participant registry (for name → ID mapping)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t sat_solver_add_policy_expression(
    sat_solver_t* solver,
    const char* expression,
    mesh_participant_registry_t* registry
);

/**
 * @brief Check if endorser set satisfies constraints
 *
 * @param solver SAT solver with constraints loaded
 * @param active_modules Modules that are active/available
 * @param active_count Number of active modules
 * @param selected_out Output: selected endorser set
 * @param selected_max Maximum endorsers to select
 * @param selected_count_out Actual count selected
 * @return SAT_RESULT_SATISFIABLE if valid set found
 */
sat_result_t sat_solver_select_endorsers(
    sat_solver_t* solver,
    const mesh_participant_id_t* active_modules,
    size_t active_count,
    mesh_participant_id_t* selected_out,
    size_t selected_max,
    size_t* selected_count_out
);

/**
 * @brief Find minimal endorser set
 *
 * Uses iterative SAT solving to find smallest valid set
 */
sat_result_t sat_solver_find_minimal_endorsers(
    sat_solver_t* solver,
    const mesh_participant_id_t* active_modules,
    size_t active_count,
    mesh_participant_id_t* selected_out,
    size_t* selected_count_out
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Create literal from variable
 */
static inline sat_literal_t sat_make_literal(uint32_t var, bool negated) {
    return negated ? -(int32_t)var : (int32_t)var;
}

/**
 * @brief Get variable from literal
 */
static inline uint32_t sat_literal_var(sat_literal_t lit) {
    return (uint32_t)(lit > 0 ? lit : -lit);
}

/**
 * @brief Check if literal is negated
 */
static inline bool sat_literal_negated(sat_literal_t lit) {
    return lit < 0;
}

/**
 * @brief Negate a literal
 */
static inline sat_literal_t sat_negate(sat_literal_t lit) {
    return -lit;
}

/**
 * @brief Convert result to string
 */
const char* sat_result_to_string(sat_result_t result);

/**
 * @brief Print solver state (for debugging)
 */
void sat_solver_print(const sat_solver_t* solver);

/**
 * @brief Print statistics
 */
void sat_solver_print_stats(const sat_stats_t* stats);

/* ============================================================================
 * BBB Integration API
 * ============================================================================ */

/**
 * Forward declaration for BBB system
 */
#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;

/**
 * @brief Set BBB system for SAT solver validation
 *
 * WHAT: Configure BBB for clause and constraint validation
 * WHY:  Prevent malicious constraint injection
 *
 * @param bbb BBB system (can be NULL to disable)
 */
void sat_solver_set_bbb(bbb_system_t bbb);

/**
 * @brief Get current BBB system for SAT solver
 *
 * @return BBB system or NULL
 */
bbb_system_t sat_solver_get_bbb(void);

/**
 * @brief Set health agent for SAT solver heartbeats
 *
 * WHAT: Configure health monitoring for long-running SAT solving
 * WHY:  Enable timeout/watchdog for computationally intensive solves
 *
 * @param agent Health agent (can be NULL to disable)
 */
void sat_solver_set_health_agent(nimcp_health_agent_t* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_SAT_SOLVER_H */
