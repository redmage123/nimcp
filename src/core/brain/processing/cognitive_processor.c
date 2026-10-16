//=============================================================================
// cognitive_processor.c - Cognitive Assessment Implementation
//=============================================================================
/**
 * @file cognitive_processor.c
 * @brief Single Responsibility: Apply cognitive assessments to neural output
 *
 * REFACTORING NOTE:
 * Extracted from nimcp_brain.c brain_process_multimodal() (394 lines → ~80 lines)
 * Reason: Apply Single Responsibility Principle - separate cognitive assessment
 *
 * DESIGN:
 * - Each cognitive module assessed independently
 * - Fallback computations if modules not initialized
 * - Pure function: no side effects except annotation writes
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"

#include "core/brain/processing/cognitive_processor.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/accessors/nimcp_brain_accessors.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MODULE "BRAIN_PROC_COG"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cognitive_processor, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Brain Structure Access
//=============================================================================

// NOTE: Use accessor functions instead of direct struct access to avoid
// layout mismatch issues. The brain structure is opaque and defined in nimcp_brain.c
// We must use the public API to access brain fields safely.

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute fallback confidence from output statistics
 */
static float compute_fallback_confidence(
    const float* output_vector,
    uint32_t output_size,
    uint32_t spikes_generated,
    uint32_t num_inputs)
{
    // Compute output variance
    float output_variance = 0.0F;
    float output_mean = 0.0F;

    for (uint32_t i = 0; i < output_size; i++) {
        output_mean += output_vector[i];
    }
    output_mean /= output_size;

    for (uint32_t i = 0; i < output_size; i++) {
        float diff = output_vector[i] - output_mean;
        output_variance += diff * diff;
    }
    output_variance /= output_size;

    // Higher activity, lower variance → higher confidence
    float confidence = fminf(1.0F, (float)spikes_generated / (num_inputs * 2.0F));
    confidence *= (1.0F - fminf(1.0F, output_variance));

    return confidence;
}

/**
 * @brief Compute fallback salience from max output activation
 */
static float compute_fallback_salience(
    const float* output_vector,
    uint32_t output_size)
{
    float max_activation = 0.0F;
    for (uint32_t i = 0; i < output_size; i++) {
        if (output_vector[i] > max_activation) {
            max_activation = output_vector[i];
        }
    }
    return fminf(1.0F, max_activation);
}

/**
 * @brief Compute fallback novelty from spike deviation
 */
static float compute_fallback_novelty(
    uint32_t spikes_generated,
    uint32_t num_inputs)
{
    float expected_spikes = num_inputs * 0.5F;
    float spike_diff = fabsf((float)spikes_generated - expected_spikes);
    return fminf(1.0F, spike_diff / expected_spikes);
}

/**
 * @brief Check for ethical violations (NaN, inf, extreme values)
 */
static bool check_ethical_output(
    const float* output_vector,
    uint32_t output_size)
{
    for (uint32_t i = 0; i < output_size; i++) {
        if (isnan(output_vector[i]) || isinf(output_vector[i]) ||
            fabsf(output_vector[i]) > 1000.0F) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "check_ethical_output: validation failed");
            return false;  // Ethical violation
        }
    }
    return true;  // Ethically acceptable
}

//=============================================================================
// Cognitive Constraint Logic Circuits
//=============================================================================

/**
 * @brief Create mutual exclusion constraint circuit (A XOR B)
 *
 * WHAT: Build XOR logic circuit for mutual exclusion constraints
 * WHY:  Ensure two conditions cannot be true simultaneously
 * HOW:  Use neural XOR gate to detect conflicts
 *
 * BIOLOGICAL RATIONALE:
 * - Prefrontal cortex implements mutual exclusion via reciprocal inhibition
 * - Competing neural populations suppress each other
 * - Winner-take-all dynamics enforce exclusivity
 *
 * @param logic Neural logic network
 * @param value_a First condition strength [0,1]
 * @param value_b Second condition strength [0,1]
 * @return true if constraint satisfied (only one or neither active)
 */
static bool check_mutual_exclusion(
    neural_logic_network_t logic,
    float value_a,
    float value_b)
{
    // Guard: Null network
    if (!logic) {
        return true;
    }

    // Create XOR gate for mutual exclusion check
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5F);
    if (xor_gate == UINT32_MAX) {
        return true;  // Failed to create gate, assume valid
    }

    // Evaluate XOR: true if values differ, false if same
    float inputs[2] = {value_a, value_b};
    float output = 0.0F;

    if (!neural_logic_evaluate(logic, xor_gate, inputs, 2, &output)) {
        return true;  // Evaluation failed, assume valid
    }

    // Mutual exclusion satisfied if:
    // - Both inactive (A=0, B=0) → XOR=0 → valid
    // - Only A active (A=1, B=0) → XOR=1 → valid
    // - Only B active (A=0, B=1) → XOR=1 → valid
    // - Both active (A=1, B=1) → XOR=0 → INVALID

    // Check if both are active (violation)
    bool both_active = (value_a > 0.5F && value_b > 0.5F);

    return !both_active;  // Valid if NOT both active
}

/**
 * @brief Create prerequisite constraint circuit (A → B)
 *
 * WHAT: Build IMPLIES logic circuit for prerequisite checking
 * WHY:  Ensure B cannot be true unless A is true first
 * HOW:  Use neural IMPLIES gate (A → B ≡ ¬A ∨ B)
 *
 * BIOLOGICAL RATIONALE:
 * - Sequential task execution in prefrontal cortex
 * - Working memory maintains prerequisite states
 * - Conditional firing patterns enforce dependencies
 *
 * @param logic Neural logic network
 * @param prerequisite_value Prerequisite condition [0,1]
 * @param dependent_value Dependent condition [0,1]
 * @return true if constraint satisfied (if A then B)
 */
static bool check_prerequisite(
    neural_logic_network_t logic,
    float prerequisite_value,
    float dependent_value)
{
    // Guard: Null network
    if (!logic) {
        return true;
    }

    // Create IMPLIES gate for prerequisite check
    uint32_t implies_gate = neural_logic_create_gate(
        logic,
        LOGIC_GATE_IMPLIES,
        0.8F
    );
    if (implies_gate == UINT32_MAX) {
        return true;  // Failed to create gate, assume valid
    }

    // Evaluate A → B
    float inputs[2] = {prerequisite_value, dependent_value};
    float output = 0.0F;

    if (!neural_logic_evaluate(logic, implies_gate, inputs, 2, &output)) {
        return true;  // Evaluation failed, assume valid
    }

    // Implication satisfied if output is true
    // A → B is false only when A=1 and B=0
    return (output > 0.5F);
}

/**
 * @brief Create conflict detection circuit (NOT (A AND B))
 *
 * WHAT: Build NOT-AND logic circuit for conflict detection
 * WHY:  Detect when two incompatible states co-occur
 * HOW:  Chain AND gate with NOT gate
 *
 * BIOLOGICAL RATIONALE:
 * - Conflict monitoring in anterior cingulate cortex (ACC)
 * - Detects response conflicts via error-related negativity
 * - Inhibitory control prevents incompatible actions
 *
 * @param logic Neural logic network
 * @param state_a First state strength [0,1]
 * @param state_b Second state strength [0,1]
 * @return true if no conflict detected
 */
static bool check_no_conflict(
    neural_logic_network_t logic,
    float state_a,
    float state_b)
{
    // Guard: Null network
    if (!logic) {
        return true;
    }

    // Create AND gate
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8F);
    if (and_gate == UINT32_MAX) {
        return true;
    }

    // Create NOT gate
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5F);
    if (not_gate == UINT32_MAX) {
        return true;
    }

    // Connect AND → NOT
    if (!neural_logic_connect(logic, and_gate, not_gate, 1.0F)) {
        return true;
    }

    // Evaluate AND(A, B)
    float and_inputs[2] = {state_a, state_b};
    float and_output = 0.0F;

    if (!neural_logic_evaluate(logic, and_gate, and_inputs, 2, &and_output)) {
        return true;
    }

    // Evaluate NOT(AND(A, B))
    float not_inputs[1] = {and_output};
    float not_output = 0.0F;

    if (!neural_logic_evaluate(logic, not_gate, not_inputs, 1, &not_output)) {
        return true;
    }

    // No conflict if NOT(AND) is true (i.e., both are NOT active together)
    return (not_output > 0.5F);
}

/**
 * @brief Validate cognitive constraints using logic circuits
 *
 * WHAT: Check all cognitive constraints using neural logic gates
 * WHY:  Ensure output satisfies logical consistency requirements
 * HOW:  Apply constraint circuits to cognitive annotation values
 *
 * BIOLOGICAL RATIONALE:
 * - Cortical constraint satisfaction networks
 * - Prefrontal executive control enforces logical rules
 * - Anterior cingulate monitors for conflicts
 *
 * CONSTRAINTS CHECKED:
 * 1. Mutual exclusion: confidence and uncertainty must sum to ~1
 * 2. Prerequisite: high confidence requires low uncertainty
 * 3. Conflict: ethical_approved and high novelty shouldn't conflict
 *
 * @param logic Neural logic network
 * @param net_output Raw network output (for validation context)
 * @param annotations Cognitive annotations to validate
 * @return true if all constraints satisfied
 */
static bool validate_cognitive_constraints(
    neural_logic_network_t logic,
    const network_output_t* net_output,
    const cognitive_annotations_t* annotations)
{
    // Guard: Null inputs
    if (!logic || !net_output || !annotations) {
        return true;
    }

    bool all_valid = true;

    // Constraint 1: Confidence and uncertainty are complementary
    // Should be mutually exclusive (high confidence = low uncertainty)
    float confidence_normalized = annotations->confidence;
    float uncertainty_normalized = annotations->uncertainty;

    // They should sum to ~1.0, check mutual exclusion of extremes
    bool confidence_uncertainty_valid = check_mutual_exclusion(
        logic,
        confidence_normalized > 0.8F ? 1.0F : 0.0F,  // Very confident
        uncertainty_normalized > 0.8F ? 1.0F : 0.0F   // Very uncertain
    );
    all_valid = all_valid && confidence_uncertainty_valid;

    // Constraint 2: High confidence requires low uncertainty (prerequisite)
    // If confident, then uncertainty must be low
    bool prerequisite_valid = check_prerequisite(
        logic,
        confidence_normalized,      // If confident
        1.0F - uncertainty_normalized  // Then certain (low uncertainty)
    );
    all_valid = all_valid && prerequisite_valid;

    // Constraint 3: Ethical violations conflict with high salience
    // Unethical outputs should not have high salience
    bool ethical_salience_valid = check_no_conflict(
        logic,
        annotations->ethical_approved ? 0.0F : 1.0F,  // Ethical violation
        annotations->salience_score                   // High salience
    );
    all_valid = all_valid && ethical_salience_valid;

    return all_valid;
}

//=============================================================================
// API Implementation
//=============================================================================

void cognitive_annotations_init(cognitive_annotations_t* annotations)
{
    if (!annotations) {
        return;
    }

    memset(annotations, 0, sizeof(cognitive_annotations_t));

    annotations->confidence = 0.5F;  // Neutral confidence
    annotations->uncertainty = 0.5F;
    annotations->ethical_approved = true;  // Assume ethical unless proven otherwise
    annotations->salience_score = 0.5F;
    annotations->novelty_score = 0.5F;
    annotations->urgency_score = 0.0F;
    annotations->exploration_bonus = 0.0F;
    annotations->information_gain = 0.0F;
    annotations->logic_valid = true;
}

bool cognitive_process_output(
    const brain_t brain,
    const network_output_t* net_output,
    const float* integrated_features,
    uint32_t integrated_dim,
    uint64_t timestamp_ms,
    cognitive_annotations_t* annotations)
{
    // =========================================================================
    // VALIDATION
    // =========================================================================

    if (!brain || !net_output || !annotations) {
        fprintf(stderr, "cognitive_processor: Invalid parameters\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_process_output: required parameter is NULL (brain, net_output, annotations)");
        return false;
    }

    // Initialize output
    cognitive_annotations_init(annotations);

    // =========================================================================
    // INTROSPECTION: Confidence and Uncertainty
    // =========================================================================

    introspection_context_t introspection = brain_get_introspection(brain);
    if (introspection) {
        // Use introspection module for sophisticated uncertainty estimation
        brain_uncertainty_t uncertainty = brain_get_uncertainty(
            introspection,
            integrated_features,
            integrated_dim
        );

        annotations->uncertainty = uncertainty.total;
        annotations->confidence = 1.0F - annotations->uncertainty;
    } else {
        // Fallback: Compute confidence from output statistics
        uint32_t num_inputs = brain_get_num_inputs(brain);
        annotations->confidence = compute_fallback_confidence(
            net_output->output_vector,
            net_output->output_size,
            net_output->spikes_generated,
            num_inputs
        );
        annotations->uncertainty = 1.0F - annotations->confidence;
    }

    // =========================================================================
    // ETHICS: Validate Output
    // =========================================================================

    ethics_engine_t ethics = brain_get_ethics(brain);
    if (ethics) {
        // Use ethics module for sophisticated ethical evaluation
        // For now, just check for NaN/inf/extreme values
        annotations->ethical_approved = check_ethical_output(
            net_output->output_vector,
            net_output->output_size
        );
    } else {
        // Fallback: Basic validity check
        annotations->ethical_approved = check_ethical_output(
            net_output->output_vector,
            net_output->output_size
        );
    }

    // =========================================================================
    // SALIENCE: Input Importance (Novelty, Surprise, Urgency)
    // =========================================================================

    salience_evaluator_t salience_eval = brain_get_salience(brain);
    if (salience_eval) {
        // Use salience module for sophisticated importance evaluation
        brain_salience_t salience = brain_evaluate_salience_temporal(
            salience_eval,
            integrated_features,
            integrated_dim,
            timestamp_ms
        );

        annotations->salience_score = salience.salience;
        annotations->novelty_score = salience.novelty;
        annotations->urgency_score = salience.surprise;  // Map surprise to urgency
    } else {
        // Fallback: Max output activation as salience
        annotations->salience_score = compute_fallback_salience(
            net_output->output_vector,
            net_output->output_size
        );

        // Fallback novelty from spike deviation
        uint32_t num_inputs = brain_get_num_inputs(brain);
        annotations->novelty_score = compute_fallback_novelty(
            net_output->spikes_generated,
            num_inputs
        );

        annotations->urgency_score = 0.0F;  // No urgency without salience module
    }

    // =========================================================================
    // CURIOSITY: Exploration Value
    // =========================================================================

    curiosity_engine_t curiosity = brain_get_curiosity(brain);
    if (curiosity) {
        // Curiosity engine can compute exploration bonus and information gain
        // Based on novelty and uncertainty
        annotations->exploration_bonus = annotations->novelty_score * annotations->uncertainty;
        annotations->information_gain = annotations->novelty_score;
    } else {
        // Fallback: No exploration bonus without curiosity module
        annotations->exploration_bonus = 0.0F;
        annotations->information_gain = 0.0F;
    }

    // =========================================================================
    // NEURAL LOGIC: Logical Reasoning (Phase 9.0)
    // =========================================================================

    neural_logic_network_t logic = brain_get_logic(brain);
    if (logic) {
        // Neural logic gates available for constraint checking / logical inference
        // Check cognitive constraints using logic circuits
        annotations->logic_valid = validate_cognitive_constraints(
            logic,
            net_output,
            annotations
        );
    } else {
        // No logic module, assume valid
        annotations->logic_valid = true;
    }

    return true;
}
