/**
 * @file nimcp_symbolic_logic.h
 * @brief Biologically-inspired symbolic reasoning system
 *
 * WHAT: Symbolic logic engine with inference and knowledge representation
 * WHY:  Enable logical reasoning and symbolic manipulation in NIMCP
 * HOW:  First-order logic with resolution-based inference
 *
 * Mimics biological reasoning processes:
 * - Prefrontal Cortex: Abstract reasoning and planning
 * - Hippocampus: Fact storage and retrieval
 * - Working Memory: Active inference and unification
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_H
#define NIMCP_SYMBOLIC_LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

#define LOGIC_MAX_PREDICATES 1000
#define LOGIC_MAX_VARIABLES 26        // a-z
#define LOGIC_MAX_CONSTANTS 100
#define LOGIC_MAX_RULES 500
#define LOGIC_MAX_CLAUSE_LENGTH 16
#define LOGIC_MAX_NAME_LENGTH 64
#define LOGIC_MAX_ARITY 4             // Max predicate arguments

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Logical operator types
 */
typedef enum {
    OP_AND,          ///< Conjunction (∧)
    OP_OR,           ///< Disjunction (∨)
    OP_NOT,          ///< Negation (¬)
    OP_IMPLIES,      ///< Implication (→)
    OP_IFF,          ///< Biconditional (↔)
    OP_FORALL,       ///< Universal quantifier (∀)
    OP_EXISTS        ///< Existential quantifier (∃)
} logical_operator_t;

/**
 * @brief Term types
 */
typedef enum {
    TERM_VARIABLE,   ///< Variable (e.g., x, y)
    TERM_CONSTANT,   ///< Constant (e.g., john, 5)
    TERM_FUNCTION    ///< Function (e.g., f(x))
} term_type_t;

/**
 * @brief Logical term
 */
typedef struct logical_term {
    term_type_t type;
    char name[LOGIC_MAX_NAME_LENGTH];
    struct logical_term** args;   ///< For functions
    uint8_t arity;                ///< Number of arguments
} logical_term_t;

/**
 * @brief Atomic formula (predicate)
 */
typedef struct {
    char name[LOGIC_MAX_NAME_LENGTH];
    logical_term_t** terms;       ///< Predicate arguments
    uint8_t arity;                ///< Number of arguments
    bool negated;                 ///< True if ¬P
} atomic_formula_t;

/**
 * @brief Logical formula (tree structure)
 */
typedef struct logical_formula {
    logical_operator_t op;
    atomic_formula_t* atom;       ///< Non-NULL if atomic
    struct logical_formula* left;
    struct logical_formula* right;
    logical_term_t* quantified_var; ///< For quantifiers
} logical_formula_t;

/**
 * @brief Clause in CNF (Conjunctive Normal Form)
 */
typedef struct {
    atomic_formula_t** literals;
    uint32_t num_literals;
    float confidence;             ///< Confidence in clause [0,1]
} logic_clause_t;

/**
 * @brief Inference rule
 */
typedef struct {
    char name[LOGIC_MAX_NAME_LENGTH];
    logic_clause_t** premises;
    uint32_t num_premises;
    logic_clause_t* conclusion;
    float priority;               ///< Rule application priority
} inference_rule_t;

/**
 * @brief Substitution (variable binding)
 */
typedef struct {
    logical_term_t* variable;
    logical_term_t* value;
} substitution_t;

/**
 * @brief Unification result
 */
typedef struct {
    bool success;
    substitution_t** bindings;
    uint32_t num_bindings;
} unification_t;

/**
 * @brief Knowledge base entry
 */
typedef struct {
    logic_clause_t* clause;
    float salience;               ///< Importance [0,1]
    uint64_t timestamp;           ///< When added
    char context[64];             ///< Context label
} kb_entry_t;

/**
 * @brief Symbolic logic engine configuration
 */
typedef struct {
    uint32_t max_predicates;
    uint32_t max_rules;
    uint32_t max_kb_size;
    uint32_t max_inference_depth;
    bool enable_forward_chaining;
    bool enable_backward_chaining;
    bool enable_resolution;
    bool enable_memory_consolidation;
    bool enable_quantum_logic;        ///< Enable quantum-accelerated reasoning
} logic_config_t;

/**
 * @brief Symbolic logic engine instance (opaque)
 */
typedef struct symbolic_logic symbolic_logic_t;

/**
 * @brief Logic engine statistics
 */
typedef struct {
    uint64_t inferences_performed;
    uint32_t facts_stored;
    uint32_t rules_applied;
    float avg_inference_time;    ///< ms
    uint32_t unification_attempts;
    uint32_t unification_successes;
} logic_stats_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create symbolic logic engine
 * @param config Configuration parameters
 * @return Logic engine instance or NULL on failure
 */
symbolic_logic_t* symbolic_logic_create(const logic_config_t* config);

/**
 * @brief Destroy symbolic logic engine
 * @param logic Logic engine to destroy
 */
void symbolic_logic_destroy(symbolic_logic_t* logic);

/**
 * @brief W7: Register symbolic logic engine with a brain's internal KG.
 *
 * Creates the 'cog_logic_symbolic_engine' structural node (if absent) and
 * stores a back-reference so subsequent add_fact / add_rule writes mirror
 * into brain->internal_kg.  Safe to call multiple times.
 *
 * Forward-declared via a void* brain to avoid header circular include.
 *
 * @param logic Logic engine (required).
 * @param brain Owning brain (required).
 * @return 0 on success, -1 on NULL arg.
 */
struct brain_struct;
int symbolic_logic_kg_register(symbolic_logic_t* logic,
                               struct brain_struct* brain);

/**
 * @brief Get logic engine statistics
 * @param logic Logic engine instance
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool symbolic_logic_get_stats(
    const symbolic_logic_t* logic,
    logic_stats_t* stats
);

//=============================================================================
// Formula Construction
//=============================================================================

/**
 * @brief Create logical term
 * @param type Term type
 * @param name Term name/symbol
 * @return Term or NULL on failure
 */
logical_term_t* logic_term_create(term_type_t type, const char* name);

/**
 * @brief Destroy logical term
 * @param term Term to destroy
 */
void logic_term_destroy(logical_term_t* term);

/**
 * @brief Create atomic formula (predicate)
 * @param name Predicate name
 * @param terms Argument terms
 * @param arity Number of arguments
 * @return Atomic formula or NULL on failure
 */
atomic_formula_t* logic_atom_create(
    const char* name,
    logical_term_t** terms,
    uint8_t arity
);

/**
 * @brief Destroy atomic formula
 * @param atom Atomic formula to destroy
 */
void logic_atom_destroy(atomic_formula_t* atom);

/**
 * @brief Create logical formula
 * @param op Logical operator
 * @param left Left subformula
 * @param right Right subformula
 * @return Formula or NULL on failure
 */
logical_formula_t* logic_formula_create(
    logical_operator_t op,
    logical_formula_t* left,
    logical_formula_t* right
);

/**
 * @brief Destroy logical formula
 * @param formula Formula to destroy
 */
void logic_formula_destroy(logical_formula_t* formula);

//=============================================================================
// Knowledge Base Management
//=============================================================================

/**
 * @brief Add fact to knowledge base
 * @param logic Logic engine instance
 * @param clause Fact as clause
 * @param salience Importance [0,1]
 * @return true on success, false on failure
 */
bool symbolic_logic_add_fact(
    symbolic_logic_t* logic,
    logic_clause_t* clause,
    float salience
);

/**
 * @brief Add inference rule
 * @param logic Logic engine instance
 * @param rule Inference rule
 * @return true on success, false on failure
 */
bool symbolic_logic_add_rule(
    symbolic_logic_t* logic,
    inference_rule_t* rule
);

/**
 * @brief Query knowledge base
 * @param logic Logic engine instance
 * @param query Query clause
 * @param results Output results array
 * @param num_results Number of results found
 * @return true on success, false on failure
 *
 * @note Caller must free results array with nimcp_free()
 */
bool symbolic_logic_query(
    symbolic_logic_t* logic,
    logic_clause_t* query,
    kb_entry_t*** results,
    int* num_results
);

//=============================================================================
// Inference Engine
//=============================================================================

/**
 * @brief Forward chaining inference
 *
 * Derive new facts from existing facts and rules.
 *
 * @param logic Logic engine instance
 * @param max_iterations Maximum inference iterations
 * @param new_facts Output: newly derived facts
 * @param num_new_facts Output: number of new facts
 * @return true on success, false on failure
 */
bool symbolic_logic_forward_chain(
    symbolic_logic_t* logic,
    uint32_t max_iterations,
    logic_clause_t*** new_facts,
    int* num_new_facts
);

/**
 * @brief Backward chaining inference
 *
 * Try to prove goal from existing facts and rules.
 *
 * @param logic Logic engine instance
 * @param goal Goal to prove
 * @param proof_trace Output: proof steps
 * @param num_steps Output: number of proof steps
 * @return true if goal proven, false otherwise
 */
bool symbolic_logic_backward_chain(
    symbolic_logic_t* logic,
    logic_clause_t* goal,
    inference_rule_t*** proof_trace,
    int* num_steps
);

/**
 * @brief Resolution-based theorem proving
 *
 * @param logic Logic engine instance
 * @param negated_goal Negated goal (for proof by contradiction)
 * @param derived_empty Output: true if empty clause derived
 * @return true on success, false on failure
 */
bool symbolic_logic_resolve(
    symbolic_logic_t* logic,
    logic_clause_t* negated_goal,
    bool* derived_empty
);

//=============================================================================
// Unification
//=============================================================================

/**
 * @brief Unify two terms
 *
 * Find substitution that makes terms identical.
 *
 * @param term1 First term
 * @param term2 Second term
 * @return Unification result (caller must free bindings)
 */
unification_t* symbolic_logic_unify(
    logical_term_t* term1,
    logical_term_t* term2
);

/**
 * @brief Apply substitution to term
 * @param term Term to substitute
 * @param subst Substitution to apply
 * @return New term with substitution applied
 */
logical_term_t* symbolic_logic_substitute(
    logical_term_t* term,
    const substitution_t* subst
);

/**
 * @brief Destroy unification result
 * @param unif Unification to destroy
 */
void unification_destroy(unification_t* unif);

//=============================================================================
// CNF Conversion
//=============================================================================

/**
 * @brief Convert formula to Conjunctive Normal Form
 *
 * @param formula Input formula
 * @param clauses Output: CNF clauses
 * @param num_clauses Output: number of clauses
 * @return true on success, false on failure
 */
bool symbolic_logic_to_cnf(
    logical_formula_t* formula,
    logic_clause_t*** clauses,
    int* num_clauses
);

//=============================================================================
// Brain Integration Helpers
//=============================================================================

/**
 * @brief Compute novelty of logical fact
 *
 * Returns how novel a fact is based on similarity to existing knowledge.
 *
 * @param logic Logic engine instance
 * @param clause Fact to evaluate
 * @return Novelty score [0,1], 0=familiar, 1=novel
 */
float symbolic_logic_compute_novelty(
    symbolic_logic_t* logic,
    logic_clause_t* clause
);

/**
 * @brief Get most salient facts
 *
 * Returns facts with highest salience scores for attention focus.
 *
 * @param logic Logic engine instance
 * @param top_k Number of facts to retrieve
 * @param salient_facts Output: most salient facts
 * @param num_facts Output: number of facts returned
 * @return true on success, false on failure
 */
bool symbolic_logic_get_salient_facts(
    symbolic_logic_t* logic,
    int top_k,
    kb_entry_t*** salient_facts,
    int* num_facts
);

/**
 * @brief Consolidate logical memory
 *
 * Integrates with memory consolidation system (hippocampus).
 *
 * @param logic Logic engine instance
 * @param clause Fact to consolidate
 * @param salience Importance [0,1]
 * @param context Context label
 * @return true on success, false on failure
 */
bool symbolic_logic_consolidate_memory(
    symbolic_logic_t* logic,
    logic_clause_t* clause,
    float salience,
    const char* context
);

/**
 * @brief Trigger curiosity-driven inference
 *
 * Use novelty and salience to guide exploration.
 *
 * @param logic Logic engine instance
 * @param exploration_depth How deep to explore
 * @param interesting_facts Output: discovered facts
 * @param num_facts Output: number of facts found
 * @return true on success, false on failure
 */
bool symbolic_logic_explore(
    symbolic_logic_t* logic,
    uint32_t exploration_depth,
    logic_clause_t*** interesting_facts,
    int* num_facts
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Parse logical formula from string
 *
 * Supports syntax: P(x), P(x) & Q(y), P(x) | Q(y), ~P(x), P(x) -> Q(y)
 *
 * @param str Formula string
 * @return Parsed formula or NULL on error
 */
logical_formula_t* symbolic_logic_parse(const char* str);

/**
 * @brief Convert formula to string
 * @param formula Formula to convert
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return true on success, false on failure
 */
bool symbolic_logic_to_string(
    const logical_formula_t* formula,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Evaluate formula truth value
 *
 * @param logic Logic engine instance
 * @param formula Formula to evaluate
 * @param result Output: truth value
 * @return true on success, false if cannot determine
 */
bool symbolic_logic_evaluate(
    symbolic_logic_t* logic,
    logical_formula_t* formula,
    bool* result
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_H
