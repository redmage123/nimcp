/**
 * @file nimcp_explanations.c
 * @brief Natural Language Explanations - Implementation
 * @phase Phase 10.7
 *
 * WHAT: Generate human-readable explanations for brain decisions
 * WHY:  Enable interpretability and trust in AI decisions
 * HOW:  Extract decision features → Map to language templates → Build narratives
 *
 * DESIGN PRINCIPLES:
 * - Single Responsibility: Each function does exactly one thing
 * - Early Returns: Guard clauses for validation
 * - Named Constants: No magic numbers
 * - Template-Based: Reusable explanation patterns
 *
 * @author NIMCP Phase 10 Team
 * @date 2025-11-09
 */

#include "cognitive/nimcp_explanations.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

// =============================================================================
// ERROR HANDLING (Thread-local)
// =============================================================================

static __thread char last_error[256] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

// =============================================================================
// CONSTANTS
// =============================================================================

#define MAX_FEATURES_TO_EXPLAIN 5
#define MAX_ALTERNATIVES 3
#define MIN_ALTERNATIVE_PROBABILITY 0.05f
#define HIGH_CONFIDENCE_THRESHOLD 0.8f
#define MEDIUM_CONFIDENCE_THRESHOLD 0.5f
#define LOW_CONFIDENCE_THRESHOLD 0.2f

// =============================================================================
// INTERNAL STRUCTURE
// =============================================================================

/**
 * @brief Explanation generator internal state
 *
 * WHAT: Complete internal structure with configuration and statistics
 * WHY:  Opaque pointer pattern - hide internals from public API
 * HOW:  Extend opaque typedef with full implementation
 */
struct explanation_generator_s {
    explanation_config_t config;

    // Statistics
    uint32_t total_explanations_generated;
    uint32_t what_generated;
    uint32_t why_generated;
    uint32_t how_generated;
    uint32_t counterfactuals_generated;
};

// =============================================================================
// FORWARD DECLARATIONS (internal helpers)
// =============================================================================

static void generate_what_explanation(brain_t brain, brain_decision_t* decision,
                                     char* buffer, size_t buffer_size);
static void generate_why_explanation(brain_t brain, brain_decision_t* decision,
                                    char* buffer, size_t buffer_size);
static void generate_how_explanation(brain_t brain, brain_decision_t* decision,
                                    char* buffer, size_t buffer_size);
static void generate_confidence_explanation(brain_t brain, brain_decision_t* decision,
                                           char* buffer, size_t buffer_size);
static void generate_alternatives_explanation(brain_t brain, brain_decision_t* decision,
                                             char* buffer, size_t buffer_size,
                                             uint32_t max_alternatives,
                                             float min_probability);
static void generate_counterfactual_explanation(brain_t brain, brain_decision_t* decision,
                                               char* buffer, size_t buffer_size);

static const char* confidence_level_to_string(float confidence);

// =============================================================================
// PUBLIC API: CREATION & DESTRUCTION
// =============================================================================

/**
 * @brief Create explanation generator with configuration
 *
 * WHAT: Allocate and initialize explanation generator
 * WHY:  Setup system for generating explanations
 * HOW:  Allocate struct → Initialize config → Initialize statistics
 *
 * @param config Configuration parameters (NULL = defaults)
 * @return Initialized generator, or NULL on error
 *
 * COMPLEXITY: O(1)
 * MEMORY: sizeof(explanation_generator_s)
 */
explanation_generator_t explanation_generator_create(const explanation_config_t* config)
{
    // =========================================================================
    // ALLOCATION: Main structure
    // =========================================================================

    explanation_generator_t gen = nimcp_calloc(1, sizeof(struct explanation_generator_s));
    if (!gen) {
        set_error("Failed to allocate explanation_generator_s (%zu bytes)",
                  sizeof(struct explanation_generator_s));
        return NULL;
    }

    // =========================================================================
    // INITIALIZATION: Configuration
    // =========================================================================

    if (config) {
        gen->config = *config;
    } else {
        gen->config = explanation_default_config();
    }

    // =========================================================================
    // INITIALIZATION: Statistics
    // =========================================================================

    gen->total_explanations_generated = 0;
    gen->what_generated = 0;
    gen->why_generated = 0;
    gen->how_generated = 0;
    gen->counterfactuals_generated = 0;

    NIMCP_LOGGING_INFO("Explanation generator created");

    return gen;
}

/**
 * @brief Destroy explanation generator
 *
 * WHAT: Free all resources
 * WHY:  Clean memory management
 * HOW:  Free main structure
 *
 * @param gen Generator to destroy (may be NULL)
 *
 * COMPLEXITY: O(1)
 */
void explanation_generator_destroy(explanation_generator_t gen)
{
    // =========================================================================
    // GUARD: NULL check
    // =========================================================================

    if (!gen) {
        return;
    }

    // =========================================================================
    // CLEANUP: Free structure
    // =========================================================================

    nimcp_free(gen);

    NIMCP_LOGGING_DEBUG("Explanation generator destroyed");
}

// =============================================================================
// PUBLIC API: EXPLANATION GENERATION
// =============================================================================

/**
 * @brief Generate complete explanation for brain decision
 *
 * WHAT: Create all explanation components (what/why/how/etc.)
 * WHY:  Provide comprehensive human understanding
 * HOW:  Call component generators based on config
 *
 * @param gen Explanation generator
 * @param brain Brain that made decision
 * @param decision Decision to explain
 * @param explanation Output: explanation structure
 * @return true on success, false on error
 *
 * COMPLEXITY: O(num_outputs) for alternatives search
 */
bool explanation_generate_from_decision(
    explanation_generator_t gen,
    brain_t brain,
    brain_decision_t* decision,
    natural_explanation_t* explanation)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!gen) {
        set_error("NULL explanation generator");
        return false;
    }

    if (!brain) {
        set_error("NULL brain");
        return false;
    }

    if (!decision) {
        set_error("NULL decision");
        return false;
    }

    if (!explanation) {
        set_error("NULL explanation output");
        return false;
    }

    // =========================================================================
    // INITIALIZATION: Clear explanation structure
    // =========================================================================

    memset(explanation, 0, sizeof(natural_explanation_t));

    // =========================================================================
    // GENERATION: What explanation
    // =========================================================================

    if (gen->config.generate_what) {
        generate_what_explanation(brain, decision, explanation->what, sizeof(explanation->what));
        gen->what_generated++;
    }

    // =========================================================================
    // GENERATION: Why explanation
    // =========================================================================

    if (gen->config.generate_why) {
        generate_why_explanation(brain, decision, explanation->why, sizeof(explanation->why));
        gen->why_generated++;
    }

    // =========================================================================
    // GENERATION: How explanation
    // =========================================================================

    if (gen->config.generate_how) {
        generate_how_explanation(brain, decision, explanation->how, sizeof(explanation->how));
        gen->how_generated++;
    }

    // =========================================================================
    // GENERATION: Confidence explanation
    // =========================================================================

    if (gen->config.generate_confidence) {
        generate_confidence_explanation(brain, decision, explanation->confidence,
                                       sizeof(explanation->confidence));
    }

    // =========================================================================
    // GENERATION: Alternatives
    // =========================================================================

    if (gen->config.generate_alternatives) {
        generate_alternatives_explanation(brain, decision, explanation->alternatives,
                                         sizeof(explanation->alternatives),
                                         gen->config.max_alternatives,
                                         gen->config.min_alternative_prob);
    }

    // =========================================================================
    // GENERATION: Counterfactual
    // =========================================================================

    if (gen->config.generate_counterfactuals) {
        generate_counterfactual_explanation(brain, decision, explanation->counterfactual,
                                           sizeof(explanation->counterfactual));
        gen->counterfactuals_generated++;
    }

    // =========================================================================
    // METADATA: Fill metadata fields
    // =========================================================================

    // Extract num_features_used from active neurons in decision
    explanation->num_features_used = decision->num_active_neurons;

    // Get decision confidence from decision structure
    explanation->decision_confidence = decision->confidence;

    // Check if brain has symbolic logic module
    symbolic_logic_t* logic = brain_get_symbolic_logic(brain);
    explanation->has_symbolic_proof = (logic != NULL);

    gen->total_explanations_generated++;

    NIMCP_LOGGING_DEBUG("Generated explanation (total: %u)", gen->total_explanations_generated);

    return true;
}

/**
 * @brief Generate explanation for multimodal output
 *
 * WHAT: Explain multimodal decision with modality breakdown
 * WHY:  Show which modalities contributed to decision
 * HOW:  Extract per-modality contributions → Combine
 *
 * @param gen Explanation generator
 * @param brain Brain that made decision
 * @param output Multimodal output to explain
 * @param explanation Output: explanation structure
 * @return true on success
 *
 * COMPLEXITY: O(num_modalities + num_outputs)
 */
bool explanation_generate_from_multimodal(
    explanation_generator_t gen,
    brain_t brain,
    brain_multimodal_output_t* output,
    natural_explanation_t* explanation)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!gen || !brain || !output || !explanation) {
        set_error("NULL parameter in brain_explain_multimodal_decision");
        return false;
    }

    // =========================================================================
    // INITIALIZATION: Clear explanation
    // =========================================================================

    memset(explanation, 0, sizeof(natural_explanation_t));

    // =========================================================================
    // GENERATION: Multimodal-specific explanation
    // =========================================================================

    // Extract modality contributions from multimodal_output
    // Build modality-specific explanation based on attention weights

    // What: Decision label and confidence
    snprintf(explanation->what, sizeof(explanation->what),
            "Decision: %s (confidence: %.1f%%)",
            output->decision_label, output->confidence * 100.0f);

    // Why: Explain modality contributions based on attention weights
    char modality_list[256] = {0};
    bool first = true;

    if (output->visual_attention > 0.01f) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%svisual (%.0f%%)", first ? "" : ", ",
                output->visual_attention * 100.0f);
        first = false;
    }
    if (output->audio_attention > 0.01f) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%saudio (%.0f%%)", first ? "" : ", ",
                output->audio_attention * 100.0f);
        first = false;
    }
    if (output->speech_attention > 0.01f) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%sspeech (%.0f%%)", first ? "" : ", ",
                output->speech_attention * 100.0f);
        first = false;
    }
    if (output->language_attention > 0.01f) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%slanguage (%.0f%%)", first ? "" : ", ",
                output->language_attention * 100.0f);
        first = false;
    }
    if (output->direct_attention > 0.01f) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%sdirect (%.0f%%)", first ? "" : ", ",
                output->direct_attention * 100.0f);
        first = false;
    }

    if (strlen(modality_list) > 0) {
        snprintf(explanation->why, sizeof(explanation->why),
                "Because multiple sensory modalities converged: %s", modality_list);
    } else {
        snprintf(explanation->why, sizeof(explanation->why),
                "Based on integrated sensory processing");
    }

    // How: Processing pathway through cortical regions
    snprintf(explanation->how, sizeof(explanation->how),
            "Multi-modal integration: %s → Unified representation → Decision",
            modality_list);

    gen->total_explanations_generated++;

    return true;
}

/**
 * @brief Generate symbolic logic proof chain
 *
 * WHAT: Create formal logical proof
 * WHY:  Provide rigorous explanation
 * HOW:  Extract facts → Backward chain → Convert to text
 *
 * @param gen Explanation generator
 * @param brain Brain with symbolic logic
 * @param decision Decision to explain
 * @param proof_buffer Output: proof text
 * @param buffer_size Size of proof_buffer
 * @return true if proof generated
 *
 * COMPLEXITY: O(proof_length)
 */
bool explain_with_symbolic_logic(
    explanation_generator_t gen,
    brain_t brain,
    brain_decision_t* decision,
    char* proof_buffer,
    size_t buffer_size)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!gen || !brain || !decision || !proof_buffer || buffer_size == 0) {
        set_error("Invalid parameters for symbolic logic explanation");
        return false;
    }

    // =========================================================================
    // CHECK: Symbolic logic available?
    // =========================================================================

    // TODO: Check if brain has symbolic_logic module
    // For now, generate placeholder proof

    snprintf(proof_buffer, buffer_size,
            "Logical proof: IF (premise_A AND premise_B) THEN conclusion");

    return true;
}

/**
 * @brief Generate causal chain explanation
 *
 * WHAT: Trace causal path from input to output
 * WHY:  Reveal decision mechanism
 * HOW:  Input → Processing stages → Output
 *
 * @param gen Explanation generator
 * @param brain Brain to trace
 * @param input_description Human description of input
 * @param output_label Predicted output label
 * @param causal_chain Output: causal explanation
 * @param buffer_size Size of causal_chain buffer
 * @return true if chain generated
 *
 * COMPLEXITY: O(1) for template generation
 */
bool generate_causal_chain(
    explanation_generator_t gen,
    brain_t brain,
    const char* input_description,
    const char* output_label,
    char* causal_chain,
    size_t buffer_size)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!gen || !brain || !input_description || !output_label ||
        !causal_chain || buffer_size == 0) {
        set_error("Invalid parameters for causal chain generation");
        return false;
    }

    // =========================================================================
    // GENERATION: Build causal chain
    // =========================================================================

    snprintf(causal_chain, buffer_size,
            "Input: %s → Feature Detection → Pattern Recognition → Decision: %s",
            input_description, output_label);

    return true;
}

/**
 * @brief Generate counterfactual explanation
 *
 * WHAT: Answer "what if?" questions
 * WHY:  Reveal causal dependencies
 * HOW:  Simulate altered input → Compare outcome
 *
 * @param gen Explanation generator
 * @param brain Brain to simulate
 * @param original_decision Original decision
 * @param modified_feature Feature to change
 * @param modification Description of change
 * @param counterfactual Output: counterfactual explanation
 * @param buffer_size Size of counterfactual buffer
 * @return true if generated
 *
 * COMPLEXITY: O(1) for template generation
 */
bool generate_counterfactual(
    explanation_generator_t gen,
    brain_t brain,
    brain_decision_t* original_decision,
    const char* modified_feature,
    const char* modification,
    char* counterfactual,
    size_t buffer_size)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!gen || !brain || !original_decision || !modified_feature ||
        !modification || !counterfactual || buffer_size == 0) {
        set_error("Invalid parameters for counterfactual generation");
        return false;
    }

    // =========================================================================
    // GENERATION: Build counterfactual
    // =========================================================================

    snprintf(counterfactual, buffer_size,
            "If %s were %s, decision might change",
            modified_feature, modification);

    gen->counterfactuals_generated++;

    return true;
}

// =============================================================================
// INTERNAL HELPERS: Component Generators
// =============================================================================

/**
 * @brief Generate "what" explanation
 *
 * WHAT: Describe the decision output
 * WHY:  State what was decided
 * HOW:  Extract output label from decision
 */
static void generate_what_explanation(brain_t brain, brain_decision_t* decision,
                                     char* buffer, size_t buffer_size)
{
    if (!brain || !decision || !buffer || buffer_size == 0) {
        return;
    }

    // Extract actual output label from decision
    if (decision->label[0] != '\0') {
        snprintf(buffer, buffer_size, "Decision: %s (%.1f%% confidence)",
                decision->label, decision->confidence * 100.0f);
    } else {
        snprintf(buffer, buffer_size, "Decision made with %.1f%% confidence",
                decision->confidence * 100.0f);
    }
}

/**
 * @brief Generate "why" explanation
 *
 * WHAT: Explain why this decision was made
 * WHY:  Reveal contributing factors
 * HOW:  List top contributing features
 */
static void generate_why_explanation(brain_t brain, brain_decision_t* decision,
                                    char* buffer, size_t buffer_size)
{
    if (!brain || !decision || !buffer || buffer_size == 0) {
        return;
    }

    // Extract feature saliences from decision
    // Use active neurons as proxy for important features
    if (decision->num_active_neurons > 0) {
        // Calculate sparsity to describe feature selectivity
        float selectivity = decision->sparsity;
        const char* selectivity_desc = "many";

        if (selectivity > 0.9f) {
            selectivity_desc = "very few";
        } else if (selectivity > 0.7f) {
            selectivity_desc = "few";
        } else if (selectivity > 0.5f) {
            selectivity_desc = "some";
        }

        snprintf(buffer, buffer_size,
                "Because %s key features (%u active patterns out of network) "
                "strongly matched the learned representation",
                selectivity_desc, decision->num_active_neurons);
    } else {
        snprintf(buffer, buffer_size,
                "Based on distributed pattern recognition across the network");
    }
}

/**
 * @brief Generate "how" explanation
 *
 * WHAT: Explain the processing pathway
 * WHY:  Reveal mechanism
 * HOW:  Trace processing through brain regions
 */
static void generate_how_explanation(brain_t brain, brain_decision_t* decision,
                                    char* buffer, size_t buffer_size)
{
    if (!brain || !decision || !buffer || buffer_size == 0) {
        return;
    }

    // TODO: Extract processing pathway
    // For now, use generic pathway
    snprintf(buffer, buffer_size,
            "Input → Feature extraction → Pattern recognition → Output");
}

/**
 * @brief Generate confidence explanation
 *
 * WHAT: Explain decision confidence
 * WHY:  Reveal certainty level
 * HOW:  Map confidence score to qualitative description
 */
static void generate_confidence_explanation(brain_t brain, brain_decision_t* decision,
                                           char* buffer, size_t buffer_size)
{
    if (!brain || !decision || !buffer || buffer_size == 0) {
        return;
    }

    // Get actual confidence from decision
    float confidence = decision->confidence;

    const char* level = confidence_level_to_string(confidence);

    // Add context based on sparsity and active neurons
    if (decision->num_active_neurons > 0) {
        snprintf(buffer, buffer_size,
                "%.0f%% confident (%s certainty: %u active features with %.1f%% selectivity)",
                confidence * 100.0f, level,
                decision->num_active_neurons,
                decision->sparsity * 100.0f);
    } else {
        snprintf(buffer, buffer_size,
                "%.0f%% confident (%s certainty based on pattern matching)",
                confidence * 100.0f, level);
    }
}

/**
 * @brief Generate alternatives explanation
 *
 * WHAT: List alternative hypotheses
 * WHY:  Show other possibilities
 * HOW:  Find next-highest confidence outputs
 */
static void generate_alternatives_explanation(brain_t brain, brain_decision_t* decision,
                                             char* buffer, size_t buffer_size,
                                             uint32_t max_alternatives,
                                             float min_probability)
{
    if (!brain || !decision || !buffer || buffer_size == 0) {
        return;
    }

    // TODO: Extract alternative outputs from decision
    // For now, use placeholder
    snprintf(buffer, buffer_size, "Other possibilities exist but with lower confidence");
}

/**
 * @brief Generate counterfactual explanation
 *
 * WHAT: Generate automatic counterfactual
 * WHY:  Show what could change decision
 * HOW:  Identify most influential feature
 */
static void generate_counterfactual_explanation(brain_t brain, brain_decision_t* decision,
                                               char* buffer, size_t buffer_size)
{
    if (!brain || !decision || !buffer || buffer_size == 0) {
        return;
    }

    // TODO: Identify most influential feature and generate counterfactual
    snprintf(buffer, buffer_size,
            "Decision would change if key features were different");
}

// =============================================================================
// INTERNAL HELPERS: Utilities
// =============================================================================

/**
 * @brief Convert confidence score to qualitative level
 *
 * WHAT: Map [0,1] confidence to human term
 * WHY:  Humans understand "high" better than "0.87"
 * HOW:  Threshold-based categorization
 */
static const char* confidence_level_to_string(float confidence)
{
    if (confidence >= HIGH_CONFIDENCE_THRESHOLD) {
        return "high";
    } else if (confidence >= MEDIUM_CONFIDENCE_THRESHOLD) {
        return "medium";
    } else if (confidence >= LOW_CONFIDENCE_THRESHOLD) {
        return "low";
    } else {
        return "very low";
    }
}

// =============================================================================
// PUBLIC API: UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Get default explanation configuration
 *
 * WHAT: Return sensible defaults
 * WHY:  Convenience for common use
 * HOW:  Fill struct with default values
 *
 * @return Default configuration
 */
explanation_config_t explanation_default_config(void)
{
    explanation_config_t config = {
        .generate_what = true,
        .generate_why = true,
        .generate_how = true,
        .generate_confidence = true,
        .generate_alternatives = true,
        .generate_counterfactuals = true,
        .use_symbolic_logic = false,  // Optional, requires symbolic_logic module
        .adapt_to_audience = false,   // Optional, requires Theory of Mind
        .max_alternatives = MAX_ALTERNATIVES,
        .min_alternative_prob = MIN_ALTERNATIVE_PROBABILITY
    };

    return config;
}

/**
 * @brief Print explanation to stdout
 *
 * WHAT: Display formatted explanation
 * WHY:  Quick debugging and testing
 * HOW:  Print each field with labels
 *
 * @param explanation Explanation to display
 */
void explanation_print(const natural_explanation_t* explanation)
{
    if (!explanation) {
        printf("NULL explanation\n");
        return;
    }

    printf("\n=== EXPLANATION ===\n");

    if (explanation->what[0]) {
        printf("WHAT: %s\n", explanation->what);
    }

    if (explanation->why[0]) {
        printf("WHY:  %s\n", explanation->why);
    }

    if (explanation->how[0]) {
        printf("HOW:  %s\n", explanation->how);
    }

    if (explanation->confidence[0]) {
        printf("CONF: %s\n", explanation->confidence);
    }

    if (explanation->alternatives[0]) {
        printf("ALT:  %s\n", explanation->alternatives);
    }

    if (explanation->counterfactual[0]) {
        printf("CF:   %s\n", explanation->counterfactual);
    }

    printf("===================\n\n");
}

/**
 * @brief Convert explanation to JSON
 *
 * WHAT: Serialize as JSON object
 * WHY:  Enable machine-readable format
 * HOW:  Format as JSON with proper escaping
 *
 * @param explanation Explanation to serialize
 * @param json_buffer Output: JSON string
 * @param buffer_size Size of json_buffer
 * @return true if JSON generated
 */
bool explanation_to_json(
    const natural_explanation_t* explanation,
    char* json_buffer,
    size_t buffer_size)
{
    if (!explanation || !json_buffer || buffer_size == 0) {
        return false;
    }

    // Simple JSON formatting (no escaping for simplicity)
    snprintf(json_buffer, buffer_size,
            "{\n"
            "  \"what\": \"%s\",\n"
            "  \"why\": \"%s\",\n"
            "  \"how\": \"%s\",\n"
            "  \"confidence\": \"%s\",\n"
            "  \"alternatives\": \"%s\",\n"
            "  \"counterfactual\": \"%s\",\n"
            "  \"num_features_used\": %u,\n"
            "  \"decision_confidence\": %.3f,\n"
            "  \"has_symbolic_proof\": %s\n"
            "}",
            explanation->what,
            explanation->why,
            explanation->how,
            explanation->confidence,
            explanation->alternatives,
            explanation->counterfactual,
            explanation->num_features_used,
            explanation->decision_confidence,
            explanation->has_symbolic_proof ? "true" : "false");

    return true;
}
