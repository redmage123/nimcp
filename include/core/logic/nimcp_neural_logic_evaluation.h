/**
 * @file nimcp_neural_logic_evaluation.h
 * @brief MODULE 2: Neural Logic Evaluation - Evaluate Logic Gates Through Brain
 * @version 4.0.0
 * @date 2025-12-31
 *
 * WHAT: Evaluation interface for logic gates with event publishing
 * WHY:  Single Responsibility: Execute logic operations and notify observers
 * HOW:  Wrapper around neural_logic_evaluate() with event bus integration
 *
 * VERSION 4.0.0 CHANGES:
 * - Refactored to use nimcp_tensor_t for variable-arity logic gates
 * - Removed fixed-size input arrays (was limited to 4 inputs)
 * - Added tensor-based batch evaluation for GPU acceleration
 * - Support for N-ary logic gates (AND_N, OR_N, etc.)
 *
 * SINGLE RESPONSIBILITY PRINCIPLE (SRP):
 * - SOLE RESPONSIBILITY: Evaluate logic gates and publish evaluation events
 * - DOES: Evaluate single gates, evaluate expressions, publish EVENT_LOGIC_GATE_EVALUATED
 * - DOES NOT: Attach networks (MODULE 1), build circuits (MODULE 3), modulate (MODULE 4)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_LOGIC_EVALUATION_H
#define NIMCP_NEURAL_LOGIC_EVALUATION_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "utils/tensor/nimcp_tensor.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Tensor-Based Logic Gate Event Structure
//=============================================================================

/**
 * @brief Event payload for EVENT_LOGIC_GATE_EVALUATED (tensor-based)
 *
 * WHAT: Data structure published when logic gate is evaluated
 * WHY:  Support variable-arity logic gates (not limited to 4 inputs)
 * HOW:  Uses nimcp_tensor_t for inputs, allowing any number of inputs
 *
 * MEMORY OWNERSHIP:
 * - inputs_tensor: Owned by event system, cloned from evaluation inputs
 * - Destroyed when event is consumed or times out
 *
 * EXAMPLE:
 * ```c
 * // AND gate with 8 inputs
 * uint32_t dims[] = {8};
 * nimcp_tensor_t* inputs = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
 * // ... fill inputs ...
 * // Event will contain copy of this tensor
 * ```
 */
typedef struct {
    uint32_t gate_id;               /**< Logic gate neuron ID */
    uint32_t num_inputs;            /**< Number of inputs (tensor size) */
    nimcp_tensor_t* inputs_tensor;  /**< Input values as 1D tensor [num_inputs] */
    float output;                   /**< Output value [0,1] */
    uint64_t timestamp_us;          /**< Evaluation timestamp */
    uint64_t eval_time_us;          /**< Evaluation duration */
} logic_gate_evaluated_event_t;

/**
 * @brief Batch evaluation result (tensor-based)
 *
 * WHAT: Results from batch gate evaluation
 * WHY:  Efficient GPU batch processing with tensor storage
 * HOW:  Stores all inputs/outputs as tensors for GPU transfer
 */
typedef struct {
    uint32_t num_gates;             /**< Number of gates evaluated */
    nimcp_tensor_t* gate_ids;       /**< Gate IDs as 1D tensor [num_gates] (I32) */
    nimcp_tensor_t* all_inputs;     /**< Flattened inputs [total_inputs] (F32) */
    nimcp_tensor_t* input_offsets;  /**< Input offsets per gate [num_gates] (I32) */
    nimcp_tensor_t* inputs_per_gate;/**< Input counts per gate [num_gates] (I32) */
    nimcp_tensor_t* outputs;        /**< Outputs [num_gates] (F32) */
    uint64_t total_eval_time_us;    /**< Total batch evaluation time */
} logic_batch_result_t;

/**
 * @brief Create batch result structure
 *
 * @param num_gates Number of gates to evaluate
 * @param total_inputs Total number of inputs across all gates
 * @return Allocated batch result, or NULL on failure
 */
NIMCP_EXPORT logic_batch_result_t* logic_batch_result_create(
    uint32_t num_gates,
    uint32_t total_inputs
);

/**
 * @brief Destroy batch result and free tensors
 *
 * @param result Batch result to destroy (NULL-safe)
 */
NIMCP_EXPORT void logic_batch_result_destroy(logic_batch_result_t* result);

//=============================================================================
// MODULE 2: Neural Logic Evaluation API
//=============================================================================

/**
 * @brief Evaluate logic gate through brain interface
 *
 * WHAT: Execute logic gate evaluation with inputs and capture output
 * WHY:  Provide brain-level wrapper for neural logic evaluation
 * HOW:  Validate inputs → call neural_logic_evaluate() → publish event → return result
 *
 * @param brain Brain instance with attached logic network
 * @param gate_id Logic gate neuron ID
 * @param inputs Input values (boolean encoded as float 0/1), array of size num_inputs
 * @param num_inputs Number of inputs (typically 1-2 for binary gates)
 * @param output Output value [0,1] (OUT parameter)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic == NULL → false + error log
 * - NULL inputs → false + error log
 * - NULL output → false + error log
 * - num_inputs == 0 → false + error log
 *
 * BEHAVIOR:
 * - Validates all inputs with guard clauses
 * - Calls neural_logic_evaluate(brain->logic, gate_id, inputs, num_inputs, output)
 * - On success: publishes EVENT_LOGIC_GATE_EVALUATED event
 * - On failure: logs error and returns false
 *
 * EVENTS:
 * - EVENT_LOGIC_GATE_EVALUATED: Fires after successful evaluation
 *   Payload: gate_id, inputs, num_inputs, output, timestamp
 *
 * COMPLEXITY: O(t * n) where t = simulation time, n = neurons in gate circuit
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * float inputs[2] = {1.0f, 1.0f};  // A=true, B=true
 * float output;
 *
 * if (brain_evaluate_logic_gate(brain, and_gate_id, inputs, 2, &output)) {
 *     printf("AND(1, 1) = %.1f\n", output);  // Expected: 1.0
 * }
 * ```
 */
NIMCP_EXPORT bool brain_evaluate_logic_gate(
    brain_t brain,
    uint32_t gate_id,
    const float* inputs,
    uint32_t num_inputs,
    float* output
);

/**
 * @brief Evaluate logic gate using tensor inputs (variable-arity)
 *
 * WHAT: Execute logic gate evaluation with tensor-based inputs
 * WHY:  Support N-ary logic gates (AND with 8+ inputs, etc.)
 * HOW:  Validate inputs -> extract tensor data -> evaluate -> publish event
 *
 * @param brain Brain instance with attached logic network
 * @param gate_id Logic gate neuron ID
 * @param inputs Input tensor (1D, F32) with variable number of inputs
 * @param output Output value [0,1] (OUT parameter)
 * @return true on success, false on failure
 *
 * TENSOR REQUIREMENTS:
 * - Must be 1D tensor (rank 1)
 * - Must be F32 dtype
 * - Size determines number of inputs (no limit)
 *
 * N-ARY GATE BEHAVIOR:
 * - AND_N: Output = 1 if ALL inputs >= threshold
 * - OR_N: Output = 1 if ANY input >= threshold
 * - MAJORITY: Output = 1 if >50% inputs >= threshold
 *
 * EXAMPLE:
 * ```c
 * // 8-input AND gate
 * uint32_t dims[] = {8};
 * nimcp_tensor_t* inputs = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
 * float output;
 * brain_evaluate_logic_gate_tensor(brain, and8_gate_id, inputs, &output);
 * nimcp_tensor_destroy(inputs);
 * ```
 */
NIMCP_EXPORT bool brain_evaluate_logic_gate_tensor(
    brain_t brain,
    uint32_t gate_id,
    const nimcp_tensor_t* inputs,
    float* output
);

/**
 * @brief Evaluate logic expression with variable bindings
 *
 * WHAT: Evaluate string expression (e.g., "A AND B") with variable substitution
 * WHY:  Provide high-level interface for complex logic without manual gate management
 * HOW:  Parse expression → bind variables → evaluate circuit → publish event
 *
 * @param brain Brain instance with attached logic network
 * @param expression Logical expression string (e.g., "A AND B", "(A OR B) AND C")
 * @param bindings Variable bindings (A→inputs[0], B→inputs[1], ..., Z→inputs[25])
 * @param num_bindings Number of variable bindings (typically matches unique vars in expr)
 * @param output Evaluation result [0,1] (OUT parameter)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic == NULL → false + error log
 * - NULL expression → false + error log
 * - Empty expression → false + error log
 * - NULL bindings → false + error log (if num_bindings > 0)
 * - NULL output → false + error log
 *
 * BEHAVIOR:
 * - Parses expression into circuit (uses MODULE 3 internally)
 * - Maps variable bindings to gate inputs (A→bindings[0], B→bindings[1], ...)
 * - Evaluates circuit with neural_logic_evaluate()
 * - Publishes EVENT_LOGIC_GATE_EVALUATED event
 * - Cleans up temporary circuit
 *
 * VARIABLE BINDING:
 * - Variables are single uppercase letters (A-Z)
 * - bindings[0] = value for 'A', bindings[1] = value for 'B', etc.
 * - Expression can use subset of variables (e.g., "A AND C" uses bindings[0], bindings[2])
 *
 * SUPPORTED OPERATORS:
 * - AND: "A AND B", "A & B", "A ∧ B"
 * - OR:  "A OR B", "A | B", "A ∨ B"
 * - NOT: "NOT A", "!A", "¬A"
 * - XOR: "A XOR B", "A ⊕ B"
 * - IMPLIES: "A -> B", "A → B"
 * - Parentheses: "(A AND B) OR C"
 *
 * COMPLEXITY: O(m + t * n) where m = expression length, t = sim time, n = neurons
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * // Evaluate: (A AND B) OR C
 * const char* expr = "(A AND B) OR C";
 * float bindings[3] = {1.0f, 1.0f, 0.0f};  // A=T, B=T, C=F
 * float output;
 *
 * if (brain_evaluate_logic_expression(brain, expr, bindings, 3, &output)) {
 *     printf("Result: %.1f\n", output);  // Expected: 1.0 (true OR false = true)
 * }
 * ```
 */
NIMCP_EXPORT bool brain_evaluate_logic_expression(
    brain_t brain,
    const char* expression,
    const float* bindings,
    uint32_t num_bindings,
    float* output
);

/**
 * @brief Get last evaluation statistics
 *
 * WHAT: Retrieve timing and performance metrics from last evaluation
 * WHY:  Enable performance monitoring and optimization
 * HOW:  Query neural logic network stats for last gate evaluation
 *
 * @param brain Brain instance with attached logic network
 * @param eval_time_us Output: evaluation time in microseconds (OUT parameter)
 * @param spike_count Output: number of spikes during evaluation (OUT parameter)
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic == NULL → false + error log
 * - NULL eval_time_us → false + error log
 * - NULL spike_count → false + error log
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe for read-only access
 *
 * EXAMPLE:
 * ```c
 * float inputs[2] = {1.0f, 0.0f};
 * float output;
 * brain_evaluate_logic_gate(brain, gate_id, inputs, 2, &output);
 *
 * uint64_t eval_time;
 * uint32_t spikes;
 * brain_get_evaluation_stats(brain, &eval_time, &spikes);
 * printf("Evaluation: %llu μs, %u spikes\n", eval_time, spikes);
 * ```
 */
NIMCP_EXPORT bool brain_get_evaluation_stats(
    brain_t brain,
    uint64_t* eval_time_us,
    uint32_t* spike_count
);

//=============================================================================
// MODULE 2.1: Batch Logic Gate Evaluation API (GPU-Accelerated)
//=============================================================================

/**
 * @brief Batch evaluate multiple logic gates on GPU
 *
 * WHAT: Evaluate many logic gates in parallel using GPU acceleration
 * WHY:  100x speedup for bulk logic operations vs sequential evaluation
 * HOW:  Transfer data to GPU -> launch batch kernel -> copy results back
 *
 * @param brain Brain instance with attached logic network
 * @param gate_ids Array of logic gate neuron IDs [num_gates]
 * @param num_gates Number of gates to evaluate
 * @param all_inputs Flattened inputs: [gate0_in0, gate0_in1, gate1_in0, gate1_in1, ...]
 * @param inputs_per_gate Number of inputs for each gate [num_gates] (typically 2 for binary gates)
 * @param outputs Output values [num_gates] - one per gate
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain -> false + error log
 * - brain->logic == NULL -> false + error log
 * - NULL gate_ids -> false + error log
 * - num_gates == 0 -> false + error log
 * - NULL all_inputs -> false + error log
 * - NULL inputs_per_gate -> false + error log
 * - NULL outputs -> false + error log
 *
 * BEHAVIOR:
 * - If GPU available: batch transfer, parallel kernel, batch result copy
 * - If CPU fallback: sequential evaluation via brain_evaluate_logic_gate()
 * - Publishes single batch event (not individual gate events)
 *
 * COMPLEXITY: O(n) on GPU, O(n * t) on CPU where n = num_gates, t = sim time
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * uint32_t gate_ids[3] = {and_gate, or_gate, xor_gate};
 * float all_inputs[6] = {1.0f, 1.0f,   // AND inputs
 *                        1.0f, 0.0f,   // OR inputs
 *                        1.0f, 1.0f};  // XOR inputs
 * uint32_t inputs_per_gate[3] = {2, 2, 2};
 * float outputs[3];
 *
 * if (brain_evaluate_logic_gates_batch(brain, gate_ids, 3, all_inputs, inputs_per_gate, outputs)) {
 *     // outputs[0] = 1.0 (AND), outputs[1] = 1.0 (OR), outputs[2] = 0.0 (XOR)
 * }
 * ```
 */
NIMCP_EXPORT bool brain_evaluate_logic_gates_batch(
    brain_t brain,
    const uint32_t* gate_ids,
    uint32_t num_gates,
    const float* all_inputs,
    const uint32_t* inputs_per_gate,
    float* outputs
);

/**
 * @brief Batch evaluate logic gates using tensors (GPU-optimized)
 *
 * WHAT: Evaluate multiple logic gates in parallel with tensor I/O
 * WHY:  Maximum GPU efficiency with tensor-based memory management
 * HOW:  Transfer tensors to GPU -> batch kernel -> transfer results back
 *
 * @param brain Brain instance with attached logic network
 * @param gate_ids_tensor 1D tensor of gate IDs [num_gates] (I32)
 * @param inputs_tensor 1D tensor of all inputs flattened [total_inputs] (F32)
 * @param offsets_tensor 1D tensor of input offsets [num_gates] (I32)
 * @param counts_tensor 1D tensor of input counts [num_gates] (I32)
 * @param result Pre-allocated batch result structure (outputs filled in)
 * @return true on success, false on failure
 *
 * TENSOR LAYOUT:
 * - gate_ids_tensor: [gate0_id, gate1_id, ...]
 * - inputs_tensor: [gate0_in0, gate0_in1, gate1_in0, ...]
 * - offsets_tensor: [0, 2, 5, ...] (cumulative offsets)
 * - counts_tensor: [2, 3, 2, ...] (inputs per gate)
 *
 * GPU OPTIMIZATION:
 * - All tensors transferred as single GPU allocation
 * - Coalesced memory access pattern
 * - 256 threads per block for optimal occupancy
 *
 * EXAMPLE:
 * ```c
 * // Evaluate 3 gates with variable inputs
 * uint32_t gate_dims[] = {3};
 * nimcp_tensor_t* gate_ids = nimcp_tensor_create(gate_dims, 1, NIMCP_DTYPE_I32);
 * // ... fill gate_ids, inputs, offsets, counts ...
 *
 * logic_batch_result_t* result = logic_batch_result_create(3, 7);
 * brain_evaluate_logic_gates_batch_tensor(brain, gate_ids, inputs, offsets, counts, result);
 * // Access result->outputs tensor
 * logic_batch_result_destroy(result);
 * ```
 */
NIMCP_EXPORT bool brain_evaluate_logic_gates_batch_tensor(
    brain_t brain,
    const nimcp_tensor_t* gate_ids_tensor,
    const nimcp_tensor_t* inputs_tensor,
    const nimcp_tensor_t* offsets_tensor,
    const nimcp_tensor_t* counts_tensor,
    logic_batch_result_t* result
);

/**
 * @brief Batch evaluate multiple logic expressions on GPU
 *
 * WHAT: Evaluate many string expressions in parallel with GPU acceleration
 * WHY:  Efficient bulk evaluation of logic formulas
 * HOW:  Parse expressions -> build circuits -> batch GPU evaluation
 *
 * @param brain Brain instance with attached logic network
 * @param expressions Array of expression strings [num_expressions]
 * @param num_expressions Number of expressions to evaluate
 * @param bindings Array of binding arrays (one per expression)
 * @param num_bindings_per_expr Number of bindings for each expression [num_expressions]
 * @param outputs Output values [num_expressions] - one per expression
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain -> false + error log
 * - brain->logic == NULL -> false + error log
 * - NULL expressions -> false + error log
 * - num_expressions == 0 -> false + error log
 * - NULL bindings -> false + error log
 * - NULL num_bindings_per_expr -> false + error log
 * - NULL outputs -> false + error log
 *
 * BEHAVIOR:
 * - Parses all expressions into circuit IDs
 * - Collects all gate evaluations needed
 * - Performs batch GPU evaluation
 * - Maps results back to expression outputs
 *
 * COMPLEXITY: O(m * n) where m = total expression length, n = neurons
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * const char* expressions[2] = {"A AND B", "A OR C"};
 * float bindings_0[2] = {1.0f, 1.0f};  // A=1, B=1
 * float bindings_1[3] = {0.0f, 0.0f, 1.0f};  // A=0, B=0, C=1
 * const float* bindings[2] = {bindings_0, bindings_1};
 * uint32_t num_bindings[2] = {2, 3};
 * float outputs[2];
 *
 * if (brain_evaluate_logic_expressions_batch(brain, expressions, 2, bindings, num_bindings, outputs)) {
 *     // outputs[0] = 1.0 (A AND B), outputs[1] = 1.0 (A OR C)
 * }
 * ```
 */
NIMCP_EXPORT bool brain_evaluate_logic_expressions_batch(
    brain_t brain,
    const char** expressions,
    uint32_t num_expressions,
    const float** bindings,
    const uint32_t* num_bindings_per_expr,
    float* outputs
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_EVALUATION_H
