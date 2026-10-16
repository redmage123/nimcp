/**
 * @file nimcp_forward_chaining.h
 * @brief MODULE 3: Forward Chaining Engine - Inductive reasoning
 *
 * SINGLE RESPONSIBILITY: Derive new facts from existing knowledge (data-driven inference)
 *
 * WHAT: Forward chaining inference through brain API
 * WHY:  Enable data-driven reasoning and knowledge discovery
 * HOW:  Iteratively apply rules to KB → Derive new facts → Repeat until convergence
 *
 * SRP ADHERENCE:
 * - ONLY handles forward chaining inference operations
 * - Does NOT manage engine attachment (see symbolic_logic_attachment.h)
 * - Does NOT manage knowledge base content (see knowledge_base_interface.h)
 * - Does NOT perform backward chaining (see backward_chaining.h)
 * - Does NOT handle unification directly (see unification_engine.h)
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Pattern matching and rule application
 * - Working memory: Active inference tracking
 * - Hippocampus: Fact retrieval for matching
 *
 * INTEGRATION POINTS:
 * - Working memory: Stores derived facts with salience
 * - Executive functions: Manages iteration control
 * - Event publishing: EVENT_FORWARD_CHAIN_STEP, EVENT_NOVEL_FACT_DERIVED
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#ifndef NIMCP_FORWARD_CHAINING_H
#define NIMCP_FORWARD_CHAINING_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_kg.h"   /* W7: KG query API */
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Chaining Result Type
//=============================================================================

/**
 * @brief Forward chaining inference result
 */
typedef struct {
    logic_clause_t** new_facts;      /**< Array of derived facts */
    uint32_t num_new_facts;          /**< Number of facts derived */
    uint32_t iterations_performed;   /**< Number of inference iterations */
    float confidence;                /**< Overall confidence [0,1] */
    uint64_t inference_time_ms;      /**< Time to complete (milliseconds) */
    bool converged;                  /**< True if reached fixpoint */
} forward_chain_result_t;

//=============================================================================
// Forward Chaining Operations - SOLE RESPONSIBILITY
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
 * 5. Publish EVENT_FORWARD_CHAIN_STEP
 * 6. Repeat until no new facts or max_iterations reached
 * 7. Publish EVENT_NOVEL_FACT_DERIVED for each new fact
 *
 * INTEGRATION:
 * - New facts added to working memory with default salience (0.7)
 * - Executive system used for iteration control (if enabled)
 * - Statistics tracked in brain->stats
 *
 * @param brain Brain instance (non-NULL)
 * @param max_iterations Maximum inference iterations (0 = unlimited, capped at 1000)
 * @param result Output inference result (caller must free with forward_chain_free_result)
 * @return true on success, false on error
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false (engine not attached)
 * - result is NULL → return false
 *
 * COMPLEXITY: O(I × R × F) where I=iterations, R=rules, F=facts
 * THREAD-SAFE: No
 * MALLOC: Yes (inference result, new facts array)
 *
 * EXAMPLE:
 * ```c
 * // Setup
 * brain_add_fact(brain, "Bird(tweety)", 0.9f);
 * brain_add_rule(brain, "Bird(x) -> Fly(x)", 0.8f);
 *
 * // Infer
 * forward_chain_result_t result;
 * if (brain_forward_chain(brain, 10, &result)) {
 *     printf("Derived %u new facts in %u iterations\n",
 *            result.num_new_facts, result.iterations_performed);
 *     forward_chain_free_result(&result);
 * }
 * ```
 */
bool brain_forward_chain(
    brain_t brain,
    uint32_t max_iterations,
    forward_chain_result_t* result
);

/**
 * @brief Perform single forward chaining step
 *
 * WHAT: Execute one iteration of forward chaining
 * WHY:  Allow fine-grained control over inference process
 * HOW:  Apply all applicable rules once → Return new facts
 *
 * EFFECTS:
 * - Applies all matching rules to current knowledge base
 * - Derives new facts (if any)
 * - Does NOT add facts to KB (caller's responsibility)
 * - Publishes EVENT_FORWARD_CHAIN_STEP
 *
 * @param brain Brain instance (non-NULL)
 * @param new_facts Output array of new facts (caller must free)
 * @param num_new_facts Number of new facts derived
 * @return true on success, false on error
 *
 * COMPLEXITY: O(R × F) where R=rules, F=facts
 * THREAD-SAFE: No
 * MALLOC: Yes (new_facts array)
 *
 * EXAMPLE:
 * ```c
 * logic_clause_t** new_facts = NULL;
 * uint32_t num_new = 0;
 *
 * while (brain_forward_chain_step(brain, &new_facts, &num_new)) {
 *     if (num_new == 0) break; // Converged
 *
 *     for (uint32_t i = 0; i < num_new; i++) {
 *         brain_add_fact(brain, format_clause(new_facts[i]), 0.7f);
 *     }
 * }
 * ```
 */
bool brain_forward_chain_step(
    brain_t brain,
    logic_clause_t*** new_facts,
    uint32_t* num_new_facts
);

/**
 * @brief Free forward chaining result resources
 *
 * WHAT: Deallocate memory for forward chaining result
 * WHY:  Prevent memory leaks
 * HOW:  Free new_facts array and result structure
 *
 * @param result Forward chaining result to free (can be NULL)
 *
 * COMPLEXITY: O(N) where N = number of new facts
 * THREAD-SAFE: Yes
 * MALLOC: No (frees memory)
 */
void forward_chain_free_result(forward_chain_result_t* result);

/**
 * @brief Get forward chaining statistics
 *
 * WHAT: Retrieve inference statistics from last operation
 * WHY:  Monitor performance and efficiency
 * HOW:  Return cached statistics from last forward chain
 *
 * @param brain Brain instance
 * @param iterations Output: number of iterations performed
 * @param facts_derived Output: total facts derived
 * @param time_ms Output: total time in milliseconds
 * @return true on success, false if no statistics available
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool brain_get_forward_chain_stats(
    brain_t brain,
    uint32_t* iterations,
    uint32_t* facts_derived,
    uint64_t* time_ms
);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message from forward chaining operations
 *
 * @return Error message string (thread-local storage)
 */
const char* forward_chain_get_last_error(void);

//=============================================================================
// W7: KG-backed antecedent lookup
//=============================================================================

/**
 * @brief Query brain->internal_kg for antecedents (incoming edges) of a goal.
 *
 * @param brain   Brain handle (required, must have internal_kg enabled).
 * @param goal    Canonical KG node name of the goal (e.g. "IsA_cat_animal").
 * @param ids     Output buffer of node IDs.
 * @param max     Max ids to write.
 * @return Number of antecedent node IDs written (0 if goal absent / KG off).
 */
int forward_chaining_kg_query_antecedents(
    brain_t brain,
    const char* goal,
    brain_kg_node_id_t* ids,
    int max);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FORWARD_CHAINING_H
