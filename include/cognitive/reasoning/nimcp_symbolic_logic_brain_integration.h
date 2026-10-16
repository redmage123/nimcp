/**
 * @file nimcp_symbolic_logic_brain_integration.h
 * @brief Brain-level symbolic logic reasoning integration
 *
 * WHAT: High-level brain API for symbolic logic and knowledge representation
 * WHY:  Enable logical reasoning, inference, and knowledge-based decision making
 * HOW:  Integrate symbolic logic engine with working memory and executive functions
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Abstract symbolic reasoning and rule manipulation
 * - Hippocampus: Declarative knowledge storage (facts, rules)
 * - Working memory: Active inference tracking and unification
 * - Executive functions: Planning multi-step proofs and goal-directed reasoning
 *
 * THEORETICAL FOUNDATION:
 * - First-order logic with resolution-based inference
 * - Forward chaining: Data-driven inference (derive new facts)
 * - Backward chaining: Goal-driven reasoning (prove hypotheses)
 * - Unification: Variable binding for pattern matching
 *
 * INTEGRATION POINTS:
 *
 * 1. Brain Structure (src/core/brain/nimcp_brain_internal.h)
 *    Already included:
 *      symbolic_logic_t* logic_engine;  // Phase 9.4
 *
 * 2. Configuration (include/core/brain/nimcp_brain.h)
 *    Already included:
 *      bool enable_logic;  // Enable symbolic logic and reasoning
 *
 * 3. Working Memory Integration
 *    - Store active inferences in working memory buffer
 *    - Maintain derived facts with salience scores
 *    - Temporal decay for transient conclusions
 *
 * 4. Executive Function Integration
 *    - Use planning for multi-step proof construction
 *    - Task prioritization for inference goals
 *    - Inhibition for circular reasoning prevention
 *
 * PERFORMANCE:
 * - Add fact: O(1) insertion into knowledge base
 * - Forward chain: O(R × F) where R=rules, F=facts
 * - Backward chain: O(D × R) where D=depth, R=rules
 * - Query: O(F) linear scan over facts
 *
 * MEMORY OVERHEAD:
 * - Base: sizeof(symbolic_logic_t) = ~1KB
 * - Per fact: ~200 bytes (clause + metadata)
 * - Per rule: ~500 bytes (premises + conclusion)
 * - Total (100 facts, 50 rules): ~50KB typical
 *
 * @author NIMCP Development Team - Phase 9.4 Integration
 * @date 2025-01-20
 * @version 2.6.2
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_BRAIN_INTEGRATION_H
#define NIMCP_SYMBOLIC_LOGIC_BRAIN_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/reasoning/nimcp_backward_chaining.h"
#include "cognitive/reasoning/nimcp_forward_chaining.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Symbolic logic brain integration configuration
 */
typedef struct {
    uint32_t max_facts;              /**< Maximum knowledge base facts (default: 1000) */
    uint32_t max_rules;              /**< Maximum inference rules (default: 500) */
    uint32_t max_inference_depth;    /**< Maximum backward chain depth (default: 10) */
    bool enable_forward_chaining;    /**< Enable data-driven inference (default: true) */
    bool enable_backward_chaining;   /**< Enable goal-driven reasoning (default: true) */
    bool enable_wm_integration;      /**< Store inferences in working memory (default: true) */
    bool enable_exec_integration;    /**< Use executive planning for proofs (default: true) */
    float wm_inference_salience;     /**< Salience for working memory items (default: 0.7) */
} logic_brain_config_t;

/**
 * @brief Inference result metadata
 */
typedef struct {
    logic_clause_t* conclusion;      /**< Derived conclusion */
    inference_rule_t** proof_steps;  /**< Steps used in derivation */
    uint32_t num_steps;              /**< Number of proof steps */
    float confidence;                /**< Confidence in derivation [0,1] */
    uint64_t inference_time_ms;      /**< Time to derive (milliseconds) */
} inference_result_t;

/**
 * @brief Query result
 */
typedef struct {
    bool success;                    /**< Query succeeded */
    kb_entry_t** matches;            /**< Matching knowledge base entries */
    int num_matches;                 /**< Number of matches */
    unification_t** bindings;        /**< Variable bindings (if query has variables) */
    int num_bindings;                /**< Number of binding sets */
} query_result_t;

//=============================================================================
// Brain API - Lifecycle
//=============================================================================

/**
 * @brief Create symbolic logic engine and attach to brain
 *
 * WHAT: Initialize symbolic reasoning system and integrate with brain
 * WHY:  Enable logical inference, knowledge representation, and rule-based reasoning
 * HOW:  Create logic engine → Attach to brain → Link working memory and executive
 *
 * EFFECTS:
 * - Allocates symbolic_logic_t instance
 * - Attaches to brain->logic_engine
 * - Links to brain->working_memory (if enabled)
 * - Links to brain->executive (if enabled)
 *
 * @param brain Brain instance (non-NULL)
 * @param config Logic configuration (NULL = use defaults)
 * @return true on success, false on failure
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine already exists → return false (already initialized)
 * - Allocation failure → return false
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access during initialization)
 * MALLOC: Yes (logic engine, knowledge base structures)
 *
 * EXAMPLE:
 * ```c
 * brain_t brain = brain_create("reasoner", BRAIN_SIZE_SMALL);
 * logic_brain_config_t config = {
 *     .max_facts = 500,
 *     .enable_wm_integration = true
 * };
 * brain_create_symbolic_logic(brain, &config);
 * ```
 */
bool brain_create_symbolic_logic(
    brain_t brain,
    const logic_brain_config_t* config
);

/**
 * @brief Destroy symbolic logic engine attached to brain
 *
 * WHAT: Clean up logic engine and free resources
 * WHY:  Prevent memory leaks on brain destruction
 * HOW:  Detach from brain → Free knowledge base → Destroy engine
 *
 * @param brain Brain instance
 *
 * COMPLEXITY: O(F + R) where F=facts, R=rules
 * THREAD-SAFE: No
 */
void brain_destroy_symbolic_logic(brain_t brain);

//=============================================================================
// Brain API - Knowledge Base Operations
//=============================================================================

/**
 * @brief Add logical fact to brain's knowledge base
 *
 * WHAT: Parse fact string and store in knowledge base
 * WHY:  Build declarative knowledge for reasoning
 * HOW:  Parse → Convert to clause → Add to KB → Store in WM (if enabled)
 *
 * SYNTAX:
 * - Atoms: "Bird(tweety)", "Mortal(socrates)"
 * - Negation: "~Fly(penguin)"
 * - Multiple literals: "Bird(x) | Mammal(x)" (disjunction)
 *
 * EFFECTS:
 * - Adds fact to knowledge base
 * - Stores in working memory (if config.enable_wm_integration)
 * - Updates brain statistics
 *
 * @param brain Brain instance (non-NULL)
 * @param fact_str Fact string in logic syntax
 * @param salience Importance score [0,1] (affects working memory retention)
 * @return true on success, false on parse error or failure
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false (not initialized)
 * - fact_str parse error → return false
 * - Knowledge base full → return false
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: No
 *
 * EXAMPLE:
 * ```c
 * brain_add_logical_fact(brain, "Bird(tweety)", 0.9);
 * brain_add_logical_fact(brain, "Penguin(opus)", 0.8);
 * ```
 */
bool brain_add_logical_fact(
    brain_t brain,
    const char* fact_str,
    float salience
);

/**
 * @brief Add inference rule to brain's knowledge base
 *
 * WHAT: Parse rule string and store in knowledge base
 * WHY:  Enable automated inference and reasoning
 * HOW:  Parse → Convert to premises and conclusion → Add to KB
 *
 * SYNTAX:
 * - Simple implication: "Bird(x) -> Fly(x)"
 * - Multiple premises: "Bird(x) & ~Penguin(x) -> Fly(x)"
 * - Modus ponens: "P(x) & (P(x) -> Q(x)) -> Q(x)"
 *
 * EFFECTS:
 * - Adds rule to inference engine
 * - Available for forward/backward chaining
 * - Updates brain statistics
 *
 * @param brain Brain instance (non-NULL)
 * @param rule_str Rule string in logic syntax
 * @param priority Rule application priority [0,1] (higher = applied first)
 * @return true on success, false on parse error or failure
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false
 * - rule_str parse error → return false
 * - Rule base full → return false
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: No
 *
 * EXAMPLE:
 * ```c
 * brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8);
 * brain_add_logical_rule(brain, "Penguin(x) -> Bird(x)", 0.9);
 * brain_add_logical_rule(brain, "Penguin(x) -> ~Fly(x)", 0.95);
 * ```
 */
bool brain_add_logical_rule(
    brain_t brain,
    const char* rule_str,
    float priority
);

/**
 * @brief Query brain's knowledge base
 *
 * WHAT: Search for facts matching query pattern
 * WHY:  Retrieve stored knowledge and check existence
 * HOW:  Parse query → Unify with KB → Return matches and bindings
 *
 * SYNTAX:
 * - Ground query: "Bird(tweety)" → returns true/false
 * - Variable query: "Bird(x)" → returns all birds with bindings
 * - Complex query: "Bird(x) & Fly(x)" → returns flying birds
 *
 * EFFECTS:
 * - Searches knowledge base
 * - Returns matching facts and variable bindings
 * - Updates query statistics
 *
 * @param brain Brain instance (non-NULL)
 * @param query_str Query string in logic syntax
 * @param result Output query result (caller must free with brain_free_query_result)
 * @return true on success (even if no matches), false on error
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false
 * - query_str parse error → return false
 * - result is NULL → return false
 *
 * COMPLEXITY: O(F) where F = number of facts
 * THREAD-SAFE: No
 * MALLOC: Yes (result structure, matches array, bindings)
 *
 * EXAMPLE:
 * ```c
 * query_result_t result;
 * if (brain_query_knowledge(brain, "Bird(x)", &result)) {
 *     printf("Found %d birds\n", result.num_matches);
 *     brain_free_query_result(&result);
 * }
 * ```
 */
bool brain_query_knowledge(
    brain_t brain,
    const char* query_str,
    query_result_t* result
);

/**
 * @brief Free query result resources
 *
 * @param result Query result to free
 *
 * COMPLEXITY: O(M) where M = number of matches
 */
void brain_free_query_result(query_result_t* result);

//=============================================================================
// Brain API - Inference Operations
//=============================================================================

/**
 * @brief Perform forward chaining inference
 *
 * WHAT: Derive new facts from existing facts and rules (data-driven)
 * WHY:  Discover implicit knowledge and logical consequences
 * HOW:  Iteratively apply rules to KB → Derive new facts → Repeat until convergence
 *
 * ALGORITHM:
 * 1. Match rule premises against knowledge base
 * 2. Apply substitutions to derive conclusions
 * 3. Add new facts to knowledge base
 * 4. Store in working memory (if enabled)
 * 5. Repeat until no new facts or max_iterations reached
 *
 * INTEGRATION:
 * - New facts added to working memory with salience
 * - Executive system used for iteration control (if enabled)
 * - Statistics tracked in brain->stats
 *
 * @param brain Brain instance (non-NULL)
 * @param max_iterations Maximum inference iterations (0 = unlimited, capped at 1000)
 * @param result Output forward chain result (caller must free with forward_chain_free_result)
 * @return true on success, false on error
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false
 * - Forward chaining disabled → return false
 * - result is NULL → return false
 *
 * COMPLEXITY: O(I × R × F) where I=iterations, R=rules, F=facts
 * THREAD-SAFE: No
 * MALLOC: Yes (inference result, new facts array)
 *
 * EXAMPLE:
 * ```c
 * // Setup
 * brain_add_logical_fact(brain, "Bird(tweety)", 0.9);
 * brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8);
 *
 * // Infer
 * forward_chain_result_t result;
 * brain_forward_chain(brain, 10, &result);
 * // Result: "Fly(tweety)" derived
 * forward_chain_free_result(&result);
 * ```
 */
bool brain_forward_chain(
    brain_t brain,
    uint32_t max_iterations,
    forward_chain_result_t* result
);

/**
 * @brief Perform backward chaining inference
 *
 * WHAT: Prove goal by working backward from conclusion to premises (goal-driven)
 * WHY:  Verify hypotheses and construct proofs
 * HOW:  Match goal with rule conclusions → Recursively prove premises → Unify variables
 *
 * ALGORITHM:
 * 1. Check if goal is in knowledge base (base case)
 * 2. Find rules with conclusion matching goal
 * 3. Recursively prove all premises
 * 4. Apply substitutions and construct proof trace
 * 5. Store proof in working memory (if enabled)
 *
 * INTEGRATION:
 * - Executive system manages proof search (if enabled)
 * - Working memory stores proof steps
 * - Task planning used for multi-step proofs
 *
 * @param brain Brain instance (non-NULL)
 * @param goal_str Goal to prove
 * @param result Output inference result with proof trace (caller must free)
 * @return true if goal proven, false if unprovable or error
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false
 * - Backward chaining disabled → return false
 * - goal_str parse error → return false
 * - result is NULL → return false
 *
 * COMPLEXITY: O(D × R) where D=max_depth, R=rules
 * THREAD-SAFE: No
 * MALLOC: Yes (inference result, proof trace)
 *
 * EXAMPLE:
 * ```c
 * // Setup
 * brain_add_logical_fact(brain, "Man(socrates)", 0.9);
 * brain_add_logical_rule(brain, "Man(x) -> Mortal(x)", 0.8);
 *
 * // Prove
 * backward_chain_result_t result;
 * if (brain_backward_chain(brain, "Mortal(socrates)", &result)) {
 *     printf("Proven! Steps: %u\n", result.num_steps);
 *     backward_chain_free_result(&result);
 * }
 * ```
 */
bool brain_backward_chain(
    brain_t brain,
    const char* goal_str,
    backward_chain_result_t* result
);

//=============================================================================
// Brain API - Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get symbolic logic engine statistics
 *
 * @param brain Brain instance
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
bool brain_get_logic_stats(
    brain_t brain,
    logic_stats_t* stats
);

/**
 * @brief Export knowledge base to file (human-readable)
 *
 * WHAT: Write all facts and rules to text file
 * WHY:  Enable inspection, debugging, and persistence
 * HOW:  Iterate KB → Format as logic syntax → Write to file
 *
 * FORMAT:
 * ```
 * // Facts
 * Bird(tweety). [salience=0.9]
 * Penguin(opus). [salience=0.8]
 *
 * // Rules
 * Bird(x) -> Fly(x). [priority=0.8]
 * Penguin(x) -> ~Fly(x). [priority=0.95]
 * ```
 *
 * @param brain Brain instance
 * @param filepath Output file path
 * @return true on success, false on error
 *
 * COMPLEXITY: O(F + R) where F=facts, R=rules
 */
bool brain_export_knowledge_base(
    brain_t brain,
    const char* filepath
);

/**
 * @brief Import knowledge base from file
 *
 * WHAT: Read facts and rules from text file
 * WHY:  Load pre-existing knowledge
 * HOW:  Parse file → Add facts and rules → Build KB
 *
 * @param brain Brain instance
 * @param filepath Input file path
 * @return true on success, false on error
 *
 * COMPLEXITY: O(F + R) where F=facts, R=rules in file
 */
bool brain_import_knowledge_base(
    brain_t brain,
    const char* filepath
);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local storage)
 */
const char* brain_logic_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_BRAIN_INTEGRATION_H
