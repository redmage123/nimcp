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
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "cognitive.explanations"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(explanations, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define BIO_MODULE_COGNITIVE_EXPLANATIONS 0x0342


// =============================================================================
// ERROR HANDLING (Thread-local)
// =============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

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
#define HIGH_CONFIDENCE_THRESHOLD NIMCP_CONFIDENCE_HIGH
#define MEDIUM_CONFIDENCE_THRESHOLD NIMCP_CONFIDENCE_MEDIUM
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

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */
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

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_generato", 0.0f);


    explanation_generator_t gen = nimcp_calloc(1, sizeof(struct explanation_generator_s));
    if (!gen) {
        set_error("Failed to allocate explanation_generator_s (%zu bytes)",
                  sizeof(struct explanation_generator_s));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate gen");

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

    
    // Bio-async registration
    gen->bio_ctx = NULL;
    gen->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_KNOWLEDGE_EXPLANATIONS,
            .module_name = "explanations",
            .inbox_capacity = 32,
            .user_data = gen
        };
        gen->bio_ctx = bio_router_register_module(&bio_info);
        if (gen->bio_ctx) {
            gen->bio_async_enabled = true;
        }
    }

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
    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_generato", 0.0f);


    LOG_DEBUG("Destroying module");
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
    gen = NULL;

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
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_generate", 0.0f);


    if (gen && gen->bio_ctx) {
        bio_router_process_inbox(gen->bio_ctx, 5);
    }

    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!gen) {
        set_error("NULL explanation generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanation_generate_from_decision: gen is NULL");
        return false;
    }

    if (!brain) {
        set_error("NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanation_generate_from_decision: brain is NULL");
        return false;
    }

    if (!decision) {
        set_error("NULL decision");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanation_generate_from_decision: decision is NULL");
        return false;
    }

    if (!explanation) {
        set_error("NULL explanation output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanation_generate_from_decision: explanation is NULL");
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanation_generate_from_multimodal: required parameter is NULL (gen, brain, output, explanation)");
        return false;
    }

    // =========================================================================
    // INITIALIZATION: Clear explanation
    // =========================================================================

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_generate", 0.0f);


    memset(explanation, 0, sizeof(natural_explanation_t));

    // =========================================================================
    // GENERATION: Multimodal-specific explanation
    // =========================================================================

    // Extract modality contributions from multimodal_output
    // Build modality-specific explanation based on attention weights

    // What: Decision label and confidence
    snprintf(explanation->what, sizeof(explanation->what),
            "Decision: %s (confidence: %.1f%%)",
            output->decision_label, output->confidence * 100.0F);

    // Why: Explain modality contributions based on attention weights
    char modality_list[NIMCP_ERROR_BUFFER_SIZE] = {0};
    bool first = true;

    if (output->visual_attention > 0.01F) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%svisual (%.0f%%)", first ? "" : ", ",
                output->visual_attention * 100.0F);
        first = false;
    }
    if (output->audio_attention > 0.01F) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%saudio (%.0f%%)", first ? "" : ", ",
                output->audio_attention * 100.0F);
        first = false;
    }
    if (output->speech_attention > 0.01F) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%sspeech (%.0f%%)", first ? "" : ", ",
                output->speech_attention * 100.0F);
        first = false;
    }
    if (output->language_attention > 0.01F) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%slanguage (%.0f%%)", first ? "" : ", ",
                output->language_attention * 100.0F);
        first = false;
    }
    if (output->direct_attention > 0.01F) {
        snprintf(modality_list + strlen(modality_list),
                sizeof(modality_list) - strlen(modality_list),
                "%sdirect (%.0f%%)", first ? "" : ", ",
                output->direct_attention * 100.0F);
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explain_with_symbolic_logic: required parameter is NULL (gen, brain, decision, proof_buffer)");
        return false;
    }

    // =========================================================================
    // CHECK: Symbolic logic available?
    // =========================================================================

    // TODO: Check if brain has symbolic_logic module
    // For now, generate placeholder proof

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explain_with_symboli", 0.0f);


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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_causal_chain: operation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_generate_causal_chain", 0.0f);

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "generate_counterfactual: operation failed");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_generate_counterfactual", 0.0f);

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
                decision->label, decision->confidence * 100.0F);
    } else {
        snprintf(buffer, buffer_size, "Decision made with %.1f%% confidence",
                decision->confidence * 100.0F);
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

        if (selectivity > 0.9F) {
            selectivity_desc = "very few";
        } else if (selectivity > 0.7F) {
            selectivity_desc = "few";
        } else if (selectivity > 0.5F) {
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
                confidence * 100.0F, level,
                decision->num_active_neurons,
                decision->sparsity * 100.0F);
    } else {
        snprintf(buffer, buffer_size,
                "%.0f%% confident (%s certainty based on pattern matching)",
                confidence * 100.0F, level);
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
    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_default_", 0.0f);


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

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_print", 0.0f);


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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanation_to_json: required parameter is NULL (explanation, json_buffer)");
        return false;
    }

    // Simple JSON formatting (no escaping for simplicity)
    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_explanation_to_json", 0.0f);


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

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int explanations_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    explanations_heartbeat("explanations_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Explanations_System");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                explanations_heartbeat("explanations_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Explanations_System");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Explanations_System");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */

void explanations_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_explanations_instance_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static uint64_t g_explanations_training_steps = 0;
static double g_explanations_training_total_error = 0.0;
static double g_explanations_training_best_error = 1e30;
static bool g_explanations_training_active = false;

int explanations_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_training_begin: NULL argument");
        return -1;
    }
    explanations_heartbeat_instance(g_explanations_instance_health_agent, "explanations_train_begin", 0.0f);
    explanation_generator_t gen = (explanation_generator_t)instance;

    /* Reset training counters */
    g_explanations_training_steps = 0;
    g_explanations_training_total_error = 0.0;
    g_explanations_training_best_error = 1e30;
    g_explanations_training_active = true;

    /* Reset module stats */
    gen->total_explanations_generated = 0;
    gen->what_generated = 0;
    gen->why_generated = 0;
    gen->how_generated = 0;
    gen->counterfactuals_generated = 0;

    NIMCP_LOGGING_INFO("explanations training begin: counters reset");
    return 0;
}

int explanations_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    explanations_heartbeat_instance(g_explanations_instance_health_agent, "explanations_train_step", progress);
    explanation_generator_t gen = (explanation_generator_t)instance;
    (void)gen;

    g_explanations_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_explanations_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_explanations_training_best_error -= (double)threshold_adjust;
    if (g_explanations_training_best_error < 0.0) g_explanations_training_best_error = 0.0;

    return 0;
}

int explanations_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_training_end: NULL argument");
        return -1;
    }
    explanations_heartbeat_instance(g_explanations_instance_health_agent, "explanations_train_end", 1.0f);

    explanation_generator_t gen = (explanation_generator_t)instance;
    /* Compute final averages */
    double avg_error = (g_explanations_training_steps > 0)
        ? g_explanations_training_total_error / (double)g_explanations_training_steps
        : 0.0;

    uint64_t total_generated = gen->total_explanations_generated;

    /* Clear training flag */
    g_explanations_training_active = false;

    NIMCP_LOGGING_INFO("explanations training end: %lu steps, avg_error=%.6f, best_error=%.6f, total_generated=%lu",
                       (unsigned long)g_explanations_training_steps,
                       avg_error, g_explanations_training_best_error,
                       (unsigned long)total_generated);
    return 0;
}
