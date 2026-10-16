/**
 * @file nimcp_symbolic_logic.c
 * @brief Symbolic logic engine implementation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6
 */

#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <ctype.h>
#include <math.h>

//=============================================================================
// Internal Structures
//=============================================================================

struct symbolic_logic {
    logic_config_t config;

    // Knowledge base
    kb_entry_t** kb;
    uint32_t num_facts;
    uint32_t kb_capacity;

    // Rules
    inference_rule_t** rules;
    uint32_t num_rules;
    uint32_t rules_capacity;

    // Statistics
    logic_stats_t stats;

    // Working memory for inference
    logic_clause_t** working_memory;
    uint32_t working_memory_size;
};

//=============================================================================
// Helper Functions - Term Management
//=============================================================================

logical_term_t* logic_term_create(term_type_t type, const char* name)
{
    if (!name) {
        return NULL;
    }

    logical_term_t* term = (logical_term_t*)nimcp_calloc(1, sizeof(logical_term_t));
    if (!term) {
        return NULL;
    }

    term->type = type;
    strncpy(term->name, name, LOGIC_MAX_NAME_LENGTH - 1);
    term->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
    term->args = NULL;
    term->arity = 0;

    return term;
}

void logic_term_destroy(logical_term_t* term)
{
    if (!term) return;

    if (term->args) {
        for (uint8_t i = 0; i < term->arity; i++) {
            logic_term_destroy(term->args[i]);
        }
        nimcp_free(term->args);
    }

    nimcp_free(term);
}

static logical_term_t* logic_term_copy(const logical_term_t* term)
{
    if (!term) return NULL;

    logical_term_t* copy = logic_term_create(term->type, term->name);
    if (!copy) return NULL;

    if (term->arity > 0 && term->args) {
        copy->arity = term->arity;
        copy->args = (logical_term_t**)nimcp_calloc(term->arity, sizeof(logical_term_t*));
        if (!copy->args) {
            nimcp_free(copy);
            return NULL;
        }

        for (uint8_t i = 0; i < term->arity; i++) {
            copy->args[i] = logic_term_copy(term->args[i]);
            if (!copy->args[i]) {
                logic_term_destroy(copy);
                return NULL;
            }
        }
    }

    return copy;
}

static bool terms_equal(const logical_term_t* t1, const logical_term_t* t2)
{
    if (!t1 || !t2) return false;
    if (t1->type != t2->type) return false;
    if (strcmp(t1->name, t2->name) != 0) return false;
    if (t1->arity != t2->arity) return false;

    for (uint8_t i = 0; i < t1->arity; i++) {
        if (!terms_equal(t1->args[i], t2->args[i])) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Helper Functions - Atomic Formula Management
//=============================================================================

atomic_formula_t* logic_atom_create(const char* name, logical_term_t** terms, uint8_t arity)
{
    if (!name) {
        return NULL;
    }

    if (arity > LOGIC_MAX_ARITY) {
        NIMCP_LOGGING_ERROR("Arity %d exceeds maximum %d", arity, LOGIC_MAX_ARITY);
        return NULL;
    }

    atomic_formula_t* atom = (atomic_formula_t*)nimcp_calloc(1, sizeof(atomic_formula_t));
    if (!atom) {
        return NULL;
    }

    strncpy(atom->name, name, LOGIC_MAX_NAME_LENGTH - 1);
    atom->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
    atom->arity = arity;
    atom->negated = false;

    if (arity > 0 && terms) {
        atom->terms = (logical_term_t**)nimcp_calloc(arity, sizeof(logical_term_t*));
        if (!atom->terms) {
            nimcp_free(atom);
            return NULL;
        }

        for (uint8_t i = 0; i < arity; i++) {
            atom->terms[i] = logic_term_copy(terms[i]);
            if (!atom->terms[i]) {
                logic_atom_destroy(atom);
                return NULL;
            }
        }
    }

    return atom;
}

void logic_atom_destroy(atomic_formula_t* atom)
{
    if (!atom) return;

    if (atom->terms) {
        for (uint8_t i = 0; i < atom->arity; i++) {
            logic_term_destroy(atom->terms[i]);
        }
        nimcp_free(atom->terms);
    }

    nimcp_free(atom);
}

static atomic_formula_t* logic_atom_copy(const atomic_formula_t* atom)
{
    if (!atom) return NULL;

    atomic_formula_t* copy = logic_atom_create(atom->name, atom->terms, atom->arity);
    if (copy) {
        copy->negated = atom->negated;
    }

    return copy;
}

static bool atoms_equal(const atomic_formula_t* a1, const atomic_formula_t* a2)
{
    if (!a1 || !a2) return false;
    if (strcmp(a1->name, a2->name) != 0) return false;
    if (a1->arity != a2->arity) return false;
    if (a1->negated != a2->negated) return false;

    for (uint8_t i = 0; i < a1->arity; i++) {
        if (!terms_equal(a1->terms[i], a2->terms[i])) {
            return false;
        }
    }

    return true;
}

//=============================================================================
// Helper Functions - Clause Management
//=============================================================================

static logic_clause_t* logic_clause_create(uint32_t num_literals)
{
    logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!clause) return NULL;

    if (num_literals > 0) {
        clause->literals = (atomic_formula_t**)nimcp_calloc(num_literals, sizeof(atomic_formula_t*));
        if (!clause->literals) {
            nimcp_free(clause);
            return NULL;
        }
    }

    clause->num_literals = num_literals;
    clause->confidence = 1.0f;

    return clause;
}

static void logic_clause_destroy(logic_clause_t* clause)
{
    if (!clause) return;

    if (clause->literals) {
        for (uint32_t i = 0; i < clause->num_literals; i++) {
            logic_atom_destroy(clause->literals[i]);
        }
        nimcp_free(clause->literals);
    }

    nimcp_free(clause);
}

static logic_clause_t* logic_clause_copy(const logic_clause_t* clause)
{
    if (!clause) return NULL;

    logic_clause_t* copy = logic_clause_create(clause->num_literals);
    if (!copy) return NULL;

    copy->confidence = clause->confidence;

    for (uint32_t i = 0; i < clause->num_literals; i++) {
        copy->literals[i] = logic_atom_copy(clause->literals[i]);
        if (!copy->literals[i]) {
            logic_clause_destroy(copy);
            return NULL;
        }
    }

    return copy;
}

//=============================================================================
// Core API Implementation
//=============================================================================

symbolic_logic_t* symbolic_logic_create(const logic_config_t* config)
{
    if (!nimcp_validate_pointer(config, "config")) {
        return NULL;
    }

    if (config->max_predicates == 0 || config->max_predicates > LOGIC_MAX_PREDICATES) {
        NIMCP_LOGGING_ERROR("Invalid max_predicates: %u", config->max_predicates);
        return NULL;
    }

    symbolic_logic_t* logic = (symbolic_logic_t*)nimcp_calloc(1, sizeof(symbolic_logic_t));
    if (!logic) {
        return NULL;
    }

    logic->config = *config;

    // Initialize knowledge base
    logic->kb_capacity = config->max_kb_size;
    logic->kb = (kb_entry_t**)nimcp_calloc(logic->kb_capacity, sizeof(kb_entry_t*));
    if (!logic->kb) {
        symbolic_logic_destroy(logic);
        return NULL;
    }

    // Initialize rules
    logic->rules_capacity = config->max_rules;
    logic->rules = (inference_rule_t**)nimcp_calloc(logic->rules_capacity, sizeof(inference_rule_t*));
    if (!logic->rules) {
        symbolic_logic_destroy(logic);
        return NULL;
    }

    // Initialize working memory
    logic->working_memory = (logic_clause_t**)nimcp_calloc(LOGIC_MAX_PREDICATES, sizeof(logic_clause_t*));
    if (!logic->working_memory) {
        symbolic_logic_destroy(logic);
        return NULL;
    }

    memset(&logic->stats, 0, sizeof(logic_stats_t));

    NIMCP_LOGGING_INFO("Symbolic logic engine created");
    return logic;
}

void symbolic_logic_destroy(symbolic_logic_t* logic)
{
    if (!logic) return;

    // Free knowledge base
    if (logic->kb) {
        for (uint32_t i = 0; i < logic->num_facts; i++) {
            if (logic->kb[i]) {
                logic_clause_destroy(logic->kb[i]->clause);
                nimcp_free(logic->kb[i]);
            }
        }
        nimcp_free(logic->kb);
    }

    // Free rules
    if (logic->rules) {
        for (uint32_t i = 0; i < logic->num_rules; i++) {
            if (logic->rules[i]) {
                for (uint32_t j = 0; j < logic->rules[i]->num_premises; j++) {
                    logic_clause_destroy(logic->rules[i]->premises[j]);
                }
                nimcp_free(logic->rules[i]->premises);
                logic_clause_destroy(logic->rules[i]->conclusion);
                nimcp_free(logic->rules[i]);
            }
        }
        nimcp_free(logic->rules);
    }

    // Free working memory
    if (logic->working_memory) {
        for (uint32_t i = 0; i < logic->working_memory_size; i++) {
            logic_clause_destroy(logic->working_memory[i]);
        }
        nimcp_free(logic->working_memory);
    }

    nimcp_free(logic);
}

bool symbolic_logic_get_stats(const symbolic_logic_t* logic, logic_stats_t* stats)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(stats, "stats")) {
        return false;
    }

    *stats = logic->stats;
    return true;
}

//=============================================================================
// Knowledge Base Management
//=============================================================================

bool symbolic_logic_add_fact(symbolic_logic_t* logic, logic_clause_t* clause, float salience)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(clause, "clause")) {
        return false;
    }

    if (salience < 0.0f || salience > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid salience value: %f (must be [0,1])", salience);
        return false;
    }

    if (logic->num_facts >= logic->kb_capacity) {
        NIMCP_LOGGING_ERROR("Knowledge base full");
        return false;
    }

    kb_entry_t* entry = (kb_entry_t*)nimcp_calloc(1, sizeof(kb_entry_t));
    if (!entry) {
        return false;
    }

    entry->clause = logic_clause_copy(clause);
    if (!entry->clause) {
        nimcp_free(entry);
        return false;
    }

    entry->salience = salience;
    entry->timestamp = nimcp_time_get_ms();
    entry->context[0] = '\0';

    logic->kb[logic->num_facts++] = entry;
    logic->stats.facts_stored++;

    return true;
}

bool symbolic_logic_add_rule(symbolic_logic_t* logic, inference_rule_t* rule)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(rule, "rule")) {
        return false;
    }

    if (logic->num_rules >= logic->rules_capacity) {
        NIMCP_LOGGING_ERROR("Rules capacity full");
        return false;
    }

    logic->rules[logic->num_rules++] = rule;

    return true;
}

bool symbolic_logic_query(symbolic_logic_t* logic, logic_clause_t* query,
                         kb_entry_t*** results, int* num_results)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(query, "query") ||
        !nimcp_validate_pointer(results, "results") ||
        !nimcp_validate_pointer(num_results, "num_results")) {
        return false;
    }

    *num_results = 0;
    *results = NULL;

    // Count matching facts
    uint32_t match_count = 0;
    for (uint32_t i = 0; i < logic->num_facts; i++) {
        kb_entry_t* entry = logic->kb[i];

        // Simple matching: check if query literals match fact literals
        bool match = false;
        if (query->num_literals > 0 && entry->clause->num_literals > 0) {
            match = atoms_equal(query->literals[0], entry->clause->literals[0]);
        }

        if (match) {
            match_count++;
        }
    }

    if (match_count == 0) {
        return true;
    }

    // Allocate results
    *results = (kb_entry_t**)nimcp_calloc(match_count, sizeof(kb_entry_t*));
    if (!*results) {
        return false;
    }

    // Fill results
    uint32_t idx = 0;
    for (uint32_t i = 0; i < logic->num_facts && idx < match_count; i++) {
        kb_entry_t* entry = logic->kb[i];

        bool match = false;
        if (query->num_literals > 0 && entry->clause->num_literals > 0) {
            match = atoms_equal(query->literals[0], entry->clause->literals[0]);
        }

        if (match) {
            (*results)[idx++] = entry;
        }
    }

    *num_results = (int)match_count;
    return true;
}

//=============================================================================
// Unification Algorithm
//=============================================================================

unification_t* symbolic_logic_unify(logical_term_t* term1, logical_term_t* term2)
{
    if (!term1 || !term2) {
        return NULL;
    }

    unification_t* unif = (unification_t*)nimcp_calloc(1, sizeof(unification_t));
    if (!unif) {
        return NULL;
    }

    unif->success = false;
    unif->bindings = NULL;
    unif->num_bindings = 0;

    // Both constants or same variable - must be identical
    if (term1->type == TERM_CONSTANT && term2->type == TERM_CONSTANT) {
        unif->success = terms_equal(term1, term2);
        return unif;
    }

    // Variable unification
    if (term1->type == TERM_VARIABLE) {
        substitution_t* binding = (substitution_t*)nimcp_calloc(1, sizeof(substitution_t));
        if (!binding) {
            unification_destroy(unif);
            return NULL;
        }

        binding->variable = logic_term_copy(term1);
        binding->value = logic_term_copy(term2);

        unif->bindings = (substitution_t**)nimcp_calloc(1, sizeof(substitution_t*));
        if (!unif->bindings) {
            nimcp_free(binding);
            unification_destroy(unif);
            return NULL;
        }

        unif->bindings[0] = binding;
        unif->num_bindings = 1;
        unif->success = true;
        return unif;
    }

    if (term2->type == TERM_VARIABLE) {
        substitution_t* binding = (substitution_t*)nimcp_calloc(1, sizeof(substitution_t));
        if (!binding) {
            unification_destroy(unif);
            return NULL;
        }

        binding->variable = logic_term_copy(term2);
        binding->value = logic_term_copy(term1);

        unif->bindings = (substitution_t**)nimcp_calloc(1, sizeof(substitution_t*));
        if (!unif->bindings) {
            nimcp_free(binding);
            unification_destroy(unif);
            return NULL;
        }

        unif->bindings[0] = binding;
        unif->num_bindings = 1;
        unif->success = true;
        return unif;
    }

    // Function unification
    if (term1->type == TERM_FUNCTION && term2->type == TERM_FUNCTION) {
        if (strcmp(term1->name, term2->name) != 0 || term1->arity != term2->arity) {
            return unif;
        }

        // Unify all arguments
        unif->success = true;
        for (uint8_t i = 0; i < term1->arity; i++) {
            unification_t* arg_unif = symbolic_logic_unify(term1->args[i], term2->args[i]);
            if (!arg_unif || !arg_unif->success) {
                unif->success = false;
                if (arg_unif) unification_destroy(arg_unif);
                break;
            }
            unification_destroy(arg_unif);
        }

        if (unif->success) {
            }
    }

    return unif;
}

logical_term_t* symbolic_logic_substitute(logical_term_t* term, const substitution_t* subst)
{
    if (!term || !subst) {
        return NULL;
    }

    // If term is the variable being substituted, return the value
    if (terms_equal(term, subst->variable)) {
        return logic_term_copy(subst->value);
    }

    // Otherwise copy term with recursive substitution of args
    logical_term_t* result = logic_term_create(term->type, term->name);
    if (!result) {
        return NULL;
    }

    if (term->arity > 0 && term->args) {
        result->arity = term->arity;
        result->args = (logical_term_t**)nimcp_calloc(term->arity, sizeof(logical_term_t*));
        if (!result->args) {
            logic_term_destroy(result);
            return NULL;
        }

        for (uint8_t i = 0; i < term->arity; i++) {
            result->args[i] = symbolic_logic_substitute(term->args[i], subst);
            if (!result->args[i]) {
                logic_term_destroy(result);
                return NULL;
            }
        }
    }

    return result;
}

void unification_destroy(unification_t* unif)
{
    if (!unif) return;

    if (unif->bindings) {
        for (uint32_t i = 0; i < unif->num_bindings; i++) {
            if (unif->bindings[i]) {
                logic_term_destroy(unif->bindings[i]->variable);
                logic_term_destroy(unif->bindings[i]->value);
                nimcp_free(unif->bindings[i]);
            }
        }
        nimcp_free(unif->bindings);
    }

    nimcp_free(unif);
}

//=============================================================================
// Inference Engine - Forward Chaining
//=============================================================================

bool symbolic_logic_forward_chain(symbolic_logic_t* logic, uint32_t max_iterations,
                                  logic_clause_t*** new_facts, int* num_new_facts)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(new_facts, "new_facts") ||
        !nimcp_validate_pointer(num_new_facts, "num_new_facts")) {
        return false;
    }

    if (!logic->config.enable_forward_chaining) {
        NIMCP_LOGGING_ERROR("Forward chaining not enabled");
        return false;
    }

    *new_facts = NULL;
    *num_new_facts = 0;

    uint64_t start_time = nimcp_time_get_ms();
    uint32_t iteration = 0;
    uint32_t derived_count = 0;

    // Allocate array for new facts
    logic_clause_t** derived = (logic_clause_t**)nimcp_calloc(LOGIC_MAX_PREDICATES, sizeof(logic_clause_t*));
    if (!derived) {
        return false;
    }

    while (iteration < max_iterations) {
        bool new_fact_derived = false;

        // Try to apply each rule
        for (uint32_t r = 0; r < logic->num_rules; r++) {
            inference_rule_t* rule = logic->rules[r];

            // Check if all premises are satisfied
            bool premises_satisfied = true;
            for (uint32_t p = 0; p < rule->num_premises; p++) {
                bool found = false;

                for (uint32_t f = 0; f < logic->num_facts; f++) {
                    if (atoms_equal(rule->premises[p]->literals[0],
                                   logic->kb[f]->clause->literals[0])) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    premises_satisfied = false;
                    break;
                }
            }

            // If premises satisfied, derive conclusion
            if (premises_satisfied && rule->conclusion) {
                // Check if conclusion is new
                bool already_known = false;
                for (uint32_t f = 0; f < logic->num_facts; f++) {
                    if (atoms_equal(rule->conclusion->literals[0],
                                   logic->kb[f]->clause->literals[0])) {
                        already_known = true;
                        break;
                    }
                }

                if (!already_known) {
                    derived[derived_count] = logic_clause_copy(rule->conclusion);
                    if (derived[derived_count]) {
                        derived_count++;
                        new_fact_derived = true;

                        // Add to knowledge base
                        symbolic_logic_add_fact(logic, rule->conclusion, 0.7f);

                        logic->stats.rules_applied++;
                    }
                }
            }
        }

        if (!new_fact_derived) {
            break;  // Fixed point reached
        }

        iteration++;
    }

    uint64_t end_time = nimcp_time_get_ms();
    logic->stats.avg_inference_time = (float)(end_time - start_time);
    logic->stats.inferences_performed++;

    *new_facts = derived;
    *num_new_facts = (int)derived_count;

    return true;
}

//=============================================================================
// Brain Integration Helpers
//=============================================================================

float symbolic_logic_compute_novelty(symbolic_logic_t* logic, logic_clause_t* clause)
{
    if (!logic || !clause) {
        return 0.0f;
    }

    if (logic->num_facts == 0) {
        return 1.0f;  // Completely novel if no facts exist
    }

    // Check similarity to existing facts
    float max_similarity = 0.0f;

    for (uint32_t i = 0; i < logic->num_facts; i++) {
        kb_entry_t* entry = logic->kb[i];

        // Simple similarity: do they share any predicates?
        if (clause->num_literals > 0 && entry->clause->num_literals > 0) {
            if (strcmp(clause->literals[0]->name, entry->clause->literals[0]->name) == 0) {
                float similarity = 0.5f;  // Same predicate

                // Check arguments
                if (atoms_equal(clause->literals[0], entry->clause->literals[0])) {
                    similarity = 1.0f;  // Identical
                }

                if (similarity > max_similarity) {
                    max_similarity = similarity;
                }
            }
        }
    }

    return 1.0f - max_similarity;
}

bool symbolic_logic_get_salient_facts(symbolic_logic_t* logic, int top_k,
                                      kb_entry_t*** salient_facts, int* num_facts)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(salient_facts, "salient_facts") ||
        !nimcp_validate_pointer(num_facts, "num_facts")) {
        return false;
    }

    if (top_k <= 0) {
        return false;
    }

    uint32_t k = (uint32_t)top_k;
    if (k > logic->num_facts) {
        k = logic->num_facts;
    }

    // Allocate results array
    *salient_facts = (kb_entry_t**)nimcp_calloc(k, sizeof(kb_entry_t*));
    if (!*salient_facts) {
        return false;
    }

    // Sort by salience (simple selection sort for top-k)
    for (uint32_t i = 0; i < k; i++) {
        uint32_t max_idx = i;
        float max_salience = (i < logic->num_facts) ? logic->kb[i]->salience : 0.0f;

        for (uint32_t j = i + 1; j < logic->num_facts; j++) {
            if (logic->kb[j]->salience > max_salience) {
                max_salience = logic->kb[j]->salience;
                max_idx = j;
            }
        }

        (*salient_facts)[i] = logic->kb[max_idx];

        // Swap
        if (max_idx != i && i < logic->num_facts) {
            kb_entry_t* temp = logic->kb[i];
            logic->kb[i] = logic->kb[max_idx];
            logic->kb[max_idx] = temp;
        }
    }

    *num_facts = (int)k;
    return true;
}

bool symbolic_logic_consolidate_memory(symbolic_logic_t* logic, logic_clause_t* clause,
                                       float salience, const char* context)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(clause, "clause")) {
        return false;
    }

    if (!logic->config.enable_memory_consolidation) {
        return false;
    }

    // Add fact with context
    if (!symbolic_logic_add_fact(logic, clause, salience)) {
        return false;
    }

    // Set context if provided
    if (context && logic->num_facts > 0) {
        kb_entry_t* entry = logic->kb[logic->num_facts - 1];
        strncpy(entry->context, context, sizeof(entry->context) - 1);
        entry->context[sizeof(entry->context) - 1] = '\0';
    }

    return true;
}

bool symbolic_logic_explore(symbolic_logic_t* logic, uint32_t exploration_depth,
                           logic_clause_t*** interesting_facts, int* num_facts)
{
    if (!nimcp_validate_pointer(logic, "logic") ||
        !nimcp_validate_pointer(interesting_facts, "interesting_facts") ||
        !nimcp_validate_pointer(num_facts, "num_facts")) {
        return false;
    }

    // Use forward chaining to explore
    return symbolic_logic_forward_chain(logic, exploration_depth,
                                       interesting_facts, num_facts);
}

//=============================================================================
// Utility Functions
//=============================================================================

bool symbolic_logic_to_string(const logical_formula_t* formula, char* buffer, size_t buffer_size)
{
    if (!formula || !buffer || buffer_size == 0) {
        return false;
    }

    if (formula->atom) {
        // Atomic formula
        int written = snprintf(buffer, buffer_size, "%s%s",
                              formula->atom->negated ? "~" : "",
                              formula->atom->name);

        if (formula->atom->arity > 0) {
            written += snprintf(buffer + written, buffer_size - written, "(");
            for (uint8_t i = 0; i < formula->atom->arity; i++) {
                written += snprintf(buffer + written, buffer_size - written, "%s%s",
                                  formula->atom->terms[i]->name,
                                  (i < formula->atom->arity - 1) ? "," : "");
            }
            written += snprintf(buffer + written, buffer_size - written, ")");
        }

        return true;
    }

    // Composite formula
    const char* op_str = "?";
    switch (formula->op) {
        case OP_AND: op_str = "&"; break;
        case OP_OR: op_str = "|"; break;
        case OP_NOT: op_str = "~"; break;
        case OP_IMPLIES: op_str = "->"; break;
        case OP_IFF: op_str = "<->"; break;
        default: break;
    }

    snprintf(buffer, buffer_size, "(%s %s)", op_str, "...");
    return true;
}
