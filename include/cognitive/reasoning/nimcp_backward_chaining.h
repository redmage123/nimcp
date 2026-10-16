/**
 * @file nimcp_backward_chaining.h
 * @brief MODULE 4: Backward Chaining Engine - Deductive reasoning
 *
 * SINGLE RESPONSIBILITY: Prove goals from facts and rules (goal-driven reasoning)
 *
 * WHAT: Backward chaining inference through brain API
 * WHY:  Enable goal-driven reasoning and hypothesis verification
 * HOW:  Match goal with rule conclusions → Recursively prove premises → Unify variables
 *
 * SRP ADHERENCE:
 * - ONLY handles backward chaining inference operations
 * - Does NOT manage engine attachment (see symbolic_logic_attachment.h)
 * - Does NOT manage knowledge base content (see knowledge_base_interface.h)
 * - Does NOT perform forward chaining (see forward_chaining.h)
 * - Does NOT handle unification directly (see unification_engine.h)
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Goal management and planning
 * - Working memory: Active proof state tracking
 * - Executive functions: Proof search strategy
 *
 * INTEGRATION POINTS:
 * - Working memory: Stores proof steps
 * - Executive functions: Manages proof search
 * - Event publishing: EVENT_BACKWARD_CHAIN_STEP, EVENT_PROOF_FOUND, EVENT_PROOF_FAILED
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#ifndef NIMCP_BACKWARD_CHAINING_H
#define NIMCP_BACKWARD_CHAINING_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Backward Chaining Result Type
//=============================================================================

/**
 * @brief Backward chaining proof result
 */
typedef struct {
    bool proven;                     /**< Goal successfully proven */
    inference_rule_t** proof_steps;  /**< Proof trace (rules applied) */
    uint32_t num_steps;              /**< Number of steps in proof */
    float confidence;                /**< Confidence in proof [0,1] */
    uint64_t inference_time_ms;      /**< Time to prove (milliseconds) */
    uint32_t depth_reached;          /**< Maximum proof depth reached */
} backward_chain_result_t;

//=============================================================================
// Backward Chaining Operations - SOLE RESPONSIBILITY
//=============================================================================

/**
 * @brief Perform backward chaining to prove goal
 *
 * WHAT: Prove goal by working backward from conclusion to premises
 * WHY:  Verify hypotheses and construct proofs
 * HOW:  Match goal with rule conclusions → Recursively prove premises → Unify variables
 *
 * ALGORITHM:
 * 1. Check if goal is in knowledge base (base case)
 * 2. Find rules with conclusion matching goal
 * 3. Recursively prove all premises
 * 4. Apply substitutions and construct proof trace
 * 5. Store proof in working memory (if enabled)
 * 6. Publish EVENT_PROOF_FOUND or EVENT_PROOF_FAILED
 *
 * @param brain Brain instance (non-NULL)
 * @param goal_str Goal to prove
 * @param result Output proof result (caller must free with backward_chain_free_result)
 * @return true if goal proven, false if unprovable or error
 *
 * COMPLEXITY: O(D × R) where D=max_depth, R=rules
 * THREAD-SAFE: No
 * MALLOC: Yes (proof trace)
 */
bool brain_backward_chain(
    brain_t brain,
    const char* goal_str,
    backward_chain_result_t* result
);

/**
 * @brief Perform single backward chaining step
 *
 * WHAT: Execute one step of backward chaining for subgoal
 * WHY:  Allow fine-grained control over proof search
 * HOW:  Find matching rules for subgoal → Return premises to prove
 *
 * @param brain Brain instance (non-NULL)
 * @param subgoal_str Subgoal to prove
 * @param premises Output premises that need proving (caller must free)
 * @param num_premises Number of premises
 * @return true if subgoal has matching rules, false otherwise
 *
 * COMPLEXITY: O(R) where R=rules
 * THREAD-SAFE: No
 * MALLOC: Yes (premises array)
 */
bool brain_backward_chain_step(
    brain_t brain,
    const char* subgoal_str,
    logic_clause_t*** premises,
    uint32_t* num_premises
);

/**
 * @brief Free backward chaining result resources
 *
 * @param result Proof result to free (can be NULL)
 *
 * COMPLEXITY: O(N) where N = number of proof steps
 * THREAD-SAFE: Yes
 */
void backward_chain_free_result(backward_chain_result_t* result);

/**
 * @brief Get backward chaining statistics
 *
 * @param brain Brain instance
 * @param proofs_attempted Output: number of proof attempts
 * @param proofs_succeeded Output: number of successful proofs
 * @param avg_depth Output: average proof depth
 * @return true on success, false if no statistics available
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool brain_get_backward_chain_stats(
    brain_t brain,
    uint32_t* proofs_attempted,
    uint32_t* proofs_succeeded,
    float* avg_depth
);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message from backward chaining operations
 *
 * @return Error message string (thread-local storage)
 */
const char* backward_chain_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BACKWARD_CHAINING_H
