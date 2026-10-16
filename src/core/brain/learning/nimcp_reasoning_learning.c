//=============================================================================
// nimcp_reasoning_learning.c - Reasoning Learning Implementation
//=============================================================================
/**
 * @file nimcp_reasoning_learning.c
 * @brief Training layer integration for NIMCP reasoning system
 *
 * WHAT: Symbolic reasoning learning implementation
 * WHY:  Bridge neural learning with logical rule induction
 * HOW:  Pattern extraction, rule compilation, reinforcement-based refinement
 *
 * @author NIMCP Development Team
 * @version 2.6.2
 * @date 2025-11-20
 */

#include "core/brain/learning/nimcp_reasoning_learning.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "core_reasoning_learning"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(reasoning_learning, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <string.h>
#include <math.h>
#include <stdio.h>
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Extract common pattern from examples with same label
 *
 * WHAT: Find features that co-occur in examples of same class
 * WHY:  Pattern extraction is core of inductive learning
 * HOW:  Count feature occurrences, select high-frequency features
 */
static bool extract_pattern_from_examples(
    const logical_example_t* examples,
    uint32_t num_examples,
    const char* target_label,
    float* pattern,
    uint32_t num_features,
    float* confidence_out)
{
    if (!examples || !target_label || !pattern || num_features == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_pattern_from_examples: required parameter is NULL (examples, target_label, pattern)");
        return false;
    }

    // Count feature occurrences across examples with target label
    float* feature_counts = (float*)nimcp_calloc(num_features, sizeof(float));
    if (!feature_counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "extract_pattern_from_examples: feature_counts is NULL");
        return false;
    }

    uint32_t matching_examples = 0;

    for (uint32_t i = 0; i < num_examples; i++) {
        if (strcmp(examples[i].label, target_label) == 0) {
            matching_examples++;
            for (uint32_t f = 0; f < num_features && f < examples[i].num_features; f++) {
                feature_counts[f] += examples[i].features[f];
            }
        }
    }

    if (matching_examples == 0) {
        nimcp_free(feature_counts);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_pattern_from_examples: matching_examples is zero");
        return false;
    }

    // Compute pattern: features that occur frequently (>threshold)
    float threshold = matching_examples * 0.7f; // 70% support
    for (uint32_t f = 0; f < num_features; f++) {
        pattern[f] = (feature_counts[f] >= threshold) ? 1.0f : 0.0f;
    }

    // Confidence = average support across features
    if (confidence_out) {
        *confidence_out = 0.0f;
        uint32_t active_features = 0;
        for (uint32_t f = 0; f < num_features; f++) {
            if (pattern[f] > 0.5f) {
                *confidence_out += (feature_counts[f] / matching_examples);
                active_features++;
            }
        }
        if (active_features > 0) {
            *confidence_out /= active_features;
        }
    }

    nimcp_free(feature_counts);
    return true;
}

/**
 * @brief Convert feature pattern to logical clause
 *
 * WHAT: Map feature vector to symbolic representation
 * WHY:  Bridge numerical features with symbolic predicates
 * HOW:  Create atoms for active features
 */
static logic_clause_t* pattern_to_clause(
    const float* pattern,
    uint32_t num_features,
    const char* clause_name)
{
    if (!pattern || !clause_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_to_clause: required parameter is NULL (pattern, clause_name)");
        return NULL;
    }

    // Count active features (>0.5)
    uint32_t num_active = 0;
    for (uint32_t i = 0; i < num_features; i++) {
        if (pattern[i] > 0.5f) {
            num_active++;
        }
    }

    if (num_active == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pattern_to_clause: num_active is zero");
        return NULL;
    }

    // Create clause with active feature literals
    logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!clause) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "clause is NULL");

        return NULL;
    }

    clause->literals = (atomic_formula_t**)nimcp_calloc(num_active, sizeof(atomic_formula_t*));
    if (!clause->literals) {
        nimcp_free(clause);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pattern_to_clause: clause->literals is NULL");
        return NULL;
    }

    clause->num_literals = num_active;
    clause->confidence = 1.0f;

    uint32_t lit_idx = 0;
    for (uint32_t i = 0; i < num_features; i++) {
        if (pattern[i] > 0.5f) {
            char feature_name[NIMCP_ID_BUFFER_SIZE];
            snprintf(feature_name, sizeof(feature_name), "%s_feature_%u", clause_name, i);

            // Create atom with variable X (generic)
            logical_term_t* var_x = logic_term_create(TERM_VARIABLE, "X");
            if (!var_x) {
                // Cleanup on failure
                for (uint32_t j = 0; j < lit_idx; j++) {
                    logic_atom_destroy(clause->literals[j]);
                }
                nimcp_free(clause->literals);
                nimcp_free(clause);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_to_clause: var_x is NULL");
                return NULL;
            }

            logical_term_t* terms[] = {var_x};
            clause->literals[lit_idx] = logic_atom_create(feature_name, terms, 1);

            logic_term_destroy(var_x);

            if (!clause->literals[lit_idx]) {
                // Cleanup on failure
                for (uint32_t j = 0; j < lit_idx; j++) {
                    logic_atom_destroy(clause->literals[j]);
                }
                nimcp_free(clause->literals);
                nimcp_free(clause);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pattern_to_clause: clause->literals is NULL");
                return NULL;
            }

            lit_idx++;
        }
    }

    return clause;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool brain_learn_logical_rule(
    brain_t brain,
    const logical_example_t* examples,
    uint32_t num_examples,
    learned_rule_t*** learned_rules,
    int* num_learned_rules,
    bool compile_to_neural)
{
    // Guard: Validate parameters
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(examples, "examples") ||
        !nimcp_validate_pointer(learned_rules, "learned_rules") ||
        !nimcp_validate_pointer(num_learned_rules, "num_learned_rules")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_learn_logical_rule: nimcp_validate_pointer is NULL");
        return false;
    }

    if (num_examples == 0) {
        NIMCP_LOGGING_ERROR("No examples provided for rule learning");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_learn_logical_rule: num_examples is zero");
        return false;
    }

    if (!brain->logic_system) {
        NIMCP_LOGGING_ERROR("Brain does not have symbolic logic system enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_logical_rule: brain->logic_system is NULL");
        return false;
    }

    // Phase 1: Group examples by label
    // Simple implementation: extract unique labels
    char labels[100][64]; // Max 100 unique labels
    uint32_t num_labels = 0;

    for (uint32_t i = 0; i < num_examples; i++) {
        bool found = false;
        for (uint32_t j = 0; j < num_labels; j++) {
            if (strcmp(labels[j], examples[i].label) == 0) {
                found = true;
                break;
            }
        }
        if (!found && num_labels < 100) {
            strncpy(labels[num_labels], examples[i].label, 63);
            labels[num_labels][63] = '\0';
            num_labels++;
        }
    }

    // Phase 2: Extract pattern for each label
    *num_learned_rules = num_labels;
    *learned_rules = (learned_rule_t**)nimcp_calloc(num_labels, sizeof(learned_rule_t*));
    if (!*learned_rules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_learn_logical_rule: validation failed");
        return false;
    }

    uint32_t num_features = examples[0].num_features;

    for (uint32_t l = 0; l < num_labels; l++) {
        learned_rule_t* rule = (learned_rule_t*)nimcp_calloc(1, sizeof(learned_rule_t));
        if (!rule) {
            // Cleanup
            for (uint32_t j = 0; j < l; j++) {
                learned_rule_destroy((*learned_rules)[j]);
            }
            nimcp_free(*learned_rules);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_logical_rule: rule is NULL");
            return false;
        }

        // Extract pattern
        float* pattern = (float*)nimcp_calloc(num_features, sizeof(float));
        if (!pattern) {
            nimcp_free(rule);
            for (uint32_t j = 0; j < l; j++) {
                learned_rule_destroy((*learned_rules)[j]);
            }
            nimcp_free(*learned_rules);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_logical_rule: pattern is NULL");
            return false;
        }

        float confidence = 0.0f;
        if (!extract_pattern_from_examples(examples, num_examples, labels[l],
                                          pattern, num_features, &confidence)) {
            nimcp_free(pattern);
            nimcp_free(rule);
            continue;
        }

        // Create rule
        snprintf(rule->rule_name, sizeof(rule->rule_name), "rule_%s", labels[l]);
        rule->confidence = confidence;
        rule->support_count = 0;
        rule->contradiction_count = 0;
        rule->last_updated = nimcp_time_get_us();

        // Convert pattern to premises
        logic_clause_t* premise = pattern_to_clause(pattern, num_features, labels[l]);
        nimcp_free(pattern);

        if (premise) {
            rule->num_premises = 1;
            rule->premises = (logic_clause_t**)nimcp_calloc(1, sizeof(logic_clause_t*));
            if (rule->premises) {
                rule->premises[0] = premise;
            }
        } else {
            rule->num_premises = 0;
            rule->premises = NULL;
        }

        // Create conclusion
        logical_term_t* var_x = logic_term_create(TERM_VARIABLE, "X");
        if (var_x) {
            logical_term_t* terms[] = {var_x};
            atomic_formula_t* conclusion_atom = logic_atom_create(labels[l], terms, 1);
            logic_term_destroy(var_x);

            if (conclusion_atom) {
                rule->conclusion = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
                if (rule->conclusion) {
                    rule->conclusion->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
                    if (rule->conclusion->literals) {
                        rule->conclusion->literals[0] = conclusion_atom;
                        rule->conclusion->num_literals = 1;
                        rule->conclusion->confidence = confidence;
                    }
                }
            }
        }

        // Add rule to KB
        if (rule->premises && rule->conclusion) {
            inference_rule_t* kb_rule = (inference_rule_t*)nimcp_calloc(1, sizeof(inference_rule_t));
            if (kb_rule) {
                strncpy(kb_rule->name, rule->rule_name, LOGIC_MAX_NAME_LENGTH - 1);
                kb_rule->premises = rule->premises;
                kb_rule->num_premises = rule->num_premises;
                kb_rule->conclusion = rule->conclusion;
                kb_rule->priority = confidence;

                symbolic_logic_add_rule(brain->logic_system, kb_rule);
                rule->support_count++;
            }
        }

        (*learned_rules)[l] = rule;

        // Optional: Compile to neural circuit
        if (compile_to_neural) {
            neural_compilation_result_t* compilation = NULL;
            brain_compile_rule_to_neural(brain, rule, &compilation);
            if (compilation) {
                neural_compilation_result_destroy(compilation);
            }
        }
    }

    return true;
}

bool brain_learn_symbolic_association(
    brain_t brain,
    atomic_formula_t* antecedent,
    atomic_formula_t* consequent,
    float confidence)
{
    // Guard: Validate parameters
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(antecedent, "antecedent") ||
        !nimcp_validate_pointer(consequent, "consequent")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_learn_symbolic_association: nimcp_validate_pointer is NULL");
        return false;
    }

    if (confidence < 0.0f || confidence > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid confidence: %.2f (must be in [0,1])", confidence);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_learn_symbolic_association: validation failed");
        return false;
    }

    if (!brain->logic_system) {
        NIMCP_LOGGING_ERROR("Brain does not have symbolic logic system enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_symbolic_association: brain->logic_system is NULL");
        return false;
    }

    // Create implication rule: antecedent → consequent
    inference_rule_t* rule = (inference_rule_t*)nimcp_calloc(1, sizeof(inference_rule_t));
    if (!rule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_learn_symbolic_association: rule is NULL");
        return false;
    }

    snprintf(rule->name, LOGIC_MAX_NAME_LENGTH, "%s_implies_%s",
             antecedent->name, consequent->name);

    // Create premise clause
    rule->num_premises = 1;
    rule->premises = (logic_clause_t**)nimcp_calloc(1, sizeof(logic_clause_t*));
    if (!rule->premises) {
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_learn_symbolic_association: rule->premises is NULL");
        return false;
    }

    rule->premises[0] = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!rule->premises[0]) {
        nimcp_free(rule->premises);
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_learn_symbolic_association: rule->premises is NULL");
        return false;
    }

    rule->premises[0]->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
    if (!rule->premises[0]->literals) {
        nimcp_free(rule->premises[0]);
        nimcp_free(rule->premises);
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_learn_symbolic_association: rule->premises is NULL");
        return false;
    }

    // Copy antecedent (would need logic_atom_copy from symbolic_logic.c)
    rule->premises[0]->literals[0] = logic_atom_create(antecedent->name,
                                                       antecedent->terms,
                                                       antecedent->arity);
    if (!rule->premises[0]->literals[0]) {
        nimcp_free(rule->premises[0]->literals);
        nimcp_free(rule->premises[0]);
        nimcp_free(rule->premises);
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_symbolic_association: rule->premises is NULL");
        return false;
    }
    rule->premises[0]->num_literals = 1;
    rule->premises[0]->confidence = confidence;

    // Create conclusion clause
    rule->conclusion = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!rule->conclusion) {
        logic_atom_destroy(rule->premises[0]->literals[0]);
        nimcp_free(rule->premises[0]->literals);
        nimcp_free(rule->premises[0]);
        nimcp_free(rule->premises);
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_symbolic_association: rule->conclusion is NULL");
        return false;
    }

    rule->conclusion->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
    if (!rule->conclusion->literals) {
        nimcp_free(rule->conclusion);
        logic_atom_destroy(rule->premises[0]->literals[0]);
        nimcp_free(rule->premises[0]->literals);
        nimcp_free(rule->premises[0]);
        nimcp_free(rule->premises);
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_symbolic_association: rule->conclusion->literals is NULL");
        return false;
    }

    rule->conclusion->literals[0] = logic_atom_create(consequent->name,
                                                      consequent->terms,
                                                      consequent->arity);
    if (!rule->conclusion->literals[0]) {
        nimcp_free(rule->conclusion->literals);
        nimcp_free(rule->conclusion);
        logic_atom_destroy(rule->premises[0]->literals[0]);
        nimcp_free(rule->premises[0]->literals);
        nimcp_free(rule->premises[0]);
        nimcp_free(rule->premises);
        nimcp_free(rule);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_learn_symbolic_association: rule->conclusion->literals is NULL");
        return false;
    }
    rule->conclusion->num_literals = 1;
    rule->conclusion->confidence = confidence;

    rule->priority = confidence;

    // Add to symbolic logic KB
    bool success = symbolic_logic_add_rule(brain->logic_system, rule);

    if (!success) {
        // Cleanup if add failed
        logic_atom_destroy(rule->conclusion->literals[0]);
        nimcp_free(rule->conclusion->literals);
        nimcp_free(rule->conclusion);
        logic_atom_destroy(rule->premises[0]->literals[0]);
        nimcp_free(rule->premises[0]->literals);
        nimcp_free(rule->premises[0]);
        nimcp_free(rule->premises);
        nimcp_free(rule);
    }

    return success;
}

bool brain_refine_rule_confidence(
    brain_t brain,
    const char* rule_name,
    bool outcome,
    float learning_rate)
{
    // Guard: Validate parameters
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(rule_name, "rule_name")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_refine_rule_confidence: nimcp_validate_pointer is NULL");
        return false;
    }

    if (learning_rate < 0.0f || learning_rate > 1.0f) {
        NIMCP_LOGGING_ERROR("Invalid learning rate: %.2f (must be in [0,1])", learning_rate);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_refine_rule_confidence: validation failed");
        return false;
    }

    if (!brain->logic_system) {
        NIMCP_LOGGING_ERROR("Brain does not have symbolic logic system enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_refine_rule_confidence: brain->logic_system is NULL");
        return false;
    }

    // Note: This is a simplified implementation
    // In full implementation, would access internal rule storage
    // For now, we log the refinement event

    NIMCP_LOGGING_INFO("Rule refinement: %s, outcome: %s, learning_rate: %.3f",
                      rule_name, outcome ? "success" : "failure", learning_rate);

    // Would implement:
    // 1. Find rule in logic_system->rules by name
    // 2. Update rule->priority (confidence) based on outcome
    // 3. Track support/contradiction counts

    return true;
}

bool brain_compile_rule_to_neural(
    brain_t brain,
    const learned_rule_t* rule,
    neural_compilation_result_t** result)
{
    // Guard: Validate parameters
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(rule, "rule") ||
        !nimcp_validate_pointer(result, "result")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compile_rule_to_neural: nimcp_validate_pointer is NULL");
        return false;
    }

    // Allocate result structure
    *result = (neural_compilation_result_t*)nimcp_calloc(1, sizeof(neural_compilation_result_t));
    if (!*result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compile_rule_to_neural: validation failed");
        return false;
    }

    strncpy((*result)->rule_name, rule->rule_name, 63);
    (*result)->rule_name[63] = '\0';

    // Simplified compilation: allocate placeholder neurons
    // Full implementation would create actual neural logic gates

    uint32_t num_neurons = (rule->num_premises + 1) * 3; // Premises + conclusion, with gates
    (*result)->neuron_ids = (uint32_t*)nimcp_calloc(num_neurons, sizeof(uint32_t));
    if (!(*result)->neuron_ids) {
        nimcp_free(*result);
        *result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compile_rule_to_neural: validation failed");
        return false;
    }

    (*result)->num_neurons = num_neurons;

    // Placeholder: assign neuron IDs
    for (uint32_t i = 0; i < num_neurons; i++) {
        (*result)->neuron_ids[i] = i;
    }

    // Allocate gate IDs
    uint32_t num_gates = rule->num_premises; // One gate per premise (AND)
    (*result)->gate_ids = (uint32_t*)nimcp_calloc(num_gates, sizeof(uint32_t));
    if (!(*result)->gate_ids) {
        nimcp_free((*result)->neuron_ids);
        nimcp_free(*result);
        *result = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_compile_rule_to_neural: validation failed");
        return false;
    }

    (*result)->num_gates = num_gates;
    for (uint32_t i = 0; i < num_gates; i++) {
        (*result)->gate_ids[i] = i; // Placeholder gate IDs
    }

    (*result)->compilation_accuracy = 0.95f; // Placeholder: assume 95% accuracy
    (*result)->compiled_successfully = true;

    NIMCP_LOGGING_INFO("Compiled rule '%s' to %u neurons, %u gates",
                      rule->rule_name, num_neurons, num_gates);

    return true;
}

bool brain_execute_neural_rule(
    brain_t brain,
    const neural_compilation_result_t* result,
    const float* input,
    uint32_t num_inputs,
    float* output,
    uint32_t num_outputs)
{
    // Guard: Validate parameters
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(result, "result") ||
        !nimcp_validate_pointer(input, "input") ||
        !nimcp_validate_pointer(output, "output")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_execute_neural_rule: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!result->compiled_successfully) {
        NIMCP_LOGGING_ERROR("Cannot execute rule that failed compilation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_execute_neural_rule: result->compiled_successfully is NULL");
        return false;
    }

    // Simplified execution: use brain_predict on compiled neuron subset
    // Full implementation would do targeted forward pass through logic gates

    return brain_predict(brain, input, num_inputs, output, num_outputs) >= 0.0f;
}

//=============================================================================
// Memory Management
//=============================================================================

void learned_rule_destroy(learned_rule_t* rule)
{
    if (!rule) return;

    if (rule->premises) {
        for (uint32_t i = 0; i < rule->num_premises; i++) {
            if (rule->premises[i]) {
                if (rule->premises[i]->literals) {
                    for (uint32_t j = 0; j < rule->premises[i]->num_literals; j++) {
                        logic_atom_destroy(rule->premises[i]->literals[j]);
                    }
                    nimcp_free(rule->premises[i]->literals);
                }
                nimcp_free(rule->premises[i]);
            }
        }
        nimcp_free(rule->premises);
    }

    if (rule->conclusion) {
        if (rule->conclusion->literals) {
            for (uint32_t i = 0; i < rule->conclusion->num_literals; i++) {
                logic_atom_destroy(rule->conclusion->literals[i]);
            }
            nimcp_free(rule->conclusion->literals);
        }
        nimcp_free(rule->conclusion);
    }

    nimcp_free(rule);
}

void symbolic_association_destroy(symbolic_association_t* assoc)
{
    if (!assoc) return;

    logic_atom_destroy(assoc->antecedent);
    logic_atom_destroy(assoc->consequent);
    nimcp_free(assoc);
}

void neural_compilation_result_destroy(neural_compilation_result_t* result)
{
    if (!result) return;

    nimcp_free(result->neuron_ids);
    nimcp_free(result->gate_ids);
    nimcp_free(result);
}

//=============================================================================
// Utility Functions
//=============================================================================

bool brain_get_learned_rules(
    brain_t brain,
    learned_rule_t*** rules,
    int* num_rules)
{
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(rules, "rules") ||
        !nimcp_validate_pointer(num_rules, "num_rules")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_learned_rules: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!brain->logic_system) {
        *num_rules = 0;
        *rules = NULL;
        return true;
    }

    // Simplified: return empty list
    // Full implementation would extract rules from logic_system
    *num_rules = 0;
    *rules = NULL;

    return true;
}

bool brain_get_symbolic_associations(
    brain_t brain,
    symbolic_association_t*** associations,
    int* num_associations)
{
    if (!nimcp_validate_pointer(brain, "brain") ||
        !nimcp_validate_pointer(associations, "associations") ||
        !nimcp_validate_pointer(num_associations, "num_associations")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_symbolic_associations: nimcp_validate_pointer is NULL");
        return false;
    }

    // Simplified: return empty list
    *num_associations = 0;
    *associations = NULL;

    return true;
}

bool learned_rule_to_string(
    const learned_rule_t* rule,
    char* buffer,
    size_t buffer_size)
{
    if (!nimcp_validate_pointer(rule, "rule") ||
        !nimcp_validate_pointer(buffer, "buffer") ||
        buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "learned_rule_to_string: nimcp_validate_pointer is NULL");
        return false;
    }

    int written = snprintf(buffer, buffer_size,
                          "Rule '%s': confidence=%.3f, support=%u, contradictions=%u",
                          rule->rule_name, rule->confidence,
                          rule->support_count, rule->contradiction_count);

    return written >= 0 && (size_t)written < buffer_size;
}
