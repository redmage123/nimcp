/**
 * @file nimcp_symbolic_logic_safety.h
 * @brief LGSS Component A1: Safety Knowledge Base API for Symbolic Logic Safety Extension
 *
 * WHAT: API for creating, managing, and evaluating safety rules
 * WHY:  Provide memory-protected, tamper-resistant safety constraints
 * HOW:  mmap-based storage, mprotect locking, forward chaining evaluation
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex: Rule-based inhibitory control
 * - Basal ganglia: Action selection and gating
 * - Amygdala: Threat assessment and safety evaluation
 *
 * SECURITY FEATURES:
 * - Memory-mapped rule storage
 * - Irreversible mprotect locking
 * - SHA-256 integrity verification
 * - Forward chaining for rule evaluation
 *
 * LIFECYCLE:
 * 1. Create KB with symbolic_logic_safety_kb_create()
 * 2. Add rules with symbolic_logic_safety_add_rule()
 * 3. Compile rules with symbolic_logic_safety_compile_rules()
 * 4. Lock KB with symbolic_logic_safety_lock() (IRREVERSIBLE)
 * 5. Evaluate with symbolic_logic_safety_evaluate()
 * 6. Destroy with symbolic_logic_safety_kb_destroy()
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_SAFETY_H
#define NIMCP_SYMBOLIC_LOGIC_SAFETY_H

#include "nimcp_symbolic_logic_safety_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Knowledge Base Lifecycle
//=============================================================================

/**
 * @brief Create a new safety knowledge base
 *
 * WHAT: Allocate and initialize a safety KB with mmap region
 * WHY:  Provide container for safety rules with memory protection capability
 * HOW:  Allocates mmap region, initializes structures
 *
 * @param max_rules Maximum number of rules the KB can hold (0 = default)
 * @return Pointer to new safety KB, or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (creates independent instance)
 * MALLOC: Yes (uses mmap for rule storage)
 *
 * ERROR CONDITIONS:
 * - Returns NULL if mmap fails
 * - Returns NULL if memory allocation fails
 *
 * EXAMPLE:
 * ```c
 * safety_kb_t* kb = symbolic_logic_safety_kb_create(100);
 * if (!kb) {
 *     LOG_ERROR("Failed to create safety KB");
 *     return;
 * }
 * // Use kb...
 * symbolic_logic_safety_kb_destroy(kb);
 * ```
 */
safety_kb_t* symbolic_logic_safety_kb_create(uint32_t max_rules);

/**
 * @brief Destroy a safety knowledge base
 *
 * WHAT: Free all resources associated with a safety KB
 * WHY:  Prevent memory leaks
 * HOW:  Unmaps mmap region, frees structures
 *
 * @param kb Safety KB to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 * MALLOC: No (frees memory)
 *
 * NOTE: Safe to call on NULL
 * NOTE: Safe to call on locked KB (munmap still works)
 */
void symbolic_logic_safety_kb_destroy(safety_kb_t* kb);

//=============================================================================
// Rule Management
//=============================================================================

/**
 * @brief Add a safety rule to the knowledge base
 *
 * WHAT: Insert a new rule into the KB
 * WHY:  Build up the safety rule set
 * HOW:  Copies rule to KB, assigns unique ID
 *
 * @param kb Safety KB (non-NULL, not locked)
 * @param rule Rule to add (non-NULL)
 * @return Assigned rule ID, or 0 on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 * MALLOC: No (uses pre-allocated mmap region)
 *
 * ERROR CONDITIONS:
 * - Returns 0 if kb is NULL
 * - Returns 0 if rule is NULL
 * - Returns 0 if KB is locked (cannot add rules after locking)
 * - Returns 0 if KB is full (num_rules >= max_rules)
 *
 * EXAMPLE:
 * ```c
 * safety_rule_t rule = {0};
 * strcpy(rule.name, "deny_weapon_synthesis");
 * rule.domain = SAFETY_DOMAIN_WEAPONS;
 * rule.severity = SAFETY_SEVERITY_CRITICAL;
 * rule.action = SAFETY_ACTION_DENY;
 * rule.conditions[0].op = SAFETY_COND_OP_CONTAINS;
 * strcpy(rule.conditions[0].field, "action_type");
 * strcpy(rule.conditions[0].value, "synthesize_weapon");
 * rule.num_conditions = 1;
 *
 * uint32_t id = symbolic_logic_safety_add_rule(kb, &rule);
 * if (id == 0) {
 *     LOG_ERROR("Failed to add rule");
 * }
 * ```
 */
uint32_t symbolic_logic_safety_add_rule(safety_kb_t* kb, const safety_rule_t* rule);

/**
 * @brief Remove a safety rule from the knowledge base
 *
 * WHAT: Remove a rule by ID
 * WHY:  Allow rule management before locking
 * HOW:  Marks slot as empty, compacts if needed
 *
 * @param kb Safety KB (non-NULL, not locked)
 * @param rule_id Rule ID to remove
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(n) where n = number of rules
 * THREAD-SAFE: No
 *
 * ERROR CONDITIONS:
 * - Returns false if kb is NULL
 * - Returns false if KB is locked
 * - Returns false if rule_id not found
 */
bool symbolic_logic_safety_remove_rule(safety_kb_t* kb, uint32_t rule_id);

/**
 * @brief Get a rule by ID
 *
 * WHAT: Retrieve a rule from the KB
 * WHY:  Allow inspection of rules
 * HOW:  Linear search by ID
 *
 * @param kb Safety KB (non-NULL)
 * @param rule_id Rule ID to find
 * @return Pointer to rule (read-only), or NULL if not found
 *
 * COMPLEXITY: O(n) where n = number of rules
 * THREAD-SAFE: Yes (read-only)
 *
 * NOTE: Returns pointer into KB's internal storage
 * NOTE: Do not modify returned rule if KB is locked
 */
const safety_rule_t* symbolic_logic_safety_get_rule(const safety_kb_t* kb, uint32_t rule_id);

/**
 * @brief Get all rules for a specific domain
 *
 * WHAT: Retrieve rules filtered by domain
 * WHY:  Enable domain-specific analysis
 * HOW:  Linear scan with domain filter
 *
 * @param kb Safety KB (non-NULL)
 * @param domain Domain to filter by
 * @param rules_out Output array of rule pointers (caller provides)
 * @param max_rules Maximum rules to return
 * @return Number of rules returned
 *
 * COMPLEXITY: O(n) where n = total rules
 * THREAD-SAFE: Yes (read-only)
 *
 * EXAMPLE:
 * ```c
 * const safety_rule_t* rules[100];
 * uint32_t count = symbolic_logic_safety_get_rules_by_domain(
 *     kb, SAFETY_DOMAIN_WEAPONS, rules, 100);
 * for (uint32_t i = 0; i < count; i++) {
 *     printf("Rule: %s\n", rules[i]->name);
 * }
 * ```
 */
uint32_t symbolic_logic_safety_get_rules_by_domain(
    const safety_kb_t* kb,
    safety_domain_t domain,
    const safety_rule_t** rules_out,
    uint32_t max_rules
);

//=============================================================================
// Rule Compilation
//=============================================================================

/**
 * @brief Compile all rules to First-Order Logic representation
 *
 * WHAT: Convert rule conditions to FOL formulas
 * WHY:  Enable efficient evaluation using forward chaining
 * HOW:  Generates FOL string for each rule, computes integrity hash
 *
 * @param kb Safety KB (non-NULL, not locked)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions per rule
 * THREAD-SAFE: No
 *
 * EFFECTS:
 * - Sets is_compiled flag on each rule
 * - Populates fol_representation for each rule
 * - Computes integrity hash
 * - Sets kb->is_compiled = true
 *
 * ERROR CONDITIONS:
 * - Returns false if kb is NULL
 * - Returns false if KB is locked
 * - Returns false if compilation fails
 *
 * NOTE: Must be called before symbolic_logic_safety_lock()
 */
bool symbolic_logic_safety_compile_rules(safety_kb_t* kb);

//=============================================================================
// Memory Protection (Locking)
//=============================================================================

/**
 * @brief Lock the safety KB with mprotect (IRREVERSIBLE)
 *
 * WHAT: Apply mprotect(PROT_READ) to make rules read-only
 * WHY:  Prevent runtime modification of safety rules
 * HOW:  mprotect on mmap region
 *
 * @param kb Safety KB (non-NULL, compiled, not already locked)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 *
 * WARNING: THIS OPERATION IS IRREVERSIBLE
 * - Once locked, rules cannot be added, removed, or modified
 * - Only way to unlock is to destroy KB and create a new one
 *
 * ERROR CONDITIONS:
 * - Returns false if kb is NULL
 * - Returns false if not compiled (must compile first)
 * - Returns false if already locked
 * - Returns false if mprotect fails
 *
 * EXAMPLE:
 * ```c
 * // Add all rules first
 * symbolic_logic_safety_add_rule(kb, &rule1);
 * symbolic_logic_safety_add_rule(kb, &rule2);
 *
 * // Compile
 * symbolic_logic_safety_compile_rules(kb);
 *
 * // Lock (IRREVERSIBLE)
 * if (!symbolic_logic_safety_lock(kb)) {
 *     LOG_ERROR("Failed to lock safety KB");
 * }
 * // KB is now read-only
 * ```
 */
bool symbolic_logic_safety_lock(safety_kb_t* kb);

/**
 * @brief Check if safety KB is locked
 *
 * WHAT: Query lock status
 * WHY:  Allow code to check before attempting modifications
 * HOW:  Returns is_locked flag
 *
 * @param kb Safety KB (can be NULL)
 * @return true if locked, false if not locked or kb is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool symbolic_logic_safety_is_locked(const safety_kb_t* kb);

//=============================================================================
// Integrity Verification
//=============================================================================

/**
 * @brief Verify integrity of safety KB
 *
 * WHAT: Check that rules have not been tampered with
 * WHY:  Detect memory corruption or malicious modification
 * HOW:  Recompute SHA-256 and compare with stored hash
 *
 * @param kb Safety KB (non-NULL, compiled)
 * @return true if integrity verified, false if failed or error
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - Returns false if kb is NULL
 * - Returns false if not compiled (no hash to verify)
 * - Returns false if hash mismatch (TAMPERING DETECTED)
 *
 * NOTE: Should be called before critical evaluations
 */
bool symbolic_logic_safety_verify_integrity(const safety_kb_t* kb);

/**
 * @brief Get the integrity hash of the safety KB
 *
 * WHAT: Retrieve the SHA-256 hash of compiled rules
 * WHY:  Allow external verification and audit
 * HOW:  Copies hash to output buffer
 *
 * @param kb Safety KB (non-NULL, compiled)
 * @param hash_out Output buffer (must be SAFETY_HASH_SIZE bytes)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * ERROR CONDITIONS:
 * - Returns false if kb is NULL
 * - Returns false if hash_out is NULL
 * - Returns false if not compiled
 */
bool symbolic_logic_safety_get_hash(const safety_kb_t* kb, uint8_t* hash_out);

//=============================================================================
// Rule Evaluation
//=============================================================================

/**
 * @brief Evaluate an action against safety rules
 *
 * WHAT: Check if an action violates any safety rules
 * WHY:  Core safety enforcement function
 * HOW:  Forward chaining over rules, match conditions against context
 *
 * @param kb Safety KB (non-NULL, compiled)
 * @param context Action context to evaluate (non-NULL)
 * @param result Output evaluation result (non-NULL)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n * c) where n = rules, c = conditions
 * THREAD-SAFE: Yes (read-only on locked KB)
 *
 * ALGORITHM (Forward Chaining):
 * 1. Verify KB integrity
 * 2. For each enabled rule:
 *    a. If domain_hint set, skip rules with non-matching domain
 *    b. Evaluate all conditions against context
 *    c. If all conditions match, rule triggers
 * 3. Collect all triggered rules
 * 4. Return highest-priority action (DENY > ESCALATE > WARN > LOG > ALLOW)
 *
 * ERROR CONDITIONS:
 * - Returns false if kb is NULL
 * - Returns false if context is NULL
 * - Returns false if result is NULL
 * - Returns false if integrity check fails
 *
 * EXAMPLE:
 * ```c
 * safety_action_context_t ctx = {0};
 * strcpy(ctx.string_fields[0].key, "action_type");
 * strcpy(ctx.string_fields[0].value, "generate_text");
 * ctx.num_string_fields = 1;
 * strcpy(ctx.action_description, "Generate response about chemistry");
 *
 * safety_evaluation_t eval;
 * if (symbolic_logic_safety_evaluate(kb, &ctx, &eval)) {
 *     if (eval.action == SAFETY_ACTION_DENY) {
 *         printf("Action denied: %s\n", eval.explanation);
 *     }
 * }
 * ```
 */
bool symbolic_logic_safety_evaluate(
    const safety_kb_t* kb,
    const safety_action_context_t* context,
    safety_evaluation_t* result
);

/**
 * @brief Free resources in an evaluation result
 *
 * WHAT: Free dynamically allocated memory in evaluation result
 * WHY:  Prevent memory leaks
 * HOW:  Frees triggered_rule_ids array
 *
 * @param result Evaluation result to free (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void symbolic_logic_safety_free_evaluation(safety_evaluation_t* result);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get safety subsystem statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Enable monitoring and debugging
 * HOW:  Copies stats from internal counters
 *
 * @param kb Safety KB (non-NULL)
 * @param stats Output statistics structure (non-NULL)
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool symbolic_logic_safety_get_stats(const safety_kb_t* kb, safety_stats_t* stats);

/**
 * @brief Reset safety statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Allow fresh measurement periods
 * HOW:  Zero out counters
 *
 * @param kb Safety KB (non-NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 *
 * NOTE: Does not affect rule trigger counts (those are per-rule)
 */
void symbolic_logic_safety_reset_stats(safety_kb_t* kb);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Initialize a safety rule to default values
 *
 * WHAT: Zero-initialize a rule and set defaults
 * WHY:  Ensure clean state before populating rule
 * HOW:  memset + default values
 *
 * @param rule Rule to initialize (non-NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void symbolic_logic_safety_init_rule(safety_rule_t* rule);

/**
 * @brief Initialize an action context to default values
 *
 * WHAT: Zero-initialize a context
 * WHY:  Ensure clean state before populating context
 * HOW:  memset
 *
 * @param context Context to initialize (non-NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void symbolic_logic_safety_init_context(safety_action_context_t* context);

/**
 * @brief Add a string field to an action context
 *
 * WHAT: Add a key-value string pair to context
 * WHY:  Convenience function for building contexts
 * HOW:  Copies to next available slot
 *
 * @param context Context to modify (non-NULL)
 * @param key Field key (non-NULL)
 * @param value Field value (non-NULL)
 * @return true on success, false if context is full
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool symbolic_logic_safety_context_add_string(
    safety_action_context_t* context,
    const char* key,
    const char* value
);

/**
 * @brief Add a numeric field to an action context
 *
 * WHAT: Add a key-value numeric pair to context
 * WHY:  Convenience function for building contexts
 * HOW:  Copies to next available slot
 *
 * @param context Context to modify (non-NULL)
 * @param key Field key (non-NULL)
 * @param value Field value
 * @return true on success, false if context is full
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool symbolic_logic_safety_context_add_numeric(
    safety_action_context_t* context,
    const char* key,
    float value
);

/**
 * @brief Print rule to debug output
 *
 * WHAT: Log rule details for debugging
 * WHY:  Aid in development and troubleshooting
 * HOW:  Uses LOG_DEBUG
 *
 * @param rule Rule to print (non-NULL)
 *
 * COMPLEXITY: O(c) where c = conditions
 * THREAD-SAFE: Yes
 */
void symbolic_logic_safety_print_rule(const safety_rule_t* rule);

/**
 * @brief Print evaluation result to debug output
 *
 * WHAT: Log evaluation details for debugging
 * WHY:  Aid in development and troubleshooting
 * HOW:  Uses LOG_DEBUG
 *
 * @param result Evaluation to print (non-NULL)
 *
 * COMPLEXITY: O(n) where n = triggered rules
 * THREAD-SAFE: Yes
 */
void symbolic_logic_safety_print_evaluation(const safety_evaluation_t* result);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_SAFETY_H
