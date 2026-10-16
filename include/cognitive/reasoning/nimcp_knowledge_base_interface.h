/**
 * @file nimcp_knowledge_base_interface.h
 * @brief MODULE 2: Knowledge Base Interface - Fact/rule management through brain
 *
 * SINGLE RESPONSIBILITY: Add/query facts and rules through brain interface
 *
 * WHAT: High-level brain API for knowledge base operations
 * WHY:  Centralize knowledge management in one module following SRP
 * HOW:  Validate brain → delegate to symbolic_logic_t methods → publish events
 *
 * SRP ADHERENCE:
 * - ONLY handles knowledge base content operations (add/query facts/rules)
 * - Does NOT manage engine attachment (see symbolic_logic_attachment.h)
 * - Does NOT perform inference (see forward_chaining.h, backward_chaining.h)
 * - Does NOT create engines (see reasoning_factory.h)
 *
 * INTEGRATION POINTS:
 * - Working memory: Stores facts with salience scores
 * - Event publishing: EVENT_FACT_ADDED, EVENT_RULE_ADDED, EVENT_QUERY_EXECUTED
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#ifndef NIMCP_KNOWLEDGE_BASE_INTERFACE_H
#define NIMCP_KNOWLEDGE_BASE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Query Result Type
//=============================================================================

/**
 * @brief Query result structure
 */
typedef struct {
    bool success;                    /**< Query succeeded */
    kb_entry_t** matches;            /**< Matching knowledge base entries */
    int num_matches;                 /**< Number of matches */
    unification_t** bindings;        /**< Variable bindings (if query has variables) */
    int num_bindings;                /**< Number of binding sets */
} kb_query_result_t;

//=============================================================================
// Knowledge Base Operations - SOLE RESPONSIBILITY
//=============================================================================

/**
 * @brief Add logical fact to brain's knowledge base
 *
 * WHAT: Parse fact string and store in knowledge base
 * WHY:  Build declarative knowledge for reasoning
 * HOW:  Validate → Parse → Add to KB → Store in WM → Publish event
 *
 * SYNTAX:
 * - Atoms: "Bird(tweety)", "Mortal(socrates)"
 * - Negation: "~Fly(penguin)"
 * - Multiple literals: "Bird(x) | Mammal(x)" (disjunction)
 *
 * EFFECTS:
 * - Adds fact to symbolic_logic_t knowledge base
 * - Stores in working memory (if enabled)
 * - Publishes EVENT_FACT_ADDED
 * - Updates brain statistics
 *
 * @param brain Brain instance (non-NULL)
 * @param fact_str Fact string in logic syntax
 * @param salience Importance score [0,1] (affects working memory retention)
 * @return true on success, false on parse error or failure
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false (engine not attached)
 * - fact_str is NULL → return false
 * - fact_str parse error → return false
 * - salience out of range [0,1] → return false
 * - Knowledge base full → return false
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: No
 * MALLOC: Yes (clause structures)
 *
 * EXAMPLE:
 * ```c
 * brain_add_fact(brain, "Bird(tweety)", 0.9f);
 * brain_add_fact(brain, "Penguin(opus)", 0.8f);
 * ```
 */
bool brain_add_fact(
    brain_t brain,
    const char* fact_str,
    float salience
);

/**
 * @brief Add inference rule to brain's knowledge base
 *
 * WHAT: Parse rule string and store in knowledge base
 * WHY:  Enable automated inference and reasoning
 * HOW:  Validate → Parse → Extract premises/conclusion → Add to KB → Publish event
 *
 * SYNTAX:
 * - Simple implication: "Bird(x) -> Fly(x)"
 * - Multiple premises: "Bird(x) & ~Penguin(x) -> Fly(x)"
 * - Modus ponens: "P(x) & (P(x) -> Q(x)) -> Q(x)"
 *
 * EFFECTS:
 * - Adds rule to symbolic_logic_t inference engine
 * - Available for forward/backward chaining
 * - Publishes EVENT_RULE_ADDED
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
 * - rule_str is NULL → return false
 * - rule_str parse error → return false
 * - priority out of range [0,1] → return false
 * - Rule base full → return false
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: No
 * MALLOC: Yes (rule structures)
 *
 * EXAMPLE:
 * ```c
 * brain_add_rule(brain, "Bird(x) -> Fly(x)", 0.8f);
 * brain_add_rule(brain, "Penguin(x) -> Bird(x)", 0.9f);
 * brain_add_rule(brain, "Penguin(x) -> ~Fly(x)", 0.95f);
 * ```
 */
bool brain_add_rule(
    brain_t brain,
    const char* rule_str,
    float priority
);

/**
 * @brief Query brain's knowledge base
 *
 * WHAT: Search for facts matching query pattern
 * WHY:  Retrieve stored knowledge and check existence
 * HOW:  Validate → Parse query → Unify with KB → Return matches and bindings
 *
 * SYNTAX:
 * - Ground query: "Bird(tweety)" → returns true/false
 * - Variable query: "Bird(x)" → returns all birds with bindings
 * - Complex query: "Bird(x) & Fly(x)" → returns flying birds
 *
 * EFFECTS:
 * - Searches symbolic_logic_t knowledge base
 * - Returns matching facts and variable bindings
 * - Publishes EVENT_QUERY_EXECUTED
 * - Updates query statistics
 *
 * @param brain Brain instance (non-NULL)
 * @param query_str Query string in logic syntax
 * @param result Output query result (caller must free with kb_free_query_result)
 * @return true on success (even if no matches), false on error
 *
 * ERROR CONDITIONS:
 * - brain is NULL → return false
 * - brain->logic_engine is NULL → return false
 * - query_str is NULL → return false
 * - query_str parse error → return false
 * - result is NULL → return false
 *
 * COMPLEXITY: O(F) where F = number of facts
 * THREAD-SAFE: No
 * MALLOC: Yes (result structure, matches array, bindings)
 *
 * EXAMPLE:
 * ```c
 * kb_query_result_t result;
 * if (brain_query_knowledge(brain, "Bird(x)", &result)) {
 *     printf("Found %d birds\n", result.num_matches);
 *     kb_free_query_result(&result);
 * }
 * ```
 */
bool brain_query_knowledge(
    brain_t brain,
    const char* query_str,
    kb_query_result_t* result
);

/**
 * @brief Free query result resources
 *
 * WHAT: Deallocate memory for query result
 * WHY:  Prevent memory leaks
 * HOW:  Free matches array, bindings array, result structure
 *
 * @param result Query result to free (can be NULL)
 *
 * COMPLEXITY: O(M) where M = number of matches
 * THREAD-SAFE: Yes
 * MALLOC: No (frees memory)
 */
void kb_free_query_result(kb_query_result_t* result);

/**
 * @brief Get number of facts in brain's knowledge base
 *
 * WHAT: Count stored facts
 * WHY:  Monitor KB size
 * HOW:  Delegate to symbolic_logic_get_stats
 *
 * @param brain Brain instance
 * @return Number of facts (0 if no engine attached)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
uint32_t brain_get_fact_count(brain_t brain);

/**
 * @brief Get number of rules in brain's knowledge base
 *
 * WHAT: Count stored rules
 * WHY:  Monitor rule base size
 * HOW:  Delegate to symbolic_logic_get_stats
 *
 * @param brain Brain instance
 * @return Number of rules (0 if no engine attached)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
uint32_t brain_get_rule_count(brain_t brain);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message from knowledge base operations
 *
 * @return Error message string (thread-local storage)
 */
const char* kb_interface_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_KNOWLEDGE_BASE_INTERFACE_H
