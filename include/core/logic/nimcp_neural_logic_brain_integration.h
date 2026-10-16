/**
 * @file nimcp_neural_logic_brain_integration.h
 * @brief Neural Logic Brain Integration - Connect Neural Logic Networks to Brain
 * @version 2.6.2
 * @date 2025-11-20
 *
 * WHAT: Integration layer connecting neural logic gates to brain architecture
 * WHY:  Enable brain-modulated logical reasoning with DA/ACh neuromodulation
 * HOW:  Brain wrapper functions for neural logic + neuromodulator-based threshold modulation
 *
 * ARCHITECTURE:
 *
 *   ┌────────────────────────────────────────────────────┐
 *   │                  Brain Instance                    │
 *   │                                                    │
 *   │  ┌─────────────────────────────────────┐          │
 *   │  │  Neural Logic Network (logic field) │          │
 *   │  │                                     │          │
 *   │  │  ┌─────┐  ┌─────┐  ┌─────┐        │          │
 *   │  │  │ AND │  │ OR  │  │ XOR │  ...   │          │
 *   │  │  └─────┘  └─────┘  └─────┘        │          │
 *   │  │      ↑         ↑         ↑         │          │
 *   │  │      └─────────┴─────────┘         │          │
 *   │  │    Neuromodulation (DA + ACh)      │          │
 *   │  └─────────────────────────────────────┘          │
 *   │                                                    │
 *   │  Neuromodulator System (DA, ACh, 5-HT, NE...)     │
 *   └────────────────────────────────────────────────────┘
 *
 * NEUROMODULATION EFFECTS:
 *
 * 1. DOPAMINE (DA):
 *    - HIGH DA → Lower gate thresholds → Permissive logic (exploratory, creative)
 *    - LOW DA → Higher gate thresholds → Rigid logic (perseverative, inflexible)
 *    - CLINICAL: Depression (low DA) → black-and-white thinking
 *                 Mania (high DA) → loose associations, illogical leaps
 *
 * 2. ACETYLCHOLINE (ACh):
 *    - HIGH ACh → Precise thresholds → Accurate logic (focused, attentive)
 *    - LOW ACh → Imprecise thresholds → Error-prone logic (distracted, confused)
 *    - CLINICAL: ADHD (low ACh) → logical errors, misses contradictions
 *                 Dementia (low ACh) → impaired reasoning, confabulation
 *
 * MODULATION FORMULA:
 *   threshold_modulated = threshold_base * (1.0 - DA_level * 0.3) * (1.0 + ACh_level * 0.2)
 *
 * USAGE:
 *   // Create brain with neural logic
 *   brain_t brain = brain_create("reasoning", BRAIN_SIZE_SMALL);
 *   brain_create_neural_logic(brain, config);
 *
 *   // Build circuit from expression
 *   uint32_t circuit_id = brain_neural_logic_build_circuit(brain, "(A AND B) OR C");
 *
 *   // Evaluate with brain neuromodulation
 *   float inputs[2] = {1.0f, 1.0f};
 *   float output;
 *   brain_neural_logic_evaluate(brain, circuit_id, inputs, 2, &output);
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_LOGIC_BRAIN_INTEGRATION_H
#define NIMCP_NEURAL_LOGIC_BRAIN_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Brain-Neural Logic Integration API
//=============================================================================

/**
 * @brief Create neural logic network attached to brain
 *
 * WHAT: Initialize neural logic network and attach to brain instance
 * WHY:  Enable brain-modulated logical reasoning with DA/ACh effects
 * HOW:  Create network, set brain reference, store in brain->logic field
 *
 * @param brain Brain instance (must be non-NULL)
 * @param config Neural logic configuration (NULL for defaults)
 * @return true on success, false on failure
 *
 * BEHAVIOR:
 * - Creates neural logic network with specified configuration
 * - Sets brain reference in network for neuromodulation
 * - Stores network handle in brain->logic field
 * - Configures DA/ACh modulation based on brain neuromodulator levels
 *
 * COMPLEXITY: O(n) where n = max_logic_neurons in config
 * THREAD SAFETY: Not thread-safe, call from single thread during brain init
 */
NIMCP_EXPORT bool brain_create_neural_logic(
    brain_t brain,
    const neural_logic_config_t* config
);

/**
 * @brief Destroy neural logic network attached to brain
 *
 * WHAT: Clean up neural logic network and clear brain reference
 * WHY:  Prevent memory leaks during brain shutdown
 * HOW:  Destroy network, set brain->logic to NULL
 *
 * @param brain Brain instance (NULL-safe)
 *
 * BEHAVIOR:
 * - Destroys neural logic network if present
 * - Sets brain->logic to NULL
 * - Safe to call multiple times
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT void brain_destroy_neural_logic(brain_t brain);

/**
 * @brief Evaluate logic gate with brain neuromodulation
 *
 * WHAT: Wrapper for neural_logic_evaluate with DA/ACh modulation
 * WHY:  Provide brain-aware logic evaluation interface
 * HOW:  Read brain DA/ACh, modulate thresholds, call neural_logic_evaluate
 *
 * @param brain Brain instance (must have logic network)
 * @param gate_id Logic gate neuron ID
 * @param inputs Input values (boolean encoded as float 0/1)
 * @param num_inputs Number of inputs (typically 1-2)
 * @param output Logical output [0,1] (OUT parameter)
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * float inputs[2] = {1.0f, 1.0f};  // A=true, B=true
 * float output;
 * bool success = brain_neural_logic_evaluate(brain, and_gate, inputs, 2, &output);
 * // output ≈ 1.0 (modulated by brain DA/ACh)
 * ```
 *
 * NEUROMODULATION:
 * - Reads brain->neuromodulator_system for DA and ACh levels
 * - Modulates gate thresholds before evaluation
 * - High DA → lower thresholds (permissive logic)
 * - High ACh → precise thresholds (accurate logic)
 *
 * COMPLEXITY: O(t * n) where t = simulation time, n = neurons
 */
NIMCP_EXPORT bool brain_neural_logic_evaluate(
    brain_t brain,
    uint32_t gate_id,
    const float* inputs,
    uint32_t num_inputs,
    float* output
);

/**
 * @brief Build logic circuit from expression string
 *
 * WHAT: Parse logical expression and construct neural circuit
 * WHY:  High-level interface for complex logic without manual gate creation
 * HOW:  Recursive descent parser → create gates → connect → return root ID
 *
 * @param brain Brain instance (must have logic network)
 * @param expression Logical expression string (e.g., "(A AND B) OR C")
 * @return Root gate ID, or UINT32_MAX on parse error
 *
 * SUPPORTED SYNTAX:
 * - AND: "A AND B", "A & B", "A ∧ B"
 * - OR:  "A OR B", "A | B", "A ∨ B"
 * - NOT: "NOT A", "!A", "¬A"
 * - XOR: "A XOR B", "A ⊕ B"
 * - IMPLIES: "A -> B", "A → B"
 * - Parentheses: "(A AND B) OR C"
 * - Variables: Single uppercase letters (A-Z)
 *
 * EXAMPLE:
 * ```c
 * // Build: (A AND B) OR (C AND NOT D)
 * uint32_t circuit = brain_neural_logic_build_circuit(brain, "(A AND B) OR (C AND NOT D)");
 *
 * // Bind variables to input neurons
 * float inputs[4] = {1.0f, 1.0f, 0.0f, 1.0f};  // A=T, B=T, C=F, D=T
 * float output;
 * brain_neural_logic_evaluate(brain, circuit, inputs, 4, &output);
 * // output = (T AND T) OR (F AND F) = T OR F = T = 1.0
 * ```
 *
 * PARSE ALGORITHM:
 * 1. Tokenize expression (operators, variables, parentheses)
 * 2. Build abstract syntax tree (AST) via recursive descent
 * 3. Post-order traversal: create gates for each AST node
 * 4. Connect gates according to tree structure
 * 5. Return root gate ID
 *
 * ERROR HANDLING:
 * - Syntax errors → UINT32_MAX, logs error message
 * - Unbalanced parentheses → UINT32_MAX
 * - Unknown operators → UINT32_MAX
 * - Empty expression → UINT32_MAX
 *
 * COMPLEXITY: O(n) where n = expression length
 */
NIMCP_EXPORT uint32_t brain_neural_logic_build_circuit(
    brain_t brain,
    const char* expression
);

/**
 * @brief Get neuromodulator-modulated threshold for gate
 *
 * WHAT: Compute effective threshold with DA/ACh modulation
 * WHY:  Expose modulation effects for debugging and monitoring
 * HOW:  Read brain neuromodulators, apply modulation formula
 *
 * @param brain Brain instance (must have neuromodulator system)
 * @param base_threshold Unmodulated threshold value
 * @param modulated_threshold Output modulated threshold (OUT parameter)
 * @return true on success, false on failure
 *
 * FORMULA:
 *   threshold_mod = threshold_base * (1.0 - DA * 0.3) * (1.0 + ACh * 0.2)
 *
 * EXAMPLE:
 * ```c
 * float base = 1.5f;
 * float modulated;
 * brain_neural_logic_get_modulated_threshold(brain, base, &modulated);
 * // If DA=0.8, ACh=0.6:
 * // modulated = 1.5 * (1.0 - 0.8*0.3) * (1.0 + 0.6*0.2)
 * //           = 1.5 * 0.76 * 1.12 = 1.276
 * ```
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT bool brain_neural_logic_get_modulated_threshold(
    brain_t brain,
    float base_threshold,
    float* modulated_threshold
);

/**
 * @brief Apply neuromodulation to all gates in logic network
 *
 * WHAT: Update all gate thresholds based on current brain DA/ACh
 * WHY:  Synchronize logic network with brain neuromodulator state
 * HOW:  Iterate gates, read brain neuromodulators, apply modulation
 *
 * @param brain Brain instance (must have logic network)
 * @return Number of gates modulated, 0 on failure
 *
 * BEHAVIOR:
 * - Reads brain->neuromodulator_system for DA and ACh levels
 * - Applies modulation formula to each gate's threshold
 * - Updates gate states in network
 * - Returns count of successfully modulated gates
 *
 * USAGE:
 * - Call after significant brain activity (training, decision)
 * - Call periodically (e.g., every 100ms) for continuous modulation
 * - Automatic in brain_neural_logic_evaluate() for single gates
 *
 * COMPLEXITY: O(n) where n = number of gates in network
 */
NIMCP_EXPORT uint32_t brain_neural_logic_apply_neuromodulation(brain_t brain);

/**
 * @brief Get logic network statistics with neuromodulation info
 *
 * WHAT: Retrieve network stats plus current DA/ACh modulation levels
 * WHY:  Monitor logic network health and neuromodulation effects
 * HOW:  Query neural logic network + read brain neuromodulators
 *
 * @param brain Brain instance (must have logic network)
 * @param total_gates Output: total logic gates (OUT parameter)
 * @param total_variables Output: total variables (OUT parameter)
 * @param total_spikes Output: total spikes emitted (OUT parameter)
 * @param da_level Output: current dopamine level [0,1] (OUT parameter)
 * @param ach_level Output: current acetylcholine level [0,1] (OUT parameter)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool brain_neural_logic_get_stats(
    brain_t brain,
    uint32_t* total_gates,
    uint32_t* total_variables,
    uint64_t* total_spikes,
    float* da_level,
    float* ach_level
);

//=============================================================================
// Circuit Building Utilities (Internal, exposed for testing)
//=============================================================================

/**
 * @brief Parse variable name from expression
 *
 * INTERNAL: Used by circuit builder, exposed for unit testing
 *
 * @param expr Expression string
 * @param pos Current position (IN/OUT parameter, advanced on success)
 * @param var_name Output variable name (single char A-Z)
 * @return true on success, false on parse error
 */
NIMCP_EXPORT bool parse_variable(const char* expr, size_t* pos, char* var_name);

/**
 * @brief Parse operator from expression
 *
 * INTERNAL: Used by circuit builder, exposed for unit testing
 *
 * @param expr Expression string
 * @param pos Current position (IN/OUT parameter, advanced on success)
 * @param gate_type Output gate type (AND/OR/NOT/XOR/IMPLIES)
 * @return true on success, false on parse error
 */
NIMCP_EXPORT bool parse_operator(const char* expr, size_t* pos, logic_gate_type_t* gate_type);

/**
 * @brief Skip whitespace in expression
 *
 * INTERNAL: Used by parser, exposed for testing
 *
 * @param expr Expression string
 * @param pos Current position (IN/OUT parameter, advanced past whitespace)
 */
NIMCP_EXPORT void skip_whitespace(const char* expr, size_t* pos);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_BRAIN_INTEGRATION_H
