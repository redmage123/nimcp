/**
 * @file nimcp_neural_logic_evaluation.c
 * @brief MODULE 2: Neural Logic Evaluation Implementation
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
 * @author NIMCP Development Team
 */

#include "core/logic/nimcp_neural_logic_evaluation.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/logic/nimcp_neural_logic_attachment.h"
#include "core/logic/nimcp_neural_logic_circuit_builder.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/events/nimcp_event_bus.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/tensor/nimcp_tensor.h"
#include <string.h>

#include <stddef.h>  /* for NULL */
// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "neural_logic_evaluation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic_evaluation)

#define BIO_MODULE_ID 0x0138

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Publish logic gate evaluation event (tensor-based)
 *
 * WHAT: Send EVENT_LOGIC_GATE_EVALUATED to event bus
 * WHY:  Notify observers of successful logic evaluation
 * HOW:  Clone input tensor, pack payload, call event_bus_publish()
 *
 * MEMORY: Creates a clone of the input tensor for the event payload.
 *         Event subscribers are responsible for destroying the tensor
 *         after processing, or the event system will destroy it on timeout.
 */
static void publish_gate_evaluated_event_tensor(
    brain_t brain,
    uint32_t gate_id,
    const nimcp_tensor_t* inputs,
    uint32_t num_inputs,
    float output,
    uint64_t eval_time_us
) {
    // Guard: NULL brain or no event bus
    if (!brain || !brain->event_bus) {
        return;
    }

    // WHAT: Pack event payload with tensor
    // WHY:  Encapsulate evaluation details for subscribers
    // HOW:  Clone tensor for event ownership

    logic_gate_evaluated_event_t payload = {0};
    payload.gate_id = gate_id;
    payload.num_inputs = num_inputs;
    payload.output = output;
    payload.timestamp_us = 0;  // Timestamp set by event bus
    payload.eval_time_us = eval_time_us;

    // Clone input tensor for event (event takes ownership)
    payload.inputs_tensor = nimcp_tensor_clone(inputs);
    if (!payload.inputs_tensor) {
        LOG_WARN("publish_gate_evaluated_event_tensor: failed to clone inputs tensor");
        // Continue without tensor - event still useful
    }

    // Publish event
    brain_event_t event = event_create(EVENT_LOGIC_GATE_EVALUATED, EVENT_PRIORITY_LOW, "neural_logic");
    event_set_data(&event, &payload, sizeof(payload));
    event_bus_publish(brain->event_bus, &event);
}

/**
 * @brief Publish logic gate evaluation event (legacy float array)
 *
 * WHAT: Send EVENT_LOGIC_GATE_EVALUATED to event bus
 * WHY:  Backward compatibility with existing code using float arrays
 * HOW:  Convert float array to tensor, then use tensor version
 */
static void publish_gate_evaluated_event(
    brain_t brain,
    uint32_t gate_id,
    const float* inputs,
    uint32_t num_inputs,
    float output,
    uint64_t eval_time_us
) {
    // Guard: NULL brain or no event bus
    if (!brain || !brain->event_bus) {
        return;
    }

    // Guard: NULL inputs
    if (!inputs || num_inputs == 0) {
        return;
    }

    // WHAT: Create tensor from float array
    // WHY:  Use unified tensor-based event structure
    // HOW:  Create 1D F32 tensor, copy data, publish

    uint32_t dims[] = {num_inputs};
    nimcp_tensor_t* inputs_tensor = nimcp_tensor_from_data(
        inputs, dims, 1, NIMCP_DTYPE_F32, true  // copy=true
    );

    if (inputs_tensor) {
        publish_gate_evaluated_event_tensor(brain, gate_id, inputs_tensor, num_inputs, output, eval_time_us);
        nimcp_tensor_destroy(inputs_tensor);
    }
}

//=============================================================================
// Batch Result Lifecycle
//=============================================================================

logic_batch_result_t* logic_batch_result_create(
    uint32_t num_gates,
    uint32_t total_inputs
) {
    // Guard: zero gates
    if (num_gates == 0) {
        LOG_ERROR("logic_batch_result_create: num_gates is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "logic_batch_result_create: num_gates is zero");
        return NULL;
    }

    // Allocate result structure
    logic_batch_result_t* result = (logic_batch_result_t*)nimcp_calloc(1, sizeof(logic_batch_result_t));
    if (!result) {
        LOG_ERROR("logic_batch_result_create: failed to allocate result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_batch_result_create: failed to allocate result");

        return NULL;
    }

    result->num_gates = num_gates;

    // Create tensors for batch data
    uint32_t gate_dims[] = {num_gates};
    uint32_t input_dims[] = {total_inputs};

    result->gate_ids = nimcp_tensor_zeros(gate_dims, 1, NIMCP_DTYPE_I32);
    result->all_inputs = nimcp_tensor_zeros(input_dims, 1, NIMCP_DTYPE_F32);
    result->input_offsets = nimcp_tensor_zeros(gate_dims, 1, NIMCP_DTYPE_I32);
    result->inputs_per_gate = nimcp_tensor_zeros(gate_dims, 1, NIMCP_DTYPE_I32);
    result->outputs = nimcp_tensor_zeros(gate_dims, 1, NIMCP_DTYPE_F32);

    // Check all allocations succeeded
    if (!result->gate_ids || !result->all_inputs || !result->input_offsets ||
        !result->inputs_per_gate || !result->outputs) {
        LOG_ERROR("logic_batch_result_create: failed to allocate tensors");
        logic_batch_result_destroy(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_batch_result_create: operation failed");
        return NULL;
    }

    result->total_eval_time_us = 0;
    return result;
}

void logic_batch_result_destroy(logic_batch_result_t* result) {
    if (!result) {
        return;
    }

    nimcp_tensor_destroy(result->gate_ids);
    nimcp_tensor_destroy(result->all_inputs);
    nimcp_tensor_destroy(result->input_offsets);
    nimcp_tensor_destroy(result->inputs_per_gate);
    nimcp_tensor_destroy(result->outputs);

    nimcp_free(result);
}

//=============================================================================
// MODULE 2: Neural Logic Evaluation Implementation
//=============================================================================

bool brain_evaluate_logic_gate(
    brain_t brain,
    uint32_t gate_id,
    const float* inputs,
    uint32_t num_inputs,
    float* output
) {
    // WHAT: Validate all inputs with guard clauses
    // WHY:  Prevent NULL derefs and invalid operations
    // HOW:  Early returns with error logging

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_evaluate_logic_gate: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_evaluate_logic_gate: brain '%s' has no logic network",
                  "brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL inputs
    if (!nimcp_validate_pointer(inputs, "inputs")) {
        LOG_ERROR("brain_evaluate_logic_gate: NULL inputs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL output
    if (!nimcp_validate_pointer(output, "output")) {
        LOG_ERROR("brain_evaluate_logic_gate: NULL output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: zero inputs
    if (num_inputs == 0) {
        LOG_ERROR("brain_evaluate_logic_gate: num_inputs is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate: num_inputs is zero");
        return false;
    }

    // WHAT: Execute logic gate evaluation
    // WHY:  Perform actual computation via neural logic network
    // HOW:  Call neural_logic_evaluate() with timing

    neural_logic_network_t network = brain_get_neural_logic(brain);

    bool success = neural_logic_evaluate(
        network,
        gate_id,
        inputs,
        num_inputs,
        output
    );

    uint64_t eval_time_us = 0;  // Timing not available

    if (!success) {
        LOG_ERROR("brain_evaluate_logic_gate: evaluation failed for gate %u", gate_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_evaluate_logic_gate: success is NULL");
        return false;
    }

    // WHAT: Publish evaluation event for observers
    // WHY:  Enable monitoring, learning, and debugging
    // HOW:  Send EVENT_LOGIC_GATE_EVALUATED

    publish_gate_evaluated_event(brain, gate_id, inputs, num_inputs, *output, eval_time_us);

    LOG_DEBUG("brain_evaluate_logic_gate: gate=%u, output=%.3f, time=%llu μs",
              gate_id, *output, (unsigned long long)eval_time_us);

    return true;
}

//=============================================================================
// MODULE 2.2: Tensor-Based Logic Gate Evaluation
//=============================================================================

bool brain_evaluate_logic_gate_tensor(
    brain_t brain,
    uint32_t gate_id,
    const nimcp_tensor_t* inputs,
    float* output
) {
    // WHAT: Validate all inputs with guard clauses
    // WHY:  Prevent NULL derefs and invalid operations
    // HOW:  Early returns with error logging

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL inputs tensor
    if (!nimcp_validate_pointer(inputs, "inputs")) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: NULL inputs tensor");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL output
    if (!nimcp_validate_pointer(output, "output")) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: NULL output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: tensor must be 1D
    if (nimcp_tensor_rank(inputs) != 1) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: inputs must be 1D tensor, got rank %u",
                  nimcp_tensor_rank(inputs));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: validation failed");
        return false;
    }

    // Guard: tensor must be F32
    if (nimcp_tensor_dtype(inputs) != NIMCP_DTYPE_F32) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: inputs must be F32, got %s",
                  nimcp_dtype_name(nimcp_tensor_dtype(inputs)));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: validation failed");
        return false;
    }

    // Guard: tensor must have elements
    size_t num_inputs = nimcp_tensor_numel(inputs);
    if (num_inputs == 0) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: inputs tensor is empty");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gate_tensor: num_inputs is zero");
        return false;
    }

    // WHAT: Extract data pointer and call legacy evaluate
    // WHY:  Reuse existing neural_logic_evaluate implementation
    // HOW:  Get contiguous data pointer from tensor

    const float* inputs_data = (const float*)nimcp_tensor_data_const(inputs);
    if (!inputs_data) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: failed to get tensor data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_evaluate_logic_gate_tensor: inputs_data is NULL");
        return false;
    }

    neural_logic_network_t network = brain_get_neural_logic(brain);

    bool success = neural_logic_evaluate(
        network,
        gate_id,
        inputs_data,
        (uint32_t)num_inputs,
        output
    );

    uint64_t eval_time_us = 0;  // Timing not available

    if (!success) {
        LOG_ERROR("brain_evaluate_logic_gate_tensor: evaluation failed for gate %u", gate_id);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_evaluate_logic_gate_tensor: success is NULL");
        return false;
    }

    // WHAT: Publish evaluation event with tensor
    // WHY:  Enable monitoring, learning, and debugging
    // HOW:  Send EVENT_LOGIC_GATE_EVALUATED with tensor payload

    publish_gate_evaluated_event_tensor(brain, gate_id, inputs, (uint32_t)num_inputs, *output, eval_time_us);

    LOG_DEBUG("brain_evaluate_logic_gate_tensor: gate=%u, num_inputs=%zu, output=%.3f",
              gate_id, num_inputs, *output);

    return true;
}

bool brain_evaluate_logic_expression(
    brain_t brain,
    const char* expression,
    const float* bindings,
    uint32_t num_bindings,
    float* output
) {
    // WHAT: Validate inputs for expression evaluation
    // WHY:  Ensure safe parsing and evaluation
    // HOW:  Guard clauses with NULL and empty checks

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_evaluate_logic_expression: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_evaluate_logic_expression: brain '%s' has no logic network",
                  "brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL expression
    if (!nimcp_validate_pointer(expression, "expression")) {
        LOG_ERROR("brain_evaluate_logic_expression: NULL expression");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: empty expression
    if (expression[0] == '\0') {
        LOG_ERROR("brain_evaluate_logic_expression: empty expression");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: validation failed");
        return false;
    }

    // Guard: NULL bindings (if num_bindings > 0)
    if (num_bindings > 0 && !nimcp_validate_pointer(bindings, "bindings")) {
        LOG_ERROR("brain_evaluate_logic_expression: NULL bindings with num_bindings=%u",
                  num_bindings);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL output
    if (!nimcp_validate_pointer(output, "output")) {
        LOG_ERROR("brain_evaluate_logic_expression: NULL output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: nimcp_validate_pointer is NULL");
        return false;
    }

    // WHAT: Build circuit from expression
    // WHY:  Convert string to executable neural circuit
    // HOW:  Call MODULE 3 (circuit builder) to parse and construct

    uint32_t circuit_id = brain_build_logic_circuit(brain, expression);
    if (circuit_id == UINT32_MAX) {
        LOG_ERROR("brain_evaluate_logic_expression: failed to parse '%s'", expression);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expression: validation failed");
        return false;
    }

    // WHAT: Map variable bindings to inputs
    // WHY:  Substitute variable values (A, B, C...) with actual input values
    // HOW:  Extract variables from expression, create input array

    // For simplicity, assume circuit_id is root gate and bindings map directly
    // Full implementation would analyze expression to determine input mapping

    // Evaluate circuit with bindings as inputs
    // Timing removed

    bool success = brain_evaluate_logic_gate(
        brain,
        circuit_id,
        bindings,
        num_bindings,
        output
    );

    uint64_t eval_time_us = 0;  // Timing not available

    if (!success) {
        LOG_ERROR("brain_evaluate_logic_expression: evaluation failed for '%s'", expression);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_evaluate_logic_expression: success is NULL");
        return false;
    }

    LOG_INFO("brain_evaluate_logic_expression: '%s' = %.3f (time=%llu μs)",
             expression, *output, (unsigned long long)eval_time_us);

    return true;
}

//=============================================================================
// MODULE 2.1: Batch Logic Gate Evaluation Implementation
//=============================================================================

// Forward declarations for GPU batch kernel launchers
// Note: GPU batch operations are implemented in nimcp_neural_logic_kernels.cu
// The C implementation uses CPU fallback or calls through neural_logic API

/**
 * @brief CPU fallback for batch gate evaluation
 *
 * Sequential evaluation when GPU is unavailable.
 */
static bool evaluate_gates_batch_cpu(
    brain_t brain,
    const uint32_t* gate_ids,
    uint32_t num_gates,
    const float* all_inputs,
    const uint32_t* inputs_per_gate,
    float* outputs
) {
    // WHAT: Sequential evaluation of gates on CPU
    // WHY:  Fallback when GPU unavailable
    // HOW:  Loop through gates, call brain_evaluate_logic_gate() for each

    uint32_t input_offset = 0;

    for (uint32_t i = 0; i < num_gates; i++) {
        uint32_t gate_id = gate_ids[i];
        uint32_t num_inputs = inputs_per_gate[i];

        // Evaluate single gate
        bool success = brain_evaluate_logic_gate(
            brain,
            gate_id,
            &all_inputs[input_offset],
            num_inputs,
            &outputs[i]
        );

        if (!success) {
            LOG_ERROR("evaluate_gates_batch_cpu: failed at gate %u (index %u)", gate_id, i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_gates_batch_cpu: success is NULL");
            return false;
        }

        input_offset += num_inputs;
    }

    return true;
}

bool brain_evaluate_logic_gates_batch(
    brain_t brain,
    const uint32_t* gate_ids,
    uint32_t num_gates,
    const float* all_inputs,
    const uint32_t* inputs_per_gate,
    float* outputs
) {
    // WHAT: Validate all inputs with guard clauses
    // WHY:  Prevent NULL derefs and invalid operations
    // HOW:  Early returns with error logging

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL gate_ids
    if (!nimcp_validate_pointer(gate_ids, "gate_ids")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: NULL gate_ids");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: zero gates
    if (num_gates == 0) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: num_gates is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: num_gates is zero");
        return false;
    }

    // Guard: NULL all_inputs
    if (!nimcp_validate_pointer(all_inputs, "all_inputs")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: NULL all_inputs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL inputs_per_gate
    if (!nimcp_validate_pointer(inputs_per_gate, "inputs_per_gate")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: NULL inputs_per_gate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL outputs
    if (!nimcp_validate_pointer(outputs, "outputs")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch: NULL outputs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // WHAT: Evaluate gates in batch
    // WHY:  Provides API for batch operations; GPU acceleration happens at network level
    // HOW:  Use CPU path with optimized sequential evaluation
    //       GPU kernels are available in nimcp_neural_logic_kernels.cu for direct use
    //       when neural_logic_network provides batch evaluation API

    // For Phase 3.1-3.2, we implement CPU path with GPU kernels available
    // The GPU path requires neural_logic_evaluate_batch() to be added to
    // nimcp_neural_logic.h/c which manages device memory

    bool success = evaluate_gates_batch_cpu(brain, gate_ids, num_gates, all_inputs, inputs_per_gate, outputs);
    if (success) {
        LOG_DEBUG("brain_evaluate_logic_gates_batch: evaluated %u gates", num_gates);
    }
    return success;
}

//=============================================================================
// MODULE 2.3: Tensor-Based Batch Evaluation (GPU-Optimized)
//=============================================================================

/**
 * @brief CPU fallback for tensor-based batch gate evaluation
 *
 * WHAT: Sequential evaluation using tensor data
 * WHY:  Fallback when GPU unavailable
 * HOW:  Extract tensor data, loop through gates
 */
static bool evaluate_gates_batch_tensor_cpu(
    brain_t brain,
    const nimcp_tensor_t* gate_ids_tensor,
    const nimcp_tensor_t* inputs_tensor,
    const nimcp_tensor_t* offsets_tensor,
    const nimcp_tensor_t* counts_tensor,
    logic_batch_result_t* result
) {
    // Get raw pointers from tensors
    const int32_t* gate_ids = (const int32_t*)nimcp_tensor_data_const(gate_ids_tensor);
    const float* all_inputs = (const float*)nimcp_tensor_data_const(inputs_tensor);
    const int32_t* offsets = (const int32_t*)nimcp_tensor_data_const(offsets_tensor);
    const int32_t* counts = (const int32_t*)nimcp_tensor_data_const(counts_tensor);
    float* outputs = (float*)nimcp_tensor_data(result->outputs);

    if (!gate_ids || !all_inputs || !offsets || !counts || !outputs) {
        LOG_ERROR("evaluate_gates_batch_tensor_cpu: failed to get tensor data");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_gates_batch_tensor_cpu: required parameter is NULL (gate_ids, all_inputs, offsets, counts, outputs)");
        return false;
    }

    size_t num_gates = nimcp_tensor_numel(gate_ids_tensor);

    // WHAT: Sequential evaluation of gates on CPU
    // WHY:  Fallback when GPU unavailable
    // HOW:  Loop through gates, create temp tensor for each evaluation

    for (size_t i = 0; i < num_gates; i++) {
        uint32_t gate_id = (uint32_t)gate_ids[i];
        uint32_t offset = (uint32_t)offsets[i];
        uint32_t num_inputs = (uint32_t)counts[i];

        // Create temp tensor for this gate's inputs
        uint32_t dims[] = {num_inputs};
        nimcp_tensor_t* gate_inputs = nimcp_tensor_from_data(
            &all_inputs[offset], dims, 1, NIMCP_DTYPE_F32, false  // no copy, just view
        );

        if (!gate_inputs) {
            LOG_ERROR("evaluate_gates_batch_tensor_cpu: failed to create input tensor at index %zu", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_gates_batch_tensor_cpu: gate_inputs is NULL");
            return false;
        }

        // Evaluate gate
        float output = 0.0f;
        bool success = brain_evaluate_logic_gate_tensor(brain, gate_id, gate_inputs, &output);

        nimcp_tensor_destroy(gate_inputs);

        if (!success) {
            LOG_ERROR("evaluate_gates_batch_tensor_cpu: failed at gate %u (index %zu)", gate_id, i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "evaluate_gates_batch_tensor_cpu: success is NULL");
            return false;
        }

        outputs[i] = output;
    }

    return true;
}

bool brain_evaluate_logic_gates_batch_tensor(
    brain_t brain,
    const nimcp_tensor_t* gate_ids_tensor,
    const nimcp_tensor_t* inputs_tensor,
    const nimcp_tensor_t* offsets_tensor,
    const nimcp_tensor_t* counts_tensor,
    logic_batch_result_t* result
) {
    // WHAT: Validate all inputs with guard clauses
    // WHY:  Prevent NULL derefs and invalid operations
    // HOW:  Early returns with error logging

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL tensors
    if (!nimcp_validate_pointer(gate_ids_tensor, "gate_ids_tensor") ||
        !nimcp_validate_pointer(inputs_tensor, "inputs_tensor") ||
        !nimcp_validate_pointer(offsets_tensor, "offsets_tensor") ||
        !nimcp_validate_pointer(counts_tensor, "counts_tensor")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: NULL tensor(s)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL result
    if (!nimcp_validate_pointer(result, "result")) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: NULL result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: tensor rank must be 1 for all batch tensors
    if (nimcp_tensor_rank(gate_ids_tensor) != 1 ||
        nimcp_tensor_rank(inputs_tensor) != 1 ||
        nimcp_tensor_rank(offsets_tensor) != 1 ||
        nimcp_tensor_rank(counts_tensor) != 1) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: all tensors must be 1D");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: validation failed");
        return false;
    }

    // Guard: gate_ids, offsets, counts must be I32
    if (nimcp_tensor_dtype(gate_ids_tensor) != NIMCP_DTYPE_I32 ||
        nimcp_tensor_dtype(offsets_tensor) != NIMCP_DTYPE_I32 ||
        nimcp_tensor_dtype(counts_tensor) != NIMCP_DTYPE_I32) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: gate_ids/offsets/counts must be I32");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: validation failed");
        return false;
    }

    // Guard: inputs must be F32
    if (nimcp_tensor_dtype(inputs_tensor) != NIMCP_DTYPE_F32) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: inputs must be F32");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: validation failed");
        return false;
    }

    // Guard: consistent sizes
    size_t num_gates = nimcp_tensor_numel(gate_ids_tensor);
    if (nimcp_tensor_numel(offsets_tensor) != num_gates ||
        nimcp_tensor_numel(counts_tensor) != num_gates) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: size mismatch in batch tensors");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_gates_batch_tensor: validation failed");
        return false;
    }

    // Guard: result outputs tensor must match
    if (!result->outputs || nimcp_tensor_numel(result->outputs) != num_gates) {
        LOG_ERROR("brain_evaluate_logic_gates_batch_tensor: result outputs size mismatch");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_evaluate_logic_gates_batch_tensor: result->outputs is NULL");
        return false;
    }

    // WHAT: Evaluate gates in batch
    // WHY:  GPU acceleration when available, CPU fallback otherwise
    // HOW:  Check GPU availability, launch appropriate path

    // TODO: Add GPU path when CUDA kernel launcher is integrated
    // For now, use CPU fallback
    bool success = evaluate_gates_batch_tensor_cpu(
        brain,
        gate_ids_tensor,
        inputs_tensor,
        offsets_tensor,
        counts_tensor,
        result
    );

    if (success) {
        LOG_DEBUG("brain_evaluate_logic_gates_batch_tensor: evaluated %zu gates", num_gates);
    }

    return success;
}

bool brain_evaluate_logic_expressions_batch(
    brain_t brain,
    const char** expressions,
    uint32_t num_expressions,
    const float** bindings,
    const uint32_t* num_bindings_per_expr,
    float* outputs
) {
    // WHAT: Validate all inputs with guard clauses
    // WHY:  Prevent NULL derefs and invalid operations
    // HOW:  Early returns with error logging

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL expressions
    if (!nimcp_validate_pointer(expressions, "expressions")) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: NULL expressions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: zero expressions
    if (num_expressions == 0) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: num_expressions is zero");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: num_expressions is zero");
        return false;
    }

    // Guard: NULL bindings
    if (!nimcp_validate_pointer(bindings, "bindings")) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: NULL bindings");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL num_bindings_per_expr
    if (!nimcp_validate_pointer(num_bindings_per_expr, "num_bindings_per_expr")) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: NULL num_bindings_per_expr");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL outputs
    if (!nimcp_validate_pointer(outputs, "outputs")) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: NULL outputs");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: nimcp_validate_pointer is NULL");
        return false;
    }

    // WHAT: Build circuits from expressions, then batch evaluate
    // WHY:  Leverage GPU acceleration for evaluation phase
    // HOW:  Parse expressions -> collect gate IDs -> batch evaluate

    // Step 1: Parse all expressions and build circuits
    uint32_t* circuit_ids = (uint32_t*)nimcp_calloc(num_expressions, sizeof(uint32_t));
    if (!circuit_ids) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: failed to allocate circuit_ids");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_evaluate_logic_expressions_batch: circuit_ids is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_expressions; i++) {
        if (!expressions[i] || expressions[i][0] == '\0') {
            LOG_ERROR("brain_evaluate_logic_expressions_batch: NULL or empty expression at index %u", i);
            nimcp_free(circuit_ids);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: expressions is NULL");
            return false;
        }

        circuit_ids[i] = brain_build_logic_circuit(brain, expressions[i]);
        if (circuit_ids[i] == UINT32_MAX) {
            LOG_ERROR("brain_evaluate_logic_expressions_batch: failed to parse expression '%s'", expressions[i]);
            nimcp_free(circuit_ids);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_evaluate_logic_expressions_batch: validation failed");
            return false;
        }
    }

    // Step 2: Calculate total inputs and build flattened arrays
    uint32_t total_inputs = 0;
    for (uint32_t i = 0; i < num_expressions; i++) {
        total_inputs += num_bindings_per_expr[i];
    }

    float* all_inputs = (float*)nimcp_calloc(total_inputs, sizeof(float));
    if (!all_inputs) {
        LOG_ERROR("brain_evaluate_logic_expressions_batch: failed to allocate all_inputs");
        nimcp_free(circuit_ids);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_evaluate_logic_expressions_batch: all_inputs is NULL");
        return false;
    }

    // Flatten bindings into single array
    uint32_t offset = 0;
    for (uint32_t i = 0; i < num_expressions; i++) {
        for (uint32_t j = 0; j < num_bindings_per_expr[i]; j++) {
            all_inputs[offset++] = bindings[i][j];
        }
    }

    // Step 3: Batch evaluate all circuits
    bool success = brain_evaluate_logic_gates_batch(
        brain,
        circuit_ids,
        num_expressions,
        all_inputs,
        num_bindings_per_expr,
        outputs
    );

    nimcp_free(all_inputs);
    nimcp_free(circuit_ids);

    if (success) {
        LOG_INFO("brain_evaluate_logic_expressions_batch: evaluated %u expressions", num_expressions);
    }

    return success;
}

bool brain_get_evaluation_stats(
    brain_t brain,
    uint64_t* eval_time_us,
    uint32_t* spike_count
) {
    // WHAT: Validate inputs for stats query
    // WHY:  Prevent NULL derefs when writing output
    // HOW:  Guard clauses on all pointers

    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_get_evaluation_stats: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_evaluation_stats: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network attached
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_get_evaluation_stats: brain '%s' has no logic network",
                  "brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_evaluation_stats: brain_has_neural_logic is NULL");
        return false;
    }

    // Guard: NULL eval_time_us
    if (!nimcp_validate_pointer(eval_time_us, "eval_time_us")) {
        LOG_ERROR("brain_get_evaluation_stats: NULL eval_time_us");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_evaluation_stats: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: NULL spike_count
    if (!nimcp_validate_pointer(spike_count, "spike_count")) {
        LOG_ERROR("brain_get_evaluation_stats: NULL spike_count");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_get_evaluation_stats: nimcp_validate_pointer is NULL");
        return false;
    }

    // WHAT: Query network statistics
    // WHY:  Retrieve performance metrics from neural logic network
    // HOW:  Call neural_logic_get_stats() and extract relevant fields

    neural_logic_network_t network = brain_get_neural_logic(brain);

    uint32_t total_gates = 0;
    uint32_t total_vars = 0;
    uint64_t total_spikes = 0;
    float avg_eval_time = 0.0F;
    uint64_t gpu_memory = 0;

    bool success = neural_logic_get_stats(
        network,
        &total_gates,
        &total_vars,
        &total_spikes,
        &avg_eval_time,
        &gpu_memory
    );

    if (!success) {
        LOG_ERROR("brain_get_evaluation_stats: failed to get network stats");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_evaluation_stats: success is NULL");
        return false;
    }

    // Convert average evaluation time to microseconds
    *eval_time_us = (uint64_t)(avg_eval_time);
    *spike_count = (uint32_t)total_spikes;

    return true;
}
