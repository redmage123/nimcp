//=============================================================================
// nimcp_quantum_reasoning.h - Quantum-Inspired Symbolic Reasoning
//=============================================================================

#ifndef NIMCP_QUANTUM_REASONING_H
#define NIMCP_QUANTUM_REASONING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_quantum_reasoning.h
 * @brief Quantum-inspired algorithms for symbolic logic and reasoning
 *
 * WHAT: Quantum-inspired inference for logical reasoning
 * WHY:  Grover-like search provides speedup for SAT-like problems
 * HOW:  Amplitude encoding of truth values, quantum interference for contradiction detection
 *
 * QUANTUM CONCEPTS USED:
 * - Grover's Algorithm: Fast search for satisfying assignments
 * - Amplitude Encoding: Fuzzy truth values as quantum amplitudes
 * - Quantum Interference: Cancel contradictory inference paths
 * - Quantum Superposition: Explore multiple inference branches
 * - Phase Kickback: Propagate logical constraints
 *
 * BIOLOGICAL ANALOGY:
 * - Prefrontal cortex explores multiple hypotheses simultaneously
 * - Hippocampal pattern completion resolves ambiguity
 * - Working memory holds superposition of possibilities
 *
 * TERNARY LOGIC INTEGRATION:
 * - TRUE (+1), FALSE (-1), UNKNOWN (0)
 * - Kleene 3-valued logic for uncertain reasoning
 * - Quantum amplitude represents confidence
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

//=============================================================================
// Constants
//=============================================================================

#define QREASON_MAX_VARIABLES    32     /**< Maximum logical variables */
#define QREASON_MAX_CLAUSES      128    /**< Maximum clauses in CNF */
#define QREASON_MAX_LITERALS     8      /**< Maximum literals per clause */
#define QREASON_MAX_RULES        64     /**< Maximum inference rules */
#define QREASON_DEFAULT_SEED     98765  /**< Default random seed */
#define QREASON_QUANTUM_VAR_LIMIT 16    /**< Maximum variables for quantum solver */
#define QREASON_MIN_SAT_PROB     0.01f  /**< Minimum satisfaction probability threshold */

/* Ternary truth values */
#define QREASON_FALSE    (-1)
#define QREASON_UNKNOWN  (0)
#define QREASON_TRUE     (1)

//=============================================================================
// Types
//=============================================================================

/**
 * WHAT: Ternary truth value
 */
typedef int8_t qreason_truth_t;

/**
 * WHAT: Literal in a clause
 */
typedef struct {
    uint32_t variable;     /**< Variable index */
    bool negated;          /**< True if NOT variable */
} qreason_literal_t;

/**
 * WHAT: Clause (disjunction of literals)
 */
typedef struct {
    qreason_literal_t literals[QREASON_MAX_LITERALS];
    uint32_t n_literals;
} qreason_clause_t;

/**
 * WHAT: CNF formula (conjunction of clauses)
 */
typedef struct {
    qreason_clause_t clauses[QREASON_MAX_CLAUSES];
    uint32_t n_clauses;
    uint32_t n_variables;
} qreason_cnf_t;

/**
 * WHAT: Inference rule (Horn clause form)
 * HOW:  antecedents[0] AND antecedents[1] AND ... -> consequent
 */
typedef struct {
    uint32_t antecedents[QREASON_MAX_LITERALS]; /**< Variable indices */
    uint32_t n_antecedents;
    uint32_t consequent;    /**< Conclusion variable */
    float confidence;       /**< Rule confidence [0, 1] */
} qreason_rule_t;

/**
 * WHAT: Knowledge base
 */
typedef struct {
    qreason_truth_t facts[QREASON_MAX_VARIABLES];     /**< Known facts */
    float confidences[QREASON_MAX_VARIABLES];         /**< Fact confidences */
    qreason_rule_t rules[QREASON_MAX_RULES];
    uint32_t n_rules;
    uint32_t n_variables;
} qreason_kb_t;

/**
 * WHAT: Quantum state for reasoning
 */
typedef struct {
    float amplitudes[1 << 16];  /**< 2^n amplitudes (limited to 16 vars) */
    uint32_t n_qubits;          /**< Number of qubits (= variables) */
    uint32_t state_dim;         /**< 2^n_qubits */
} qreason_qstate_t;

/**
 * WHAT: Solver method used
 */
typedef enum {
    QREASON_METHOD_QUANTUM,     /**< Quantum/Grover-inspired solver */
    QREASON_METHOD_CLASSICAL    /**< Classical DPLL fallback */
} qreason_method_t;

/**
 * WHAT: Reasoning result
 */
typedef struct {
    qreason_truth_t assignment[QREASON_MAX_VARIABLES]; /**< Best assignment */
    float confidences[QREASON_MAX_VARIABLES];          /**< Assignment confidences */
    bool satisfiable;           /**< Is formula satisfiable? */
    float satisfaction_prob;    /**< Probability of satisfaction */
    uint32_t grover_iterations; /**< Iterations used */
    uint32_t inferences_made;   /**< Forward chaining steps */
    qreason_method_t method;    /**< Solver method used (quantum or classical) */
    uint32_t dpll_decisions;    /**< DPLL decision count (classical only) */
    uint32_t unit_propagations; /**< Unit propagations performed (classical only) */
} qreason_result_t;

/**
 * WHAT: Configuration for quantum reasoner
 */
typedef struct {
    uint32_t max_grover_iterations;  /**< Max Grover iterations */
    uint32_t max_inference_depth;    /**< Max forward chaining depth */
    float min_confidence;            /**< Minimum confidence threshold */
    bool use_ternary_logic;          /**< Enable 3-valued logic */
    bool enable_interference;        /**< Enable quantum interference */
    uint32_t seed;                   /**< Random seed */
} qreason_config_t;

/**
 * WHAT: Quantum reasoner context (opaque handle)
 */
typedef struct qreason_struct* qreason_t;

/**
 * WHAT: Internal structure
 */
typedef struct qreason_struct {
    qreason_config_t config;
    qreason_kb_t kb;
    qreason_qstate_t qstate;
    uint32_t rng_state;

    /* Statistics */
    uint64_t queries_performed;
    uint64_t satisfiable_count;
    uint64_t unsatisfiable_count;
    uint64_t total_grover_iterations;
} qreason_internal_t;

/**
 * WHAT: Statistics
 */
typedef struct {
    uint64_t queries_performed;
    uint64_t satisfiable_count;
    uint64_t unsatisfiable_count;
    float satisfiability_rate;
    float avg_grover_iterations;
} qreason_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

static inline qreason_config_t qreason_default_config(void) {
    return (qreason_config_t){
        .max_grover_iterations = 10,
        .max_inference_depth = 20,
        .min_confidence = 0.5f,
        .use_ternary_logic = true,
        .enable_interference = true,
        .seed = QREASON_DEFAULT_SEED
    };
}

//=============================================================================
// Random Number Generation
//=============================================================================

static inline uint32_t qreason_rand(uint32_t* state) {
    *state = (*state) * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static inline float qreason_randf(uint32_t* state) {
    return (float)qreason_rand(state) / 32767.0f;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create quantum reasoner
 * WHY:  Initialize reasoning engine with configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Reasoner context or NULL on error
 */
static inline qreason_t qreason_create(const qreason_config_t* config) {
    qreason_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = qreason_default_config();
    }

    qreason_internal_t* ctx = (qreason_internal_t*)nimcp_calloc(1, sizeof(qreason_internal_t));
    if (!ctx) return NULL;

    ctx->config = cfg;
    ctx->rng_state = cfg.seed;

    /* Initialize knowledge base */
    for (uint32_t i = 0; i < QREASON_MAX_VARIABLES; i++) {
        ctx->kb.facts[i] = QREASON_UNKNOWN;
        ctx->kb.confidences[i] = 0.0f;
    }

    return (qreason_t)ctx;
}

/**
 * WHAT: Destroy quantum reasoner
 */
static inline void qreason_destroy(qreason_t ctx) {
    nimcp_free(ctx);
}

//=============================================================================
// Knowledge Base Management
//=============================================================================

/**
 * WHAT: Set a fact in the knowledge base
 *
 * @param ctx Reasoner context
 * @param variable Variable index
 * @param value Truth value (TRUE, FALSE, UNKNOWN)
 * @param confidence Confidence [0, 1]
 * @return 0 on success
 */
static inline int qreason_set_fact(
    qreason_t ctx,
    uint32_t variable,
    qreason_truth_t value,
    float confidence
) {
    if (!ctx || variable >= QREASON_MAX_VARIABLES) return -1;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    internal->kb.facts[variable] = value;
    internal->kb.confidences[variable] = confidence;

    if (variable >= internal->kb.n_variables) {
        internal->kb.n_variables = variable + 1;
    }

    return 0;
}

/**
 * WHAT: Get a fact from the knowledge base
 */
static inline qreason_truth_t qreason_get_fact(
    qreason_t ctx,
    uint32_t variable,
    float* confidence_out
) {
    if (!ctx || variable >= QREASON_MAX_VARIABLES) {
        if (confidence_out) *confidence_out = 0.0f;
        return QREASON_UNKNOWN;
    }
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    if (confidence_out) {
        *confidence_out = internal->kb.confidences[variable];
    }
    return internal->kb.facts[variable];
}

/**
 * WHAT: Add an inference rule
 *
 * @param ctx Reasoner context
 * @param antecedents Variable indices for antecedents
 * @param n_antecedents Number of antecedents
 * @param consequent Consequent variable
 * @param confidence Rule confidence
 * @return 0 on success
 */
static inline int qreason_add_rule(
    qreason_t ctx,
    const uint32_t* antecedents,
    uint32_t n_antecedents,
    uint32_t consequent,
    float confidence
) {
    if (!ctx || n_antecedents > QREASON_MAX_LITERALS) return -1;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    if (internal->kb.n_rules >= QREASON_MAX_RULES) return -2;

    qreason_rule_t* rule = &internal->kb.rules[internal->kb.n_rules];
    rule->n_antecedents = n_antecedents;
    rule->consequent = consequent;
    rule->confidence = confidence;

    for (uint32_t i = 0; i < n_antecedents; i++) {
        rule->antecedents[i] = antecedents[i];
    }

    internal->kb.n_rules++;
    return 0;
}

/**
 * WHAT: Clear all facts
 */
static inline void qreason_clear_facts(qreason_t ctx) {
    if (!ctx) return;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    for (uint32_t i = 0; i < QREASON_MAX_VARIABLES; i++) {
        internal->kb.facts[i] = QREASON_UNKNOWN;
        internal->kb.confidences[i] = 0.0f;
    }
    internal->kb.n_variables = 0;
}

/**
 * WHAT: Clear all rules
 */
static inline void qreason_clear_rules(qreason_t ctx) {
    if (!ctx) return;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;
    internal->kb.n_rules = 0;
}

//=============================================================================
// Ternary Logic Operations
//=============================================================================

/**
 * WHAT: Kleene 3-valued AND
 */
static inline qreason_truth_t qreason_and(qreason_truth_t a, qreason_truth_t b) {
    if (a == QREASON_FALSE || b == QREASON_FALSE) return QREASON_FALSE;
    if (a == QREASON_UNKNOWN || b == QREASON_UNKNOWN) return QREASON_UNKNOWN;
    return QREASON_TRUE;
}

/**
 * WHAT: Kleene 3-valued OR
 */
static inline qreason_truth_t qreason_or(qreason_truth_t a, qreason_truth_t b) {
    if (a == QREASON_TRUE || b == QREASON_TRUE) return QREASON_TRUE;
    if (a == QREASON_UNKNOWN || b == QREASON_UNKNOWN) return QREASON_UNKNOWN;
    return QREASON_FALSE;
}

/**
 * WHAT: Kleene 3-valued NOT
 */
static inline qreason_truth_t qreason_not(qreason_truth_t a) {
    if (a == QREASON_TRUE) return QREASON_FALSE;
    if (a == QREASON_FALSE) return QREASON_TRUE;
    return QREASON_UNKNOWN;
}

/**
 * WHAT: Kleene 3-valued IMPLIES
 */
static inline qreason_truth_t qreason_implies(qreason_truth_t a, qreason_truth_t b) {
    return qreason_or(qreason_not(a), b);
}

//=============================================================================
// Quantum State Operations
//=============================================================================

/**
 * WHAT: Initialize quantum state to uniform superposition
 */
static inline void qreason_qstate_init(qreason_qstate_t* qstate, uint32_t n_vars) {
    if (n_vars > 16) n_vars = 16;  /* Limit to 16 qubits */

    qstate->n_qubits = n_vars;
    qstate->state_dim = 1u << n_vars;

    float uniform = 1.0f / sqrtf((float)qstate->state_dim);
    for (uint32_t i = 0; i < qstate->state_dim; i++) {
        qstate->amplitudes[i] = uniform;
    }
}

/**
 * WHAT: Apply oracle for CNF formula
 * WHY:  Mark satisfying assignments with phase flip
 */
static inline void qreason_oracle_cnf(
    qreason_qstate_t* qstate,
    const qreason_cnf_t* cnf
) {
    for (uint32_t state = 0; state < qstate->state_dim; state++) {
        bool satisfies = true;

        /* Check each clause */
        for (uint32_t c = 0; c < cnf->n_clauses && satisfies; c++) {
            bool clause_sat = false;

            for (uint32_t l = 0; l < cnf->clauses[c].n_literals; l++) {
                uint32_t var = cnf->clauses[c].literals[l].variable;
                bool negated = cnf->clauses[c].literals[l].negated;
                bool var_true = (state >> var) & 1;

                if (negated ? !var_true : var_true) {
                    clause_sat = true;
                    break;
                }
            }

            if (!clause_sat) {
                satisfies = false;
            }
        }

        if (satisfies) {
            qstate->amplitudes[state] *= -1.0f;  /* Phase flip */
        }
    }
}

/**
 * WHAT: Apply diffusion operator (inversion about mean)
 */
static inline void qreason_diffusion(qreason_qstate_t* qstate) {
    /* Compute mean */
    float mean = 0.0f;
    for (uint32_t i = 0; i < qstate->state_dim; i++) {
        mean += qstate->amplitudes[i];
    }
    mean /= (float)qstate->state_dim;

    /* Invert about mean */
    for (uint32_t i = 0; i < qstate->state_dim; i++) {
        qstate->amplitudes[i] = 2.0f * mean - qstate->amplitudes[i];
    }
}

/**
 * WHAT: Measure quantum state (find highest probability assignment)
 */
static inline uint32_t qreason_measure(const qreason_qstate_t* qstate) {
    float max_prob = 0.0f;
    uint32_t best_state = 0;

    for (uint32_t i = 0; i < qstate->state_dim; i++) {
        float prob = qstate->amplitudes[i] * qstate->amplitudes[i];
        if (prob > max_prob) {
            max_prob = prob;
            best_state = i;
        }
    }

    return best_state;
}

/**
 * WHAT: Get probability of satisfying assignment
 */
static inline float qreason_satisfaction_probability(
    const qreason_qstate_t* qstate,
    const qreason_cnf_t* cnf
) {
    float prob = 0.0f;

    for (uint32_t state = 0; state < qstate->state_dim; state++) {
        bool satisfies = true;

        for (uint32_t c = 0; c < cnf->n_clauses && satisfies; c++) {
            bool clause_sat = false;

            for (uint32_t l = 0; l < cnf->clauses[c].n_literals; l++) {
                uint32_t var = cnf->clauses[c].literals[l].variable;
                bool negated = cnf->clauses[c].literals[l].negated;
                bool var_true = (state >> var) & 1;

                if (negated ? !var_true : var_true) {
                    clause_sat = true;
                    break;
                }
            }

            if (!clause_sat) {
                satisfies = false;
            }
        }

        if (satisfies) {
            prob += qstate->amplitudes[state] * qstate->amplitudes[state];
        }
    }

    return prob;
}

//=============================================================================
// Classical DPLL SAT Solver (Fallback)
//=============================================================================

/**
 * WHAT: Internal state for DPLL solver
 */
typedef struct {
    qreason_truth_t assignment[QREASON_MAX_VARIABLES];
    bool assigned[QREASON_MAX_VARIABLES];
    uint32_t decision_level[QREASON_MAX_VARIABLES];
    uint32_t n_variables;
    uint32_t decisions;
    uint32_t propagations;
} qreason_dpll_state_t;

/**
 * WHAT: Check if a clause is satisfied under current assignment
 * @return 1 if satisfied, 0 if unsatisfied, -1 if unit, -2 if unresolved
 */
static inline int qreason_dpll_clause_status(
    const qreason_clause_t* clause,
    const qreason_dpll_state_t* state,
    uint32_t* unit_var,
    bool* unit_negated
) {
    uint32_t unassigned_count = 0;
    uint32_t last_unassigned_idx = 0;

    for (uint32_t i = 0; i < clause->n_literals; i++) {
        uint32_t var = clause->literals[i].variable;
        bool negated = clause->literals[i].negated;

        if (!state->assigned[var]) {
            unassigned_count++;
            last_unassigned_idx = i;
        } else {
            /* Check if this literal satisfies the clause */
            bool var_true = (state->assignment[var] == QREASON_TRUE);
            if (negated ? !var_true : var_true) {
                return 1;  /* Satisfied */
            }
        }
    }

    if (unassigned_count == 0) {
        return 0;  /* Unsatisfied (conflict) */
    } else if (unassigned_count == 1) {
        /* Unit clause - return the unit literal info */
        if (unit_var) {
            *unit_var = clause->literals[last_unassigned_idx].variable;
        }
        if (unit_negated) {
            *unit_negated = clause->literals[last_unassigned_idx].negated;
        }
        return -1;  /* Unit */
    }

    return -2;  /* Unresolved (multiple unassigned) */
}

/**
 * WHAT: Perform unit propagation (Boolean Constraint Propagation)
 * WHY:  Forced assignments from unit clauses
 *
 * @return true if no conflict, false if conflict detected
 */
static inline bool qreason_dpll_unit_propagate(
    const qreason_cnf_t* cnf,
    qreason_dpll_state_t* state,
    uint32_t decision_level
) {
    bool changed = true;

    while (changed) {
        changed = false;

        for (uint32_t c = 0; c < cnf->n_clauses; c++) {
            uint32_t unit_var;
            bool unit_negated;
            int status = qreason_dpll_clause_status(
                &cnf->clauses[c], state, &unit_var, &unit_negated
            );

            if (status == 0) {
                /* Conflict - clause is falsified */
                return false;
            } else if (status == -1) {
                /* Unit clause - propagate */
                state->assignment[unit_var] = unit_negated ? QREASON_FALSE : QREASON_TRUE;
                state->assigned[unit_var] = true;
                state->decision_level[unit_var] = decision_level;
                state->propagations++;
                changed = true;
            }
        }
    }

    return true;
}

/**
 * WHAT: Check if all clauses are satisfied
 */
static inline bool qreason_dpll_all_satisfied(
    const qreason_cnf_t* cnf,
    const qreason_dpll_state_t* state
) {
    for (uint32_t c = 0; c < cnf->n_clauses; c++) {
        int status = qreason_dpll_clause_status(&cnf->clauses[c], state, NULL, NULL);
        if (status != 1) {
            return false;
        }
    }
    return true;
}

/**
 * WHAT: Find the next unassigned variable for branching
 * HOW:  Simple heuristic - pick first unassigned variable
 *
 * @return Variable index, or UINT32_MAX if all assigned
 */
static inline uint32_t qreason_dpll_pick_variable(
    const qreason_cnf_t* cnf,
    const qreason_dpll_state_t* state
) {
    /* Simple heuristic: first unassigned variable */
    for (uint32_t v = 0; v < cnf->n_variables; v++) {
        if (!state->assigned[v]) {
            return v;
        }
    }
    return UINT32_MAX;
}

/**
 * WHAT: Recursive DPLL solver core
 */
static inline bool qreason_dpll_solve_recursive(
    const qreason_cnf_t* cnf,
    qreason_dpll_state_t* state,
    uint32_t decision_level,
    uint32_t max_decisions
) {
    /* Check decision limit to avoid infinite loops on hard instances */
    if (state->decisions > max_decisions) {
        return false;
    }

    /* Unit propagation */
    if (!qreason_dpll_unit_propagate(cnf, state, decision_level)) {
        return false;  /* Conflict */
    }

    /* Check if all clauses are satisfied */
    if (qreason_dpll_all_satisfied(cnf, state)) {
        return true;
    }

    /* Pick a variable to branch on */
    uint32_t branch_var = qreason_dpll_pick_variable(cnf, state);
    if (branch_var == UINT32_MAX) {
        /* All variables assigned but not all satisfied = conflict */
        return false;
    }

    state->decisions++;

    /* Try assigning TRUE first */
    qreason_dpll_state_t saved_state = *state;

    state->assignment[branch_var] = QREASON_TRUE;
    state->assigned[branch_var] = true;
    state->decision_level[branch_var] = decision_level + 1;

    if (qreason_dpll_solve_recursive(cnf, state, decision_level + 1, max_decisions)) {
        return true;
    }

    /* Backtrack and try FALSE */
    *state = saved_state;
    state->decisions++;  /* Count backtrack as another decision */

    state->assignment[branch_var] = QREASON_FALSE;
    state->assigned[branch_var] = true;
    state->decision_level[branch_var] = decision_level + 1;

    return qreason_dpll_solve_recursive(cnf, state, decision_level + 1, max_decisions);
}

/**
 * WHAT: Classical DPLL SAT solver (fallback for quantum solver)
 * WHY:  Provides solution when quantum solver cannot handle the problem
 *
 * ALGORITHM: DPLL with unit propagation
 * - Unit propagation forces assignments from unit clauses
 * - Backtracking search explores assignment space
 * - Simple variable selection heuristic
 *
 * @param cnf CNF formula to solve
 * @param result Output: result with satisfying assignment
 * @return 0 on success, -1 on error
 */
static inline int qreason_classical_solve_sat(
    const qreason_cnf_t* cnf,
    qreason_result_t* result
) {
    if (!cnf || !result) return -1;

    /* Initialize result */
    memset(result, 0, sizeof(qreason_result_t));
    result->method = QREASON_METHOD_CLASSICAL;

    /* Handle trivial cases */
    if (cnf->n_variables == 0 || cnf->n_clauses == 0) {
        result->satisfiable = true;
        result->satisfaction_prob = 1.0f;
        return 0;
    }

    /* Check for empty clauses (immediately unsatisfiable) */
    for (uint32_t c = 0; c < cnf->n_clauses; c++) {
        if (cnf->clauses[c].n_literals == 0) {
            result->satisfiable = false;
            result->satisfaction_prob = 0.0f;
            return 0;
        }
    }

    /* Initialize DPLL state */
    qreason_dpll_state_t state;
    memset(&state, 0, sizeof(state));
    state.n_variables = cnf->n_variables;

    /* Set all assignments to UNKNOWN initially */
    for (uint32_t v = 0; v < QREASON_MAX_VARIABLES; v++) {
        state.assignment[v] = QREASON_UNKNOWN;
    }

    /* Max decisions: limit to prevent exponential blowup on hard instances */
    uint32_t max_decisions = 100000;

    /* Run DPLL */
    bool sat = qreason_dpll_solve_recursive(cnf, &state, 0, max_decisions);

    /* Copy results */
    result->satisfiable = sat;
    result->satisfaction_prob = sat ? 1.0f : 0.0f;
    result->dpll_decisions = state.decisions;
    result->unit_propagations = state.propagations;
    result->grover_iterations = 0;

    if (sat) {
        for (uint32_t v = 0; v < cnf->n_variables; v++) {
            result->assignment[v] = state.assignment[v];
            /* Classical solver provides definite answer */
            result->confidences[v] = 1.0f;
        }
    }

    return 0;
}

//=============================================================================
// Core Reasoning Functions
//=============================================================================

/**
 * WHAT: Run forward chaining inference
 * WHY:  Derive new facts from rules
 *
 * @param ctx Reasoner context
 * @param result Output: inference result
 * @return Number of new facts derived
 */
static inline uint32_t qreason_forward_chain(
    qreason_t ctx,
    qreason_result_t* result
) {
    if (!ctx || !result) return 0;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    uint32_t new_facts = 0;
    bool changed = true;
    uint32_t depth = 0;

    while (changed && depth < internal->config.max_inference_depth) {
        changed = false;
        depth++;

        for (uint32_t r = 0; r < internal->kb.n_rules; r++) {
            qreason_rule_t* rule = &internal->kb.rules[r];

            /* Check if all antecedents are true */
            bool all_true = true;
            float min_conf = 1.0f;

            for (uint32_t a = 0; a < rule->n_antecedents; a++) {
                qreason_truth_t val = internal->kb.facts[rule->antecedents[a]];
                float conf = internal->kb.confidences[rule->antecedents[a]];

                if (val != QREASON_TRUE) {
                    all_true = false;
                    break;
                }
                if (conf < min_conf) {
                    min_conf = conf;
                }
            }

            /* If all antecedents true and consequent unknown, derive */
            if (all_true && internal->kb.facts[rule->consequent] == QREASON_UNKNOWN) {
                float new_conf = min_conf * rule->confidence;

                if (new_conf >= internal->config.min_confidence) {
                    internal->kb.facts[rule->consequent] = QREASON_TRUE;
                    internal->kb.confidences[rule->consequent] = new_conf;
                    changed = true;
                    new_facts++;
                }
            }
        }
    }

    result->inferences_made = new_facts;
    return new_facts;
}

/**
 * WHAT: Check if a state (bit pattern) satisfies a CNF formula
 * WHY:  Verify measurement result or scan for satisfying assignments
 *
 * @param state Bit pattern representing variable assignment
 * @param cnf CNF formula
 * @return true if state satisfies formula
 */
static inline bool qreason_state_satisfies_cnf(
    uint32_t state,
    const qreason_cnf_t* cnf
) {
    for (uint32_t c = 0; c < cnf->n_clauses; c++) {
        bool clause_sat = false;

        for (uint32_t l = 0; l < cnf->clauses[c].n_literals; l++) {
            uint32_t var = cnf->clauses[c].literals[l].variable;
            bool negated = cnf->clauses[c].literals[l].negated;
            bool var_true = (state >> var) & 1;

            if (negated ? !var_true : var_true) {
                clause_sat = true;
                break;
            }
        }

        if (!clause_sat) {
            return false;
        }
    }
    return true;
}

/**
 * WHAT: Solve SAT using quantum (Grover-inspired) solver
 * WHY:  Internal function for quantum-only solving
 *
 * ALGORITHM:
 * 1. Initialize uniform superposition
 * 2. Compute optimal Grover iteration count: pi/4 * sqrt(N/M)
 * 3. Run Grover iterations (oracle + diffusion) when solutions are rare
 * 4. Measure highest-probability state
 * 5. Verify measurement satisfies CNF; if not, scan for satisfying state
 *
 * @param internal Internal context
 * @param cnf CNF formula
 * @param result Output: result with assignment
 * @return 0 on success, -1 on error
 */
static inline int qreason_quantum_solve_sat_internal(
    qreason_internal_t* internal,
    const qreason_cnf_t* cnf,
    qreason_result_t* result
) {
    /* Initialize quantum state */
    qreason_qstate_init(&internal->qstate, cnf->n_variables);

    /* Check initial satisfaction probability before Grover */
    float initial_prob = qreason_satisfaction_probability(&internal->qstate, cnf);

    /* Compute optimal Grover iteration count: pi/4 * sqrt(N/M)
     * where N = state_dim, M = estimated number of solutions.
     * Grover only helps when solutions are rare (< 50%).
     * When solutions are majority, skip Grover to preserve probability. */
    uint32_t iterations = 0;
    if (initial_prob > 0.0f && initial_prob < 0.5f) {
        float estimated_solutions = initial_prob * (float)internal->qstate.state_dim;
        float ratio = (float)internal->qstate.state_dim / estimated_solutions;
        uint32_t optimal = (uint32_t)(M_PI / 4.0 * sqrtf(ratio));
        if (optimal < 1) optimal = 1;

        /* Cap at max_grover_iterations if set, otherwise use reasonable limit */
        uint32_t max_iter = internal->config.max_grover_iterations;
        if (max_iter == 0) max_iter = 20;
        iterations = (optimal < max_iter) ? optimal : max_iter;

        /* Run Grover iterations */
        for (uint32_t i = 0; i < iterations; i++) {
            qreason_oracle_cnf(&internal->qstate, cnf);
            qreason_diffusion(&internal->qstate);
        }
    }

    /* Measure result - find highest probability state */
    uint32_t best_state = qreason_measure(&internal->qstate);

    /* Verify the measured state actually satisfies the CNF.
     * If not (due to uniform superposition or Grover oscillation),
     * scan states by probability to find a satisfying one. */
    if (!qreason_state_satisfies_cnf(best_state, cnf)) {
        /* Scan all states by descending probability for a satisfying one */
        float best_sat_prob = -1.0f;
        uint32_t best_sat_state = best_state;
        bool found_sat = false;

        for (uint32_t s = 0; s < internal->qstate.state_dim; s++) {
            if (qreason_state_satisfies_cnf(s, cnf)) {
                float prob = internal->qstate.amplitudes[s] *
                            internal->qstate.amplitudes[s];
                if (prob > best_sat_prob) {
                    best_sat_prob = prob;
                    best_sat_state = s;
                    found_sat = true;
                }
            }
        }

        if (found_sat) {
            best_state = best_sat_state;
        }
    }

    /* Extract assignment from best state */
    for (uint32_t v = 0; v < cnf->n_variables; v++) {
        bool val = (best_state >> v) & 1;
        result->assignment[v] = val ? QREASON_TRUE : QREASON_FALSE;
        result->confidences[v] = internal->qstate.amplitudes[best_state] *
                                internal->qstate.amplitudes[best_state];
    }

    /* Check if satisfiable */
    result->satisfaction_prob = qreason_satisfaction_probability(&internal->qstate, cnf);
    result->satisfiable = (result->satisfaction_prob > 0.0f);
    result->grover_iterations = iterations;
    result->method = QREASON_METHOD_QUANTUM;
    result->dpll_decisions = 0;
    result->unit_propagations = 0;

    return 0;
}

/**
 * WHAT: Solve SAT using Grover-inspired search with classical fallback
 * WHY:  Find satisfying assignment for CNF formula
 *
 * FALLBACK CONDITIONS:
 * 1. Variables exceed QREASON_QUANTUM_VAR_LIMIT (16) - quantum state too large
 * 2. Quantum satisfaction probability below QREASON_MIN_SAT_PROB threshold
 * 3. Quantum solver returns unsatisfiable - try classical for confirmation
 *
 * @param ctx Reasoner context
 * @param cnf CNF formula
 * @param result Output: result with assignment
 * @return 0 on success, -1 on error
 */
static inline int qreason_solve_sat(
    qreason_t ctx,
    const qreason_cnf_t* cnf,
    qreason_result_t* result
) {
    if (!ctx || !cnf || !result) return -1;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    /* Initialize result */
    memset(result, 0, sizeof(qreason_result_t));

    /* Handle trivial cases */
    if (cnf->n_variables == 0 || cnf->n_clauses == 0) {
        result->satisfiable = true;
        result->satisfaction_prob = 1.0f;
        result->method = QREASON_METHOD_QUANTUM;
        return 0;
    }

    /*
     * FALLBACK CONDITION 1: Variables exceed quantum limit
     * The quantum solver is limited to 16 qubits (65K states).
     * For larger problems, use classical DPLL directly.
     */
    if (cnf->n_variables > QREASON_QUANTUM_VAR_LIMIT) {
        int ret = qreason_classical_solve_sat(cnf, result);

        /* Update statistics */
        internal->queries_performed++;
        if (result->satisfiable) {
            internal->satisfiable_count++;
        } else {
            internal->unsatisfiable_count++;
        }

        return ret;
    }

    /* Try quantum solver first */
    int quantum_ret = qreason_quantum_solve_sat_internal(internal, cnf, result);

    if (quantum_ret != 0) {
        /*
         * FALLBACK CONDITION 2: Quantum solver returned error
         * Fall back to classical solver.
         */
        int ret = qreason_classical_solve_sat(cnf, result);

        /* Update statistics */
        internal->queries_performed++;
        if (result->satisfiable) {
            internal->satisfiable_count++;
        } else {
            internal->unsatisfiable_count++;
        }

        return ret;
    }

    /*
     * FALLBACK CONDITION 3: Quantum satisfaction probability below threshold
     * When quantum solver finds very low probability, the result may be unreliable.
     * Use classical solver for definitive answer.
     */
    if (result->satisfaction_prob < QREASON_MIN_SAT_PROB && !result->satisfiable) {
        qreason_result_t classical_result;
        int classical_ret = qreason_classical_solve_sat(cnf, &classical_result);

        if (classical_ret == 0) {
            /* Use classical result if it found a solution */
            if (classical_result.satisfiable) {
                *result = classical_result;
            }
            /* If classical also says unsatisfiable, keep quantum result but
             * update statistics - now we're confident it's truly unsatisfiable */
        }
    }

    /*
     * FALLBACK CONDITION 4: Quantum says unsatisfiable but we want confirmation
     * Classical DPLL can definitively prove unsatisfiability.
     * Only do this check for small formulas to avoid performance hit.
     */
    if (!result->satisfiable && cnf->n_variables <= 12 && cnf->n_clauses <= 32) {
        qreason_result_t classical_result;
        int classical_ret = qreason_classical_solve_sat(cnf, &classical_result);

        if (classical_ret == 0 && classical_result.satisfiable) {
            /* Classical found a solution that quantum missed! Use it. */
            *result = classical_result;
        }
    }

    /* Update statistics */
    internal->queries_performed++;
    internal->total_grover_iterations += result->grover_iterations;
    if (result->satisfiable) {
        internal->satisfiable_count++;
    } else {
        internal->unsatisfiable_count++;
    }

    return 0;
}

/**
 * WHAT: Query the knowledge base
 * WHY:  Determine truth value of a variable using inference
 *
 * @param ctx Reasoner context
 * @param variable Variable to query
 * @param result Output: result
 * @return 0 on success
 */
static inline int qreason_query(
    qreason_t ctx,
    uint32_t variable,
    qreason_result_t* result
) {
    if (!ctx || !result || variable >= QREASON_MAX_VARIABLES) return -1;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    /* First run forward chaining */
    qreason_forward_chain(ctx, result);

    /* Copy current knowledge base state to result */
    for (uint32_t i = 0; i < QREASON_MAX_VARIABLES; i++) {
        result->assignment[i] = internal->kb.facts[i];
        result->confidences[i] = internal->kb.confidences[i];
    }

    return 0;
}

/**
 * WHAT: Check consistency of knowledge base
 * WHY:  Detect contradictions
 *
 * @param ctx Reasoner context
 * @return true if consistent, false if contradictory
 */
static inline bool qreason_check_consistency(qreason_t ctx) {
    if (!ctx) return false;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    /* Check for any variable with conflicting derivations */
    /* In ternary logic, a variable can be TRUE or FALSE but not both */

    /* Simple check: run forward chaining and look for conflicts */
    /* This is a placeholder - full consistency checking would use resolution */

    for (uint32_t i = 0; i < internal->kb.n_variables; i++) {
        /* A contradiction would be a variable that derives both TRUE and FALSE */
        /* For now, just verify no UNKNOWN variables have confidence > 0 */
        if (internal->kb.facts[i] == QREASON_UNKNOWN &&
            internal->kb.confidences[i] > 0.5f) {
            return false;  /* Inconsistent */
        }
    }

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Create a CNF clause
 */
static inline qreason_clause_t qreason_make_clause(
    const int* literals,  /* Positive = variable, negative = NOT variable */
    uint32_t n_literals
) {
    qreason_clause_t clause = {0};
    clause.n_literals = (n_literals < QREASON_MAX_LITERALS) ?
                        n_literals : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < clause.n_literals; i++) {
        if (literals[i] < 0) {
            clause.literals[i].variable = (uint32_t)(-literals[i] - 1);
            clause.literals[i].negated = true;
        } else {
            clause.literals[i].variable = (uint32_t)(literals[i] - 1);
            clause.literals[i].negated = false;
        }
    }

    return clause;
}

/**
 * WHAT: Get statistics
 */
static inline int qreason_get_stats(qreason_t ctx, qreason_stats_t* stats) {
    if (!ctx || !stats) return -1;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;

    stats->queries_performed = internal->queries_performed;
    stats->satisfiable_count = internal->satisfiable_count;
    stats->unsatisfiable_count = internal->unsatisfiable_count;

    if (internal->queries_performed > 0) {
        stats->satisfiability_rate = (float)internal->satisfiable_count /
                                    (float)internal->queries_performed;
        stats->avg_grover_iterations = (float)internal->total_grover_iterations /
                                       (float)internal->queries_performed;
    } else {
        stats->satisfiability_rate = 0.0f;
        stats->avg_grover_iterations = 0.0f;
    }

    return 0;
}

/**
 * WHAT: Get configuration
 */
static inline int qreason_get_config(qreason_t ctx, qreason_config_t* config) {
    if (!ctx || !config) return -1;
    qreason_internal_t* internal = (qreason_internal_t*)ctx;
    *config = internal->config;
    return 0;
}

//=============================================================================
// Ternary Belief Propagation API
//=============================================================================

/**
 * @brief Ternary belief state for probabilistic reasoning
 *
 * WHAT: Discrete belief states for efficient belief propagation
 * WHY:  Simplifies continuous probabilities to actionable decision states
 * HOW:  Maps to believe/uncertain/disbelieve for logical inference
 *
 * BIOLOGICAL GROUNDING:
 * - Prefrontal cortex forms discrete belief categories
 * - Decision-making uses categorical rather than continuous judgments
 * - Confidence thresholds trigger different neural responses
 *
 * LOGICAL CORRESPONDENCE:
 * - BELIEVE (+1) corresponds to Kleene TRUE
 * - DISBELIEVE (-1) corresponds to Kleene FALSE
 * - UNCERTAIN (0) corresponds to Kleene UNKNOWN
 */
typedef enum {
    QREASON_BELIEF_DISBELIEVE = -1,  /**< Strong disbelief (p < low_threshold) */
    QREASON_BELIEF_UNCERTAIN = 0,    /**< Uncertain (between thresholds) */
    QREASON_BELIEF_BELIEVE = 1       /**< Strong belief (p > high_threshold) */
} ternary_belief_t;

/**
 * @brief Ternary belief matrix for weight-based inference
 *
 * WHAT: Matrix of ternary beliefs for structured reasoning
 * WHY:  Extend Kleene logic to weight matrices for graph-based inference
 * HOW:  Each connection has a ternary strength: support/neutral/inhibit
 */
typedef struct {
    ternary_belief_t* data;          /**< Flattened ternary matrix */
    uint32_t rows;                   /**< Number of rows (from variables) */
    uint32_t cols;                   /**< Number of columns (to variables) */
    float* confidences;              /**< Optional confidence per edge */
} ternary_belief_matrix_t;

/**
 * @brief Ternary belief propagation configuration
 *
 * WHAT: Configuration for ternary belief propagation
 * WHY:  Customize belief discretization and propagation behavior
 */
typedef struct {
    float believe_threshold;         /**< Probability >= this is BELIEVE (default: 0.7) */
    float disbelieve_threshold;      /**< Probability <= this is DISBELIEVE (default: 0.3) */
    uint32_t max_iterations;         /**< Max propagation iterations (default: 20) */
    float damping_factor;            /**< Message damping for convergence (default: 0.5) */
    bool use_ternary_messages;       /**< Send ternary instead of continuous messages */
    bool track_convergence;          /**< Track convergence statistics */
    float convergence_threshold;     /**< Convergence check threshold (default: 0.01) */
} ternary_bp_config_t;

/**
 * @brief Ternary belief propagation statistics
 *
 * WHAT: Statistics for ternary belief propagation
 * WHY:  Monitor convergence and belief distribution
 */
typedef struct {
    uint32_t iterations_performed;   /**< Iterations until convergence */
    bool converged;                  /**< Whether propagation converged */
    uint32_t believe_count;          /**< Variables in BELIEVE state */
    uint32_t uncertain_count;        /**< Variables in UNCERTAIN state */
    uint32_t disbelieve_count;       /**< Variables in DISBELIEVE state */
    uint32_t state_changes;          /**< Total state changes during propagation */
    float avg_confidence;            /**< Average confidence of beliefs */
} ternary_bp_stats_t;

/**
 * @brief Get default ternary belief propagation configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Easy setup for ternary belief propagation
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
static inline int ternary_bp_default_config(ternary_bp_config_t* config) {
    if (!config) return -1;

    config->believe_threshold = 0.7f;
    config->disbelieve_threshold = 0.3f;
    config->max_iterations = 20;
    config->damping_factor = 0.5f;
    config->use_ternary_messages = true;
    config->track_convergence = true;
    config->convergence_threshold = 0.01f;

    return 0;
}

/**
 * @brief Convert probability to ternary belief
 *
 * WHAT: Discretize continuous probability to ternary belief state
 * WHY:  Simplify probabilistic reasoning to categorical decisions
 *
 * @param config Configuration with thresholds (NULL for defaults)
 * @param probability Continuous probability [0,1]
 * @return Ternary belief state
 */
static inline ternary_belief_t ternary_belief_from_prob(
    const ternary_bp_config_t* config,
    float probability
) {
    float believe_thresh = config ? config->believe_threshold : 0.7f;
    float disbelieve_thresh = config ? config->disbelieve_threshold : 0.3f;

    if (probability >= believe_thresh) {
        return QREASON_BELIEF_BELIEVE;
    }
    if (probability <= disbelieve_thresh) {
        return QREASON_BELIEF_DISBELIEVE;
    }
    return QREASON_BELIEF_UNCERTAIN;
}

/**
 * @brief Convert ternary belief to representative probability
 *
 * WHAT: Get typical probability for belief state
 * WHY:  Interface with systems expecting continuous probabilities
 *
 * @param belief Ternary belief state
 * @return Representative probability
 */
static inline float ternary_belief_to_prob(ternary_belief_t belief) {
    switch (belief) {
        case QREASON_BELIEF_BELIEVE:    return 0.9f;
        case QREASON_BELIEF_DISBELIEVE: return 0.1f;
        default:                        return 0.5f;
    }
}

/**
 * @brief Ternary belief AND operation
 *
 * WHAT: Combine beliefs with AND semantics
 * WHY:  Logical conjunction for ternary beliefs
 * HOW:  Minimum operation (Kleene AND semantics)
 *
 * @param a First belief
 * @param b Second belief
 * @return Combined belief
 */
static inline ternary_belief_t ternary_belief_and(ternary_belief_t a, ternary_belief_t b) {
    /* Kleene AND: min(a, b) in ternary space */
    if (a == QREASON_BELIEF_DISBELIEVE || b == QREASON_BELIEF_DISBELIEVE) {
        return QREASON_BELIEF_DISBELIEVE;
    }
    if (a == QREASON_BELIEF_UNCERTAIN || b == QREASON_BELIEF_UNCERTAIN) {
        return QREASON_BELIEF_UNCERTAIN;
    }
    return QREASON_BELIEF_BELIEVE;
}

/**
 * @brief Ternary belief OR operation
 *
 * WHAT: Combine beliefs with OR semantics
 * WHY:  Logical disjunction for ternary beliefs
 * HOW:  Maximum operation (Kleene OR semantics)
 *
 * @param a First belief
 * @param b Second belief
 * @return Combined belief
 */
static inline ternary_belief_t ternary_belief_or(ternary_belief_t a, ternary_belief_t b) {
    /* Kleene OR: max(a, b) in ternary space */
    if (a == QREASON_BELIEF_BELIEVE || b == QREASON_BELIEF_BELIEVE) {
        return QREASON_BELIEF_BELIEVE;
    }
    if (a == QREASON_BELIEF_UNCERTAIN || b == QREASON_BELIEF_UNCERTAIN) {
        return QREASON_BELIEF_UNCERTAIN;
    }
    return QREASON_BELIEF_DISBELIEVE;
}

/**
 * @brief Ternary belief NOT operation
 *
 * WHAT: Negate ternary belief
 * WHY:  Logical negation for ternary beliefs
 *
 * @param belief Belief to negate
 * @return Negated belief
 */
static inline ternary_belief_t ternary_belief_not(ternary_belief_t belief) {
    switch (belief) {
        case QREASON_BELIEF_BELIEVE:    return QREASON_BELIEF_DISBELIEVE;
        case QREASON_BELIEF_DISBELIEVE: return QREASON_BELIEF_BELIEVE;
        default:                        return QREASON_BELIEF_UNCERTAIN;
    }
}

/**
 * @brief Create ternary belief matrix
 *
 * WHAT: Allocate and initialize ternary belief matrix
 * WHY:  Represent inference graph with ternary edge weights
 *
 * @param rows Number of rows
 * @param cols Number of columns
 * @param with_confidences Whether to track per-edge confidence
 * @return New matrix or NULL on failure
 */
static inline ternary_belief_matrix_t* ternary_belief_matrix_create(
    uint32_t rows,
    uint32_t cols,
    bool with_confidences
) {
    ternary_belief_matrix_t* matrix = (ternary_belief_matrix_t*)nimcp_calloc(
        1, sizeof(ternary_belief_matrix_t)
    );
    if (!matrix) return NULL;

    size_t size = (size_t)rows * cols;
    matrix->data = (ternary_belief_t*)nimcp_calloc(size, sizeof(ternary_belief_t));
    if (!matrix->data) {
        nimcp_free(matrix);
        return NULL;
    }

    if (with_confidences) {
        matrix->confidences = (float*)nimcp_calloc(size, sizeof(float));
        if (!matrix->confidences) {
            nimcp_free(matrix->data);
            nimcp_free(matrix);
            return NULL;
        }
        /* Initialize confidences to 0.5 (neutral) */
        for (size_t i = 0; i < size; i++) {
            matrix->confidences[i] = 0.5f;
        }
    }

    matrix->rows = rows;
    matrix->cols = cols;

    return matrix;
}

/**
 * @brief Destroy ternary belief matrix
 *
 * @param matrix Matrix to destroy (NULL-safe)
 */
static inline void ternary_belief_matrix_destroy(ternary_belief_matrix_t* matrix) {
    if (!matrix) return;
    nimcp_free(matrix->data);
    nimcp_free(matrix->confidences);
    nimcp_free(matrix);
}

/**
 * @brief Get belief from matrix
 *
 * @param matrix Belief matrix
 * @param row Row index
 * @param col Column index
 * @return Belief value
 */
static inline ternary_belief_t ternary_belief_matrix_get(
    const ternary_belief_matrix_t* matrix,
    uint32_t row,
    uint32_t col
) {
    if (!matrix || row >= matrix->rows || col >= matrix->cols) {
        return QREASON_BELIEF_UNCERTAIN;
    }
    return matrix->data[row * matrix->cols + col];
}

/**
 * @brief Set belief in matrix
 *
 * @param matrix Belief matrix
 * @param row Row index
 * @param col Column index
 * @param belief Belief value
 * @param confidence Optional confidence (if matrix has confidences)
 */
static inline void ternary_belief_matrix_set(
    ternary_belief_matrix_t* matrix,
    uint32_t row,
    uint32_t col,
    ternary_belief_t belief,
    float confidence
) {
    if (!matrix || row >= matrix->rows || col >= matrix->cols) return;
    size_t idx = row * matrix->cols + col;
    matrix->data[idx] = belief;
    if (matrix->confidences) {
        matrix->confidences[idx] = confidence;
    }
}

/**
 * @brief Enable ternary belief propagation mode
 *
 * WHAT: Enable ternary belief propagation in quantum reasoner
 * WHY:  Combine quantum-inspired reasoning with ternary beliefs
 *
 * @param ctx Quantum reasoner context
 * @param config Ternary BP configuration (NULL for defaults)
 * @return 0 on success
 */
int qreason_enable_ternary_bp(
    qreason_t ctx,
    const ternary_bp_config_t* config
);

/**
 * @brief Disable ternary belief propagation mode
 *
 * @param ctx Quantum reasoner context
 * @return 0 on success
 */
int qreason_disable_ternary_bp(qreason_t ctx);

/**
 * @brief Check if ternary BP is enabled
 *
 * @param ctx Quantum reasoner context
 * @return true if ternary BP active
 */
bool qreason_is_ternary_bp(const qreason_t ctx);

/**
 * @brief Run ternary belief propagation
 *
 * WHAT: Propagate beliefs through factor graph using ternary messages
 * WHY:  Efficient approximate inference with discrete beliefs
 * HOW:  Sum-product algorithm with ternary discretization
 *
 * @param ctx Quantum reasoner context
 * @param weights Ternary belief matrix (edge weights)
 * @param initial_beliefs Initial beliefs for variables (NULL for UNCERTAIN)
 * @param result Output: propagated beliefs
 * @return 0 on success
 */
int qreason_ternary_bp_propagate(
    qreason_t ctx,
    const ternary_belief_matrix_t* weights,
    const ternary_belief_t* initial_beliefs,
    qreason_result_t* result
);

/**
 * @brief Query with ternary belief propagation
 *
 * WHAT: Query variable belief using ternary propagation
 * WHY:  Get ternary belief state for variable
 *
 * @param ctx Quantum reasoner context
 * @param variable Variable index to query
 * @param belief_out Output: ternary belief
 * @param confidence_out Output: confidence [0,1]
 * @return 0 on success
 */
int qreason_ternary_bp_query(
    qreason_t ctx,
    uint32_t variable,
    ternary_belief_t* belief_out,
    float* confidence_out
);

/**
 * @brief Convert knowledge base to ternary belief matrix
 *
 * WHAT: Build belief matrix from rules in knowledge base
 * WHY:  Enable ternary BP on existing knowledge base
 *
 * @param ctx Quantum reasoner context
 * @param matrix Output: ternary belief matrix (caller must destroy)
 * @return 0 on success
 */
int qreason_kb_to_ternary_matrix(
    qreason_t ctx,
    ternary_belief_matrix_t** matrix
);

/**
 * @brief Apply ternary message passing
 *
 * WHAT: Single round of ternary message passing
 * WHY:  Low-level control over propagation
 *
 * @param ctx Quantum reasoner context
 * @param weights Belief matrix
 * @param beliefs Current beliefs (modified in place)
 * @param num_variables Number of variables
 * @return Number of belief changes
 */
uint32_t qreason_ternary_message_pass(
    qreason_t ctx,
    const ternary_belief_matrix_t* weights,
    ternary_belief_t* beliefs,
    uint32_t num_variables
);

/**
 * @brief Get ternary belief propagation statistics
 *
 * @param ctx Quantum reasoner context
 * @param stats Output statistics
 * @return 0 on success
 */
int qreason_get_ternary_bp_stats(
    const qreason_t ctx,
    ternary_bp_stats_t* stats
);

/**
 * @brief Map quantum amplitude to ternary belief
 *
 * WHAT: Convert quantum amplitude (probability) to ternary belief
 * WHY:  Bridge quantum and ternary representations
 *
 * @param amplitude Quantum amplitude
 * @return Ternary belief
 */
static inline ternary_belief_t qreason_amplitude_to_belief(float amplitude) {
    float prob = amplitude * amplitude;  /* |amplitude|^2 = probability */
    if (prob >= 0.7f) return QREASON_BELIEF_BELIEVE;
    if (prob <= 0.3f) return QREASON_BELIEF_DISBELIEVE;
    return QREASON_BELIEF_UNCERTAIN;
}

/**
 * @brief Map ternary belief to quantum amplitude
 *
 * WHAT: Convert ternary belief to quantum amplitude
 * WHY:  Initialize quantum state from beliefs
 *
 * @param belief Ternary belief
 * @return Representative amplitude
 */
static inline float qreason_belief_to_amplitude(ternary_belief_t belief) {
    switch (belief) {
        case QREASON_BELIEF_BELIEVE:    return 0.95f;   /* sqrt(0.9) approx */
        case QREASON_BELIEF_DISBELIEVE: return 0.32f;   /* sqrt(0.1) approx */
        default:                        return 0.71f;   /* sqrt(0.5) approx */
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_REASONING_H */
