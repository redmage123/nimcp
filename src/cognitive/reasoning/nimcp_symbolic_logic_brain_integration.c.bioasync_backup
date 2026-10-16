/**
 * @file nimcp_symbolic_logic_brain_integration.c
 * @brief Brain-level symbolic logic reasoning integration implementation
 *
 * @author NIMCP Development Team - Phase 9.4 Integration
 * @date 2025-01-20
 * @version 2.6.2
 */

#include "cognitive/reasoning/nimcp_symbolic_logic_brain_integration.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

//=============================================================================
// Error Handling
//=============================================================================

static __thread char last_error[256] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

const char* brain_logic_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Default Configuration
//=============================================================================

static logic_brain_config_t get_default_config(void)
{
    logic_brain_config_t config = {
        .max_facts = 1000,
        .max_rules = 500,
        .max_inference_depth = 10,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_wm_integration = true,
        .enable_exec_integration = true,
        .wm_inference_salience = 0.7f
    };
    return config;
}

//=============================================================================
// Brain API - Lifecycle
//=============================================================================

bool brain_create_symbolic_logic(
    brain_t brain,
    const logic_brain_config_t* config)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    // Check if already initialized
    if (brain->logic_engine) {
        set_error("Symbolic logic engine already initialized");
        return false;
    }

    // Use defaults if no config provided
    logic_brain_config_t actual_config = config ? *config : get_default_config();

    // Create symbolic logic engine configuration
    logic_config_t logic_config = {
        .max_predicates = 1000,
        .max_rules = actual_config.max_rules,
        .max_kb_size = actual_config.max_facts,
        .max_inference_depth = actual_config.max_inference_depth,
        .enable_forward_chaining = actual_config.enable_forward_chaining,
        .enable_backward_chaining = actual_config.enable_backward_chaining,
        .enable_resolution = true,
        .enable_memory_consolidation = true
    };

    // Create symbolic logic engine
    brain->logic_engine = symbolic_logic_create(&logic_config);
    if (!brain->logic_engine) {
        set_error("Failed to create symbolic logic engine");
        return false;
    }

    NIMCP_LOGGING_INFO("Symbolic logic engine created: max_facts=%u, max_rules=%u, depth=%u",
                       actual_config.max_facts,
                       actual_config.max_rules,
                       actual_config.max_inference_depth);

    return true;
}

void brain_destroy_symbolic_logic(brain_t brain)
{
    if (!brain) return;

    if (brain->logic_engine) {
        symbolic_logic_destroy(brain->logic_engine);
        brain->logic_engine = NULL;
        NIMCP_LOGGING_INFO("Symbolic logic engine destroyed");
    }
}

//=============================================================================
// Brain API - Knowledge Base Operations
//=============================================================================

bool brain_add_logical_fact(
    brain_t brain,
    const char* fact_str,
    float salience)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized - call brain_create_symbolic_logic first");
        return false;
    }

    if (!nimcp_validate_pointer(fact_str, "fact_str")) {
        set_error("Fact string is NULL");
        return false;
    }

    if (salience < 0.0f || salience > 1.0f) {
        set_error("Salience must be in range [0,1], got %.2f", salience);
        return false;
    }

    // Parse fact string to logical formula
    logical_formula_t* formula = symbolic_logic_parse(fact_str);
    if (!formula) {
        set_error("Failed to parse fact: %s", fact_str);
        return false;
    }

    // Convert to CNF clauses
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = symbolic_logic_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert fact to CNF: %s", fact_str);
        return false;
    }

    // Add all clauses to knowledge base
    for (int i = 0; i < num_clauses; i++) {
        if (!symbolic_logic_add_fact(brain->logic_engine, clauses[i], salience)) {
            set_error("Failed to add fact clause %d to knowledge base", i);
            // Clean up remaining clauses
            for (int j = i; j < num_clauses; j++) {
                nimcp_free(clauses[j]);
            }
            nimcp_free(clauses);
            return false;
        }
    }

    nimcp_free(clauses);

    // Integrate with working memory if enabled
    if (brain->working_memory && brain->config.enable_working_memory) {
        // Create a simple float representation for working memory
        // (In a real implementation, you'd encode the logical structure)
        float fact_encoding[4] = {salience, 1.0f, 0.0f, 0.0f}; // Simple encoding
        working_memory_add_item(brain->working_memory, fact_encoding, 4, salience);
        NIMCP_LOGGING_DEBUG("Fact added to working memory: %s (salience=%.2f)", fact_str, salience);
    }

    NIMCP_LOGGING_INFO("Logical fact added: %s (salience=%.2f)", fact_str, salience);
    return true;
}

bool brain_add_logical_rule(
    brain_t brain,
    const char* rule_str,
    float priority)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(rule_str, "rule_str")) {
        set_error("Rule string is NULL");
        return false;
    }

    if (priority < 0.0f || priority > 1.0f) {
        set_error("Priority must be in range [0,1], got %.2f", priority);
        return false;
    }

    // Parse rule string
    // Expected format: "premise1 & premise2 -> conclusion"
    logical_formula_t* formula = symbolic_logic_parse(rule_str);
    if (!formula) {
        set_error("Failed to parse rule: %s", rule_str);
        return false;
    }

    // Extract premises and conclusion from implication
    if (formula->op != OP_IMPLIES) {
        set_error("Rule must be an implication (->): %s", rule_str);
        logic_formula_destroy(formula);
        return false;
    }

    // Convert premises and conclusion to CNF
    logic_clause_t** premise_clauses = NULL;
    int num_premises = 0;
    bool success = symbolic_logic_to_cnf(formula->left, &premise_clauses, &num_premises);

    logic_clause_t* conclusion_clause = NULL;
    int num_conclusions = 0;
    logic_clause_t** conclusion_clauses = NULL;
    if (success) {
        success = symbolic_logic_to_cnf(formula->right, &conclusion_clauses, &num_conclusions);
        if (success && num_conclusions > 0) {
            conclusion_clause = conclusion_clauses[0]; // Take first conclusion
        }
    }

    logic_formula_destroy(formula);

    if (!success || !conclusion_clause) {
        set_error("Failed to convert rule to CNF: %s", rule_str);
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        return false;
    }

    // Create inference rule
    inference_rule_t* rule = (inference_rule_t*)nimcp_calloc(1, sizeof(inference_rule_t));
    if (!rule) {
        set_error("Failed to allocate inference rule");
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        return false;
    }

    strncpy(rule->name, rule_str, LOGIC_MAX_NAME_LENGTH - 1);
    rule->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
    rule->premises = premise_clauses;
    rule->num_premises = num_premises;
    rule->conclusion = conclusion_clause;
    rule->priority = priority;

    // Add rule to logic engine
    if (!symbolic_logic_add_rule(brain->logic_engine, rule)) {
        set_error("Failed to add rule to logic engine");
        nimcp_free(rule);
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        return false;
    }

    nimcp_free(conclusion_clauses); // Only needed the first element

    NIMCP_LOGGING_INFO("Logical rule added: %s (priority=%.2f)", rule_str, priority);
    return true;
}

bool brain_query_knowledge(
    brain_t brain,
    const char* query_str,
    query_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(query_str, "query_str")) {
        set_error("Query string is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(query_result_t));

    // Parse query
    logical_formula_t* formula = symbolic_logic_parse(query_str);
    if (!formula) {
        set_error("Failed to parse query: %s", query_str);
        return false;
    }

    // Convert to CNF clause for querying
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = symbolic_logic_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert query to CNF: %s", query_str);
        return false;
    }

    // Query the knowledge base with first clause
    kb_entry_t** matches = NULL;
    int num_matches = 0;
    success = symbolic_logic_query(brain->logic_engine, clauses[0], &matches, &num_matches);

    // Clean up query clauses
    for (int i = 0; i < num_clauses; i++) {
        nimcp_free(clauses[i]);
    }
    nimcp_free(clauses);

    if (!success) {
        set_error("Query execution failed: %s", query_str);
        return false;
    }

    // Populate result
    result->success = true;
    result->matches = matches;
    result->num_matches = num_matches;
    result->bindings = NULL; // TODO: Implement variable binding extraction
    result->num_bindings = 0;

    NIMCP_LOGGING_DEBUG("Query executed: %s (matches=%d)", query_str, num_matches);
    return true;
}

void brain_free_query_result(query_result_t* result)
{
    if (!result) return;

    if (result->matches) {
        nimcp_free(result->matches);
        result->matches = NULL;
    }

    if (result->bindings) {
        for (int i = 0; i < result->num_bindings; i++) {
            if (result->bindings[i]) {
                nimcp_free(result->bindings[i]);
            }
        }
        nimcp_free(result->bindings);
        result->bindings = NULL;
    }

    memset(result, 0, sizeof(query_result_t));
}

//=============================================================================
// Brain API - Inference Operations
//=============================================================================

bool brain_forward_chain(
    brain_t brain,
    uint32_t max_iterations,
    inference_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(inference_result_t));

    // Cap max iterations
    if (max_iterations == 0 || max_iterations > 1000) {
        max_iterations = 1000;
    }

    // Create executive task if enabled
    uint32_t task_id = 0;
    if (brain->executive && brain->config.enable_executive) {
        task_descriptor_t task = {
            .type = TASK_TYPE_REASONING,
            .priority = PRIORITY_NORMAL,
            .status = TASK_STATUS_PENDING,
            .steps_total = max_iterations
        };
        strncpy(task.name, "Forward Chaining", sizeof(task.name) - 1);
        task_id = executive_add_task(brain->executive, &task);
    }

    // Perform forward chaining
    uint64_t start_time = nimcp_time_monotonic_ms();
    logic_clause_t** new_facts = NULL;
    int num_new_facts = 0;

    bool success = symbolic_logic_forward_chain(
        brain->logic_engine,
        max_iterations,
        &new_facts,
        &num_new_facts
    );

    uint64_t end_time = nimcp_time_monotonic_ms();

    if (!success) {
        set_error("Forward chaining failed");
        if (task_id > 0 && brain->executive) {
            executive_mark_task_failed(brain->executive, task_id);
        }
        return false;
    }

    // Store new facts in working memory if enabled
    if (brain->working_memory && brain->config.enable_working_memory && num_new_facts > 0) {
        for (int i = 0; i < num_new_facts && i < 7; i++) { // Limit to WM capacity
            float fact_encoding[4] = {0.7f, 1.0f, (float)i, 0.0f};
            working_memory_add_item(brain->working_memory, fact_encoding, 4, 0.7f);
        }
        NIMCP_LOGGING_DEBUG("Stored %d new inferences in working memory",
                           (num_new_facts < 7) ? num_new_facts : 7);
    }

    // Populate result
    result->conclusion = (num_new_facts > 0) ? new_facts[0] : NULL;
    result->proof_steps = NULL; // Forward chaining doesn't produce explicit proof trace
    result->num_steps = 0;
    result->confidence = (num_new_facts > 0) ? 0.8f : 0.0f;
    result->inference_time_ms = end_time - start_time;

    // Complete executive task if enabled
    if (task_id > 0 && brain->executive) {
        executive_mark_task_completed(brain->executive, task_id);
    }

    NIMCP_LOGGING_INFO("Forward chaining completed: %d new facts derived in %llu ms",
                       num_new_facts, (unsigned long long)(end_time - start_time));

    // Free the new_facts array (but keep first for result)
    if (new_facts) {
        for (int i = 1; i < num_new_facts; i++) {
            if (new_facts[i]) {
                nimcp_free(new_facts[i]);
            }
        }
        nimcp_free(new_facts);
    }

    return true;
}

bool brain_backward_chain(
    brain_t brain,
    const char* goal_str,
    inference_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(goal_str, "goal_str")) {
        set_error("Goal string is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(inference_result_t));

    // Parse goal
    logical_formula_t* formula = symbolic_logic_parse(goal_str);
    if (!formula) {
        set_error("Failed to parse goal: %s", goal_str);
        return false;
    }

    // Convert to CNF clause
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = symbolic_logic_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert goal to CNF: %s", goal_str);
        return false;
    }

    // Create executive task for planning
    uint32_t task_id = 0;
    if (brain->executive && brain->config.enable_executive) {
        task_descriptor_t task = {
            .type = TASK_TYPE_PLANNING,
            .priority = PRIORITY_HIGH,
            .status = TASK_STATUS_PENDING
        };
        strncpy(task.name, "Backward Chaining Proof", sizeof(task.name) - 1);
        task_id = executive_add_task(brain->executive, &task);
    }

    // Perform backward chaining
    uint64_t start_time = nimcp_time_monotonic_ms();
    inference_rule_t** proof_trace = NULL;
    int num_steps = 0;

    success = symbolic_logic_backward_chain(
        brain->logic_engine,
        clauses[0],
        &proof_trace,
        &num_steps
    );

    uint64_t end_time = nimcp_time_monotonic_ms();

    // Clean up goal clauses
    for (int i = 0; i < num_clauses; i++) {
        nimcp_free(clauses[i]);
    }
    nimcp_free(clauses);

    if (!success) {
        NIMCP_LOGGING_INFO("Goal not proven: %s", goal_str);
        if (task_id > 0 && brain->executive) {
            executive_mark_task_failed(brain->executive, task_id);
        }
        return false; // Goal not provable
    }

    // Store proof in working memory if enabled
    if (brain->working_memory && brain->config.enable_working_memory && num_steps > 0) {
        float proof_encoding[4] = {1.0f, 0.0f, (float)num_steps, 0.0f};
        working_memory_add_item(brain->working_memory, proof_encoding, 4, 0.9f);
        NIMCP_LOGGING_DEBUG("Stored proof trace in working memory (steps=%d)", num_steps);
    }

    // Populate result
    result->conclusion = NULL; // Backward chaining proves existing goal
    result->proof_steps = proof_trace;
    result->num_steps = num_steps;
    result->confidence = 0.95f; // High confidence if proof found
    result->inference_time_ms = end_time - start_time;

    // Complete executive task
    if (task_id > 0 && brain->executive) {
        executive_mark_task_completed(brain->executive, task_id);
    }

    NIMCP_LOGGING_INFO("Backward chaining successful: goal '%s' proven in %d steps (%llu ms)",
                       goal_str, num_steps, (unsigned long long)(end_time - start_time));

    return true;
}

void brain_free_inference_result(inference_result_t* result)
{
    if (!result) return;

    if (result->conclusion) {
        nimcp_free(result->conclusion);
        result->conclusion = NULL;
    }

    if (result->proof_steps) {
        nimcp_free(result->proof_steps);
        result->proof_steps = NULL;
    }

    memset(result, 0, sizeof(inference_result_t));
}

//=============================================================================
// Brain API - Statistics and Diagnostics
//=============================================================================

bool brain_get_logic_stats(
    brain_t brain,
    logic_stats_t* stats)
{
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(stats, "stats")) {
        set_error("Stats pointer is NULL");
        return false;
    }

    return symbolic_logic_get_stats(brain->logic_engine, stats);
}

bool brain_export_knowledge_base(
    brain_t brain,
    const char* filepath)
{
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(filepath, "filepath")) {
        set_error("Filepath is NULL");
        return false;
    }

    // TODO: Implement knowledge base export
    set_error("Knowledge base export not yet implemented");
    return false;
}

bool brain_import_knowledge_base(
    brain_t brain,
    const char* filepath)
{
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        return false;
    }

    if (!brain->logic_engine) {
        set_error("Symbolic logic engine not initialized");
        return false;
    }

    if (!nimcp_validate_pointer(filepath, "filepath")) {
        set_error("Filepath is NULL");
        return false;
    }

    // TODO: Implement knowledge base import
    set_error("Knowledge base import not yet implemented");
    return false;
}
