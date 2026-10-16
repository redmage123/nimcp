/**
 * @file nimcp_knowledge_base_interface.c
 * @brief MODULE 2: Knowledge Base Interface implementation
 *
 * SINGLE RESPONSIBILITY: Add/query facts and rules through brain interface
 *
 * @author NIMCP Development Team - SRP Refactoring
 * @date 2025-11-20
 * @version 3.0.0
 */

#include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "core/events/nimcp_event_bus.h"
#include "cognitive/nimcp_working_memory.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

//=============================================================================
// Event Types
//=============================================================================

#define EVENT_FACT_ADDED 0x0942
#define EVENT_RULE_ADDED 0x0943
#define EVENT_QUERY_EXECUTED 0x0944

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

const char* kb_interface_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Parse fact string to logical formula
 */
static logical_formula_t* parse_fact(const char* fact_str)
{
    // Delegate to symbolic logic parser
    return symbolic_logic_parse(fact_str);
}

/**
 * @brief Convert formula to CNF clauses
 */
static bool formula_to_cnf(logical_formula_t* formula,
                          logic_clause_t*** clauses,
                          int* num_clauses)
{
    return symbolic_logic_to_cnf(formula, clauses, num_clauses);
}

//=============================================================================
// Knowledge Base Operations Implementation
//=============================================================================

bool brain_add_fact(
    brain_t brain,
    const char* fact_str,
    float salience)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_add_fact: brain is NULL");
        return false;
    }

    // Check if logic engine attached
    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached - call brain_attach_symbolic_logic first");
        NIMCP_LOGGING_ERROR("brain_add_fact: no logic engine attached to brain %p", (void*)brain);
        return false;
    }

    if (!nimcp_validate_pointer(fact_str, "fact_str")) {
        set_error("Fact string is NULL");
        NIMCP_LOGGING_ERROR("brain_add_fact: fact_str is NULL");
        return false;
    }

    if (salience < 0.0f || salience > 1.0f) {
        set_error("Salience must be in range [0,1], got %.2f", salience);
        NIMCP_LOGGING_ERROR("brain_add_fact: invalid salience %.2f", salience);
        return false;
    }

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse fact string to logical formula
    logical_formula_t* formula = parse_fact(fact_str);
    if (!formula) {
        set_error("Failed to parse fact: %s", fact_str);
        NIMCP_LOGGING_ERROR("brain_add_fact: parse failed for '%s'", fact_str);
        return false;
    }

    // Convert to CNF clauses
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = formula_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert fact to CNF: %s", fact_str);
        NIMCP_LOGGING_ERROR("brain_add_fact: CNF conversion failed for '%s'", fact_str);
        return false;
    }

    // Add all clauses to knowledge base
    for (int i = 0; i < num_clauses; i++) {
        if (!symbolic_logic_add_fact(engine, clauses[i], salience)) {
            set_error("Failed to add fact clause %d to knowledge base", i);
            NIMCP_LOGGING_ERROR("brain_add_fact: failed to add clause %d", i);

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
        float fact_encoding[4] = {salience, 1.0f, 0.0f, 0.0f};
        working_memory_add_item(brain->working_memory, fact_encoding, 4, salience);
        NIMCP_LOGGING_DEBUG("Fact added to working memory: %s (salience=%.2f)",
                           fact_str, salience);
    }

    // Publish event
    event_data_t event = {
        .event_type = EVENT_FACT_ADDED,
        .timestamp = 0,
        .priority = EVENT_PRIORITY_NORMAL,
        .data_size = strlen(fact_str) + 1,
        .data = (void*)fact_str
    };

    if (brain->event_bus) {
        event_bus_publish(brain->event_bus, &event);
    }

    NIMCP_LOGGING_INFO("Logical fact added: %s (salience=%.2f)", fact_str, salience);
    return true;
}

bool brain_add_rule(
    brain_t brain,
    const char* rule_str,
    float priority)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_add_rule: brain is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_add_rule: no logic engine attached to brain %p", (void*)brain);
        return false;
    }

    if (!nimcp_validate_pointer(rule_str, "rule_str")) {
        set_error("Rule string is NULL");
        NIMCP_LOGGING_ERROR("brain_add_rule: rule_str is NULL");
        return false;
    }

    if (priority < 0.0f || priority > 1.0f) {
        set_error("Priority must be in range [0,1], got %.2f", priority);
        NIMCP_LOGGING_ERROR("brain_add_rule: invalid priority %.2f", priority);
        return false;
    }

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse rule string (expected format: "premise1 & premise2 -> conclusion")
    logical_formula_t* formula = parse_fact(rule_str);
    if (!formula) {
        set_error("Failed to parse rule: %s", rule_str);
        NIMCP_LOGGING_ERROR("brain_add_rule: parse failed for '%s'", rule_str);
        return false;
    }

    // Extract premises and conclusion from implication
    if (formula->op != OP_IMPLIES) {
        set_error("Rule must be an implication (->): %s", rule_str);
        NIMCP_LOGGING_ERROR("brain_add_rule: not an implication '%s'", rule_str);
        logic_formula_destroy(formula);
        return false;
    }

    // Convert premises and conclusion to CNF
    logic_clause_t** premise_clauses = NULL;
    int num_premises = 0;
    bool success = formula_to_cnf(formula->left, &premise_clauses, &num_premises);

    logic_clause_t* conclusion_clause = NULL;
    int num_conclusions = 0;
    logic_clause_t** conclusion_clauses = NULL;
    if (success) {
        success = formula_to_cnf(formula->right, &conclusion_clauses, &num_conclusions);
        if (success && num_conclusions > 0) {
            conclusion_clause = conclusion_clauses[0]; // Take first conclusion
        }
    }

    logic_formula_destroy(formula);

    if (!success || !conclusion_clause) {
        set_error("Failed to convert rule to CNF: %s", rule_str);
        NIMCP_LOGGING_ERROR("brain_add_rule: CNF conversion failed for '%s'", rule_str);
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        return false;
    }

    // Create inference rule
    inference_rule_t* rule = (inference_rule_t*)nimcp_calloc(1, sizeof(inference_rule_t));
    if (!rule) {
        set_error("Failed to allocate inference rule");
        NIMCP_LOGGING_ERROR("brain_add_rule: allocation failed");
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
    if (!symbolic_logic_add_rule(engine, rule)) {
        set_error("Failed to add rule to logic engine");
        NIMCP_LOGGING_ERROR("brain_add_rule: failed to add to engine");
        nimcp_free(rule);
        if (premise_clauses) nimcp_free(premise_clauses);
        if (conclusion_clauses) nimcp_free(conclusion_clauses);
        return false;
    }

    nimcp_free(conclusion_clauses); // Only needed the first element

    // Publish event
    event_data_t event = {
        .event_type = EVENT_RULE_ADDED,
        .timestamp = 0,
        .priority = EVENT_PRIORITY_NORMAL,
        .data_size = strlen(rule_str) + 1,
        .data = (void*)rule_str
    };

    if (brain->event_bus) {
        event_bus_publish(brain->event_bus, &event);
    }

    NIMCP_LOGGING_INFO("Logical rule added: %s (priority=%.2f)", rule_str, priority);
    return true;
}

bool brain_query_knowledge(
    brain_t brain,
    const char* query_str,
    kb_query_result_t* result)
{
    // Validate inputs
    if (!nimcp_validate_pointer(brain, "brain")) {
        set_error("Brain is NULL");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: brain is NULL");
        return false;
    }

    if (!brain_has_symbolic_logic(brain)) {
        set_error("Symbolic logic engine not attached");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: no logic engine attached");
        return false;
    }

    if (!nimcp_validate_pointer(query_str, "query_str")) {
        set_error("Query string is NULL");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: query_str is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(result, "result")) {
        set_error("Result pointer is NULL");
        NIMCP_LOGGING_ERROR("brain_query_knowledge: result is NULL");
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(kb_query_result_t));

    // Get logic engine
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    // Parse query
    logical_formula_t* formula = parse_fact(query_str);
    if (!formula) {
        set_error("Failed to parse query: %s", query_str);
        NIMCP_LOGGING_ERROR("brain_query_knowledge: parse failed for '%s'", query_str);
        return false;
    }

    // Convert to CNF clause for querying
    logic_clause_t** clauses = NULL;
    int num_clauses = 0;
    bool success = formula_to_cnf(formula, &clauses, &num_clauses);
    logic_formula_destroy(formula);

    if (!success || num_clauses == 0) {
        set_error("Failed to convert query to CNF: %s", query_str);
        NIMCP_LOGGING_ERROR("brain_query_knowledge: CNF conversion failed for '%s'", query_str);
        return false;
    }

    // Query the knowledge base with first clause
    kb_entry_t** matches = NULL;
    int num_matches = 0;
    success = symbolic_logic_query(engine, clauses[0], &matches, &num_matches);

    // Clean up query clauses
    for (int i = 0; i < num_clauses; i++) {
        nimcp_free(clauses[i]);
    }
    nimcp_free(clauses);

    if (!success) {
        set_error("Query execution failed: %s", query_str);
        NIMCP_LOGGING_ERROR("brain_query_knowledge: query execution failed");
        return false;
    }

    // Populate result
    result->success = true;
    result->matches = matches;
    result->num_matches = num_matches;
    result->bindings = NULL; // TODO: Implement variable binding extraction
    result->num_bindings = 0;

    // Publish event
    event_data_t event = {
        .event_type = EVENT_QUERY_EXECUTED,
        .timestamp = 0,
        .priority = EVENT_PRIORITY_LOW,
        .data_size = strlen(query_str) + 1,
        .data = (void*)query_str
    };

    if (brain->event_bus) {
        event_bus_publish(brain->event_bus, &event);
    }

    NIMCP_LOGGING_DEBUG("Query executed: %s (matches=%d)", query_str, num_matches);
    return true;
}

void kb_free_query_result(kb_query_result_t* result)
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

    memset(result, 0, sizeof(kb_query_result_t));
}

uint32_t brain_get_fact_count(brain_t brain)
{
    if (!brain || !brain_has_symbolic_logic(brain)) {
        return 0;
    }

    logic_stats_t stats;
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    if (symbolic_logic_get_stats(engine, &stats)) {
        return stats.facts_stored;
    }

    return 0;
}

uint32_t brain_get_rule_count(brain_t brain)
{
    if (!brain || !brain_has_symbolic_logic(brain)) {
        return 0;
    }

    logic_stats_t stats;
    symbolic_logic_t* engine = brain_get_symbolic_logic(brain);

    if (symbolic_logic_get_stats(engine, &stats)) {
        return stats.rules_applied; // Note: This is "rules_applied", should be "rules_stored"
    }

    return 0;
}
