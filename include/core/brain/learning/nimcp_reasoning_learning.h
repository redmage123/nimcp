//=============================================================================
// nimcp_reasoning_learning.h - Reasoning Learning Integration Layer
//=============================================================================
/**
 * @file nimcp_reasoning_learning.h
 * @brief Training layer integration for NIMCP reasoning system
 *
 * WHAT: Symbolic reasoning learning APIs that bridge neural learning with logic
 * WHY:  Enable brain networks to learn logical rules from examples (inductive learning)
 * HOW:  Extract patterns from data, compile symbolic rules to neural circuits
 *
 * ARCHITECTURE:
 * - Inductive learning: Extract logical rules from labeled examples
 * - Symbolic association: Learn A→B implications from co-occurrence
 * - Rule refinement: Adjust rule confidence based on success/failure feedback
 * - Neural-symbolic integration: Compile learned rules to neural logic gates
 *
 * BIOLOGICAL INSPIRATION:
 * - Prefrontal Cortex: Abstract rule learning and generalization
 * - Hippocampus: Pattern extraction and episodic-to-semantic transfer
 * - Basal Ganglia: Reward-based rule reinforcement
 * - Cortical Columns: Distributed symbolic-neural representations
 *
 * PERFORMANCE:
 * - Rule extraction: O(n*m) where n=examples, m=features
 * - Neural compilation: O(r*c) where r=rules, c=circuit_complexity
 * - Rule refinement: O(1) per feedback event
 *
 * @author NIMCP Development Team
 * @version 2.6.2
 * @date 2025-11-20
 */

#ifndef NIMCP_REASONING_LEARNING_H
#define NIMCP_REASONING_LEARNING_H

#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Reasoning Learning Structures
//=============================================================================

/**
 * @brief Logical rule learning example
 *
 * WHAT: Training example for rule induction
 * WHY:  Structured format for supervised rule learning
 * HOW:  Features + label + context enable pattern extraction
 */
typedef struct {
    float* features;              ///< Input feature vector
    uint32_t num_features;        ///< Feature count
    char label[64];               ///< Output label/class
    float confidence;             ///< Label confidence [0,1]
    char context[64];             ///< Context/category for rule
    atomic_formula_t* ground_truth; ///< Optional: known logical fact
} logical_example_t;

/**
 * @brief Learned logical rule
 *
 * WHAT: Induced rule from training examples
 * WHY:  Captures generalizable patterns as symbolic rules
 * HOW:  IF conditions THEN conclusion with confidence
 */
typedef struct {
    char rule_name[64];           ///< Human-readable rule name
    logic_clause_t** premises;    ///< Rule conditions (antecedents)
    uint32_t num_premises;        ///< Number of conditions
    logic_clause_t* conclusion;   ///< Rule result (consequent)
    float confidence;             ///< Rule strength [0,1]
    uint32_t support_count;       ///< # examples supporting this rule
    uint32_t contradiction_count; ///< # examples contradicting this rule
    uint64_t last_updated;        ///< Timestamp of last refinement
} learned_rule_t;

/**
 * @brief Symbolic association
 *
 * WHAT: Learned A→B implication from co-occurrence
 * WHY:  Capture correlations for associative reasoning
 * HOW:  Track joint occurrence statistics
 */
typedef struct {
    atomic_formula_t* antecedent; ///< A (trigger)
    atomic_formula_t* consequent; ///< B (result)
    float confidence;             ///< P(B|A) estimate
    uint32_t co_occurrence_count; ///< # times A and B seen together
    uint32_t antecedent_only_count; ///< # times A seen without B
} symbolic_association_t;

/**
 * @brief Neural-symbolic compilation result
 *
 * WHAT: Result of compiling symbolic rule to neural circuit
 * WHY:  Bridge symbolic logic with neural execution
 * HOW:  Map rule structure to neural logic gates
 */
typedef struct {
    char rule_name[64];           ///< Source rule identifier
    uint32_t* neuron_ids;         ///< Neurons implementing this rule
    uint32_t num_neurons;         ///< Circuit size
    uint32_t* gate_ids;           ///< Logic gate identifiers (AND, OR, NOT)
    uint32_t num_gates;           ///< Number of gates
    float compilation_accuracy;   ///< How well neural approximates symbolic
    bool compiled_successfully;   ///< Compilation status
} neural_compilation_result_t;

//=============================================================================
// Inductive Rule Learning
//=============================================================================

/**
 * @brief Learn logical rule from labeled examples (inductive learning)
 *
 * WHAT: Extract symbolic rules from training data
 * WHY:  Enable brain to generalize from examples to rules
 * HOW:  Find common patterns across examples with same label
 *
 * ALGORITHM:
 * 1. Group examples by label
 * 2. For each group, find common features (pattern extraction)
 * 3. Generate rule: IF <common_pattern> THEN <label>
 * 4. Compute confidence based on support vs contradictions
 * 5. Add rule to symbolic logic KB
 * 6. Optionally compile rule to neural circuit
 *
 * EXAMPLE:
 *   Input: [{features: [has_wings=1, flies=1, ...], label: "bird"}, ...]
 *   Output: IF has_wings(X) AND flies(X) THEN bird(X) [confidence: 0.95]
 *
 * COMPLEXITY: O(n*m*k) where n=examples, m=features, k=patterns
 * PERFORMANCE: ~10-50ms for 100 examples, ~100-500ms for 1000 examples
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal pattern completion and generalization
 * - Prefrontal cortex abstract rule extraction
 * - Sleep-dependent consolidation of episodic → semantic
 *
 * @param brain Brain handle with symbolic logic system
 * @param examples Array of training examples
 * @param num_examples Number of examples
 * @param learned_rules Output: extracted rules (caller must free)
 * @param num_learned_rules Output: number of rules extracted
 * @param compile_to_neural If true, compile rules to neural circuits
 * @return true on success, false on failure
 *
 * @example
 * logical_example_t examples[] = {
 *     {.features = bird1, .num_features = 10, .label = "bird", .confidence = 1.0},
 *     {.features = bird2, .num_features = 10, .label = "bird", .confidence = 1.0},
 *     {.features = fish1, .num_features = 10, .label = "fish", .confidence = 1.0}
 * };
 * learned_rule_t** rules;
 * int num_rules;
 * brain_learn_logical_rule(brain, examples, 3, &rules, &num_rules, true);
 */
bool brain_learn_logical_rule(
    brain_t brain,
    const logical_example_t* examples,
    uint32_t num_examples,
    learned_rule_t*** learned_rules,
    int* num_learned_rules,
    bool compile_to_neural
);

//=============================================================================
// Symbolic Association Learning
//=============================================================================

/**
 * @brief Learn symbolic association A→B from co-occurrence
 *
 * WHAT: Discover A implies B based on correlation
 * WHY:  Enable associative reasoning (if A then likely B)
 * HOW:  Track co-occurrence statistics, estimate P(B|A)
 *
 * ALGORITHM:
 * 1. Increment co_occurrence_count if A and B both true
 * 2. Increment antecedent_only_count if A true but not B
 * 3. Compute confidence = co_occur / (co_occur + antecedent_only)
 * 4. If confidence > threshold, add A→B rule to KB
 *
 * EXAMPLE:
 *   Observe: "dark clouds" and "rain" co-occur frequently
 *   Learn: IF dark_clouds(sky) THEN rain(sky) [confidence: 0.8]
 *
 * COMPLEXITY: O(1) per association update
 * PERFORMANCE: <0.1ms per update
 *
 * BIOLOGICAL BASIS:
 * - Hebbian learning: "neurons that fire together wire together"
 * - Associative memory (hippocampus CA3)
 * - Predictive coding (temporal cortex)
 *
 * @param brain Brain handle with symbolic logic system
 * @param antecedent Formula A (trigger)
 * @param consequent Formula B (predicted result)
 * @param confidence Initial confidence [0,1]
 * @return true on success, false on failure
 *
 * @example
 * atomic_formula_t* dark_clouds = logic_atom_create("dark_clouds", NULL, 0);
 * atomic_formula_t* rain = logic_atom_create("rain", NULL, 0);
 * brain_learn_symbolic_association(brain, dark_clouds, rain, 0.8);
 */
bool brain_learn_symbolic_association(
    brain_t brain,
    atomic_formula_t* antecedent,
    atomic_formula_t* consequent,
    float confidence
);

//=============================================================================
// Rule Refinement
//=============================================================================

/**
 * @brief Refine rule confidence based on outcome (reinforcement learning)
 *
 * WHAT: Adjust rule strength based on success/failure feedback
 * WHY:  Enable online rule refinement via reinforcement
 * HOW:  Increase confidence on success, decrease on failure
 *
 * ALGORITHM:
 * 1. Find rule in KB by name
 * 2. If outcome = success: confidence *= (1 + learning_rate)
 * 3. If outcome = failure: confidence *= (1 - learning_rate)
 * 4. Clamp confidence to [0, 1]
 * 5. Update support/contradiction counts
 *
 * COMPLEXITY: O(r) where r = number of rules (linear search)
 * PERFORMANCE: <1ms per refinement
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic reinforcement (basal ganglia)
 * - Reward prediction error (TD-learning)
 * - Synaptic scaling based on activity outcomes
 *
 * @param brain Brain handle with symbolic logic system
 * @param rule_name Name of rule to refine
 * @param outcome true = success (strengthen), false = failure (weaken)
 * @param learning_rate Adjustment magnitude [0, 1] (default: 0.1)
 * @return true on success, false if rule not found
 *
 * @example
 * // Rule predicts rain, and it actually rains
 * brain_refine_rule_confidence(brain, "rain_prediction", true, 0.1);
 *
 * // Rule predicts rain, but it doesn't rain
 * brain_refine_rule_confidence(brain, "rain_prediction", false, 0.1);
 */
bool brain_refine_rule_confidence(
    brain_t brain,
    const char* rule_name,
    bool outcome,
    float learning_rate
);

//=============================================================================
// Neural-Symbolic Compilation
//=============================================================================

/**
 * @brief Compile symbolic rule to neural logic circuit
 *
 * WHAT: Convert symbolic rule to executable neural network
 * WHY:  Enable fast neural execution of logical reasoning
 * HOW:  Map logical operators to neural gates (AND, OR, NOT)
 *
 * ALGORITHM:
 * 1. Parse rule structure (premises → conclusion)
 * 2. For each logical operator:
 *    - AND: Create neurons that fire only if all inputs active
 *    - OR: Create neurons that fire if any input active
 *    - NOT: Create inhibitory connections
 * 3. Wire premises to operators to conclusion
 * 4. Train neural circuit to match symbolic truth table
 *
 * EXAMPLE:
 *   Rule: IF A AND (B OR C) THEN D
 *   Neural circuit:
 *     neuron_or = OR_gate(B, C)
 *     neuron_and = AND_gate(A, neuron_or)
 *     neuron_d = neuron_and
 *
 * COMPLEXITY: O(o*n) where o=operators, n=neurons_per_gate
 * PERFORMANCE: ~1-10ms per rule compilation
 *
 * BIOLOGICAL BASIS:
 * - Cortical microcircuits implement logical operations
 * - Feedforward inhibition (NOT gates)
 * - Coincidence detection (AND gates)
 * - Population coding for OR gates
 *
 * @param brain Brain handle with neural network
 * @param rule Symbolic rule to compile
 * @param result Output: compilation result (caller must free)
 * @return true on success, false on failure
 *
 * @example
 * neural_compilation_result_t* result;
 * if (brain_compile_rule_to_neural(brain, my_rule, &result)) {
 *     printf("Compiled rule to %d neurons, accuracy: %.2f\n",
 *            result->num_neurons, result->compilation_accuracy);
 * }
 */
bool brain_compile_rule_to_neural(
    brain_t brain,
    const learned_rule_t* rule,
    neural_compilation_result_t** result
);

/**
 * @brief Execute neural-compiled rule on input
 *
 * WHAT: Run neural circuit implementing symbolic rule
 * WHY:  Fast inference using neural implementation
 * HOW:  Forward pass through compiled neural circuit
 *
 * @param brain Brain handle
 * @param result Compiled neural circuit
 * @param input Input feature vector
 * @param num_inputs Input size
 * @param output Output buffer (rule conclusion activations)
 * @param num_outputs Output size
 * @return true if rule fires (output > threshold), false otherwise
 */
bool brain_execute_neural_rule(
    brain_t brain,
    const neural_compilation_result_t* result,
    const float* input,
    uint32_t num_inputs,
    float* output,
    uint32_t num_outputs
);

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Destroy learned rule and free memory
 * @param rule Rule to destroy
 */
void learned_rule_destroy(learned_rule_t* rule);

/**
 * @brief Destroy symbolic association
 * @param assoc Association to destroy
 */
void symbolic_association_destroy(symbolic_association_t* assoc);

/**
 * @brief Destroy neural compilation result
 * @param result Compilation result to destroy
 */
void neural_compilation_result_destroy(neural_compilation_result_t* result);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get learned rules from brain's symbolic logic system
 *
 * @param brain Brain handle
 * @param rules Output: array of learned rules
 * @param num_rules Output: number of rules
 * @return true on success, false on failure
 */
bool brain_get_learned_rules(
    brain_t brain,
    learned_rule_t*** rules,
    int* num_rules
);

/**
 * @brief Get symbolic associations from brain
 *
 * @param brain Brain handle
 * @param associations Output: array of associations
 * @param num_associations Output: number of associations
 * @return true on success, false on failure
 */
bool brain_get_symbolic_associations(
    brain_t brain,
    symbolic_association_t*** associations,
    int* num_associations
);

/**
 * @brief Print learned rule in human-readable format
 *
 * @param rule Rule to print
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return true on success, false on failure
 */
bool learned_rule_to_string(
    const learned_rule_t* rule,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_LEARNING_H */
