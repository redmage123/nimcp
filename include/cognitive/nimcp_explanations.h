/**
 * @file nimcp_explanations.h
 * @brief Natural Language Explanations - Human-readable AI interpretability
 *
 * WHAT: Generate what-why-how explanations for brain decisions
 * WHY:  Enable human understanding of AI reasoning (interpretability)
 * HOW:  Extract decision features → Map to natural language → Build causal chains
 *
 * BIOLOGICAL BASIS:
 * - Left hemisphere language centers (Broca's, Wernicke's) generate explanations
 * - Theory of Mind adapts explanations to audience knowledge level
 * - Episodic memory provides concrete examples
 * - Prefrontal cortex constructs causal narratives
 *
 * CAPABILITIES:
 * - What-explanations: "The decision was X"
 * - Why-explanations: "Because features A, B, C were present"
 * - How-explanations: "Through pathway V1 → IT → PFC"
 * - Confidence: "87% certain based on strong feature salience"
 * - Alternatives: "Could be Y (13% probability)"
 * - Counterfactuals: "Would be Y if feature A was absent"
 *
 * PHASE: 10.7 (Natural Explanations)
 * DEPENDENCIES: Symbolic Logic (optional), Theory of Mind (for audience adaptation)
 * TRAINING_IMPACT: None (post-processing only, inference-time generation)
 *
 * @author NIMCP Development Team - Phase 10.7
 * @date 2025-11-09
 * @version 2.8.0 Phase 10.7
 */

#ifndef NIMCP_EXPLANATIONS_H
#define NIMCP_EXPLANATIONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

//=============================================================================
// Dependencies
//=============================================================================

// Include brain.h for type definitions
// Note: No circular dependency - brain.h does not include nimcp_explanations.h
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Natural Explanation Structure
//=============================================================================

/**
 * @brief Complete natural language explanation of a decision
 *
 * WHAT: Structured explanation with what/why/how/confidence/alternatives
 * WHY:  Humans need multi-faceted explanations for trust and understanding
 * HOW:  Fill each field from different explanation generation strategies
 *
 * DESIGN: Fixed-size buffers for zero-allocation explanations
 */
typedef struct natural_explanation {
    char what[256];             /**< "Classified as 'cat'" */
    char why[512];              /**< "Because whiskers + fur + ears detected" */
    char how[512];              /**< "V1 edges → IT cat pattern → PFC decision" */
    char confidence[128];       /**< "87% certain (high whisker salience)" */
    char alternatives[256];     /**< "Could be 'dog' (13%)" */
    char counterfactual[256];   /**< "Would be 'dog' if ears floppy" */

    // Metadata
    uint32_t num_features_used; /**< How many input features contributed */
    float decision_confidence;  /**< Raw confidence score [0.0, 1.0] */
    bool has_symbolic_proof;    /**< Was symbolic logic used? */
} natural_explanation_t;

/**
 * @brief Explanation generation configuration
 *
 * WHAT: Control what types of explanations to generate
 * WHY:  Different use cases need different levels of detail
 * HOW:  Flags enable/disable explanation components
 */
typedef struct {
    bool generate_what;         /**< Generate "what" description */
    bool generate_why;          /**< Generate "why" reasoning */
    bool generate_how;          /**< Generate "how" processing path */
    bool generate_confidence;   /**< Include confidence explanation */
    bool generate_alternatives; /**< List alternative hypotheses */
    bool generate_counterfactuals; /**< Generate counterfactual scenarios */
    bool use_symbolic_logic;    /**< Use symbolic logic for proof chains */
    bool adapt_to_audience;     /**< Use Theory of Mind for audience level */
    uint32_t max_alternatives;  /**< Max number of alternatives to list */
    float min_alternative_prob; /**< Min probability to include alternative */
} explanation_config_t;

/**
 * @brief Explanation generator state (opaque handle)
 *
 * WHAT: Internal state for explanation generation
 * WHY:  Maintain templates, statistics, and caches
 * HOW:  Opaque pointer pattern for encapsulation
 */
typedef struct explanation_generator_s* explanation_generator_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create explanation generator
 *
 * WHAT: Initialize explanation system with default config
 * WHY:  Setup templates and resources for explanation generation
 * HOW:  Allocate structures, load templates, initialize state
 *
 * @param config Configuration (NULL = use defaults)
 * @return Explanation generator handle, or NULL on error
 *
 * SIDE EFFECTS:
 * - Allocates memory
 * - Initializes templates
 *
 * COMPLEXITY: O(1)
 */
explanation_generator_t explanation_generator_create(const explanation_config_t* config);

/**
 * @brief Destroy explanation generator
 *
 * WHAT: Free all explanation generator resources
 * WHY:  Clean memory management
 * HOW:  Free templates, caches, and main structure
 *
 * @param gen Explanation generator to destroy
 *
 * SIDE EFFECTS:
 * - Frees memory
 * - Invalidates handle
 *
 * COMPLEXITY: O(1)
 */
void explanation_generator_destroy(explanation_generator_t gen);

/**
 * @brief Generate explanation for a brain decision
 *
 * WHAT: Create complete natural language explanation
 * WHY:  Enable human understanding of AI decision process
 * HOW:  Extract features → Analyze decision → Generate text
 *
 * @param gen Explanation generator
 * @param brain Brain that made the decision
 * @param decision The decision to explain (may be NULL for last decision)
 * @param explanation Output: generated explanation structure
 * @return true if explanation generated successfully
 *
 * ALGORITHM:
 * 1. Extract decision metadata (label, confidence, features)
 * 2. Generate "what" description from output label
 * 3. Generate "why" from top contributing features
 * 4. Generate "how" from processing pathway
 * 5. Format confidence explanation
 * 6. List alternative hypotheses
 * 7. Generate counterfactual scenarios
 *
 * COMPLEXITY: O(num_outputs) for finding alternatives
 */
bool explanation_generate_from_decision(
    explanation_generator_t gen,
    brain_t brain,
    brain_decision_t* decision,
    natural_explanation_t* explanation
);

/**
 * @brief Generate explanation from multimodal output
 *
 * WHAT: Explain multimodal decision (visual + audio + speech + direct)
 * WHY:  Multimodal decisions need modality-aware explanations
 * HOW:  Extract per-modality contributions → Combine explanations
 *
 * @param gen Explanation generator
 * @param brain Brain that made the decision
 * @param output Multimodal output to explain
 * @param explanation Output: generated explanation structure
 * @return true if explanation generated successfully
 *
 * COMPLEXITY: O(num_modalities + num_outputs)
 */
bool explanation_generate_from_multimodal(
    explanation_generator_t gen,
    brain_t brain,
    brain_multimodal_output_t* output,
    natural_explanation_t* explanation
);

/**
 * @brief Generate symbolic logic proof chain
 *
 * WHAT: Create logical proof from premises to conclusion
 * WHY:  Formal logic provides rigorous explanations
 * HOW:  Extract facts → Backward chain → Convert to text
 *
 * @param gen Explanation generator
 * @param brain Brain with symbolic logic system
 * @param decision Decision to explain
 * @param proof_buffer Output: text proof (min 512 bytes)
 * @param buffer_size Size of proof_buffer
 * @return true if proof generated successfully
 *
 * REQUIRES: brain->symbolic_logic != NULL
 *
 * EXAMPLE OUTPUT:
 * "IF (has_whiskers AND has_fur AND has_pointy_ears) THEN is_cat"
 * "Premise: has_whiskers = true (detected at V1)"
 * "Premise: has_fur = true (detected at V1)"
 * "Premise: has_pointy_ears = true (detected at V1)"
 * "Conclusion: is_cat (confidence 0.87)"
 *
 * COMPLEXITY: O(proof_length)
 */
bool explain_with_symbolic_logic(
    explanation_generator_t gen,
    brain_t brain,
    brain_decision_t* decision,
    char* proof_buffer,
    size_t buffer_size
);

/**
 * @brief Generate causal chain explanation
 *
 * WHAT: Trace causal path from input to output
 * WHY:  Causal reasoning reveals decision mechanism
 * HOW:  Input → Feature extraction → Pattern matching → Output
 *
 * @param gen Explanation generator
 * @param brain Brain to trace
 * @param input_description Human description of input (e.g., "gray cat photo")
 * @param output_label Predicted output label (e.g., "cat")
 * @param causal_chain Output: causal explanation (min 512 bytes)
 * @param buffer_size Size of causal_chain buffer
 * @return true if chain generated successfully
 *
 * EXAMPLE OUTPUT:
 * "Input: gray cat photo → Feature Detection: whiskers, fur, ears →
 *  Pattern Matching: 87% cat template match → Decision: cat"
 *
 * COMPLEXITY: O(num_features)
 */
bool generate_causal_chain(
    explanation_generator_t gen,
    brain_t brain,
    const char* input_description,
    const char* output_label,
    char* causal_chain,
    size_t buffer_size
);

/**
 * @brief Generate counterfactual explanation
 *
 * WHAT: Answer "what if?" questions about decisions
 * WHY:  Counterfactuals reveal causal dependencies
 * HOW:  Simulate altered inputs → Compare outcomes
 *
 * @param gen Explanation generator
 * @param brain Brain to simulate
 * @param original_decision Original decision to compare against
 * @param modified_feature Which feature to change (e.g., "ears")
 * @param modification Description of change (e.g., "floppy instead of pointy")
 * @param counterfactual Output: counterfactual explanation (min 256 bytes)
 * @param buffer_size Size of counterfactual buffer
 * @return true if counterfactual generated
 *
 * EXAMPLE OUTPUT:
 * "If ears were floppy (instead of pointy), decision would be 'dog' (73%)"
 *
 * COMPLEXITY: O(num_outputs) for re-evaluation
 */
bool generate_counterfactual(
    explanation_generator_t gen,
    brain_t brain,
    brain_decision_t* original_decision,
    const char* modified_feature,
    const char* modification,
    char* counterfactual,
    size_t buffer_size
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default explanation configuration
 *
 * WHAT: Return sensible default config
 * WHY:  Convenience for common use cases
 * HOW:  Return struct with reasonable defaults
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - All generation flags: true
 * - max_alternatives: 3
 * - min_alternative_prob: 0.05 (5%)
 *
 * COMPLEXITY: O(1)
 */
explanation_config_t explanation_default_config(void);

/**
 * @brief Print explanation to stdout
 *
 * WHAT: Display explanation in human-readable format
 * WHY:  Quick debugging and testing
 * HOW:  Format and print all explanation fields
 *
 * @param explanation Explanation to display
 *
 * COMPLEXITY: O(1)
 */
void explanation_print(const natural_explanation_t* explanation);

/**
 * @brief Convert explanation to JSON
 *
 * WHAT: Serialize explanation for web APIs
 * WHY:  JSON enables machine-readable explanations
 * HOW:  Format as JSON object
 *
 * @param explanation Explanation to serialize
 * @param json_buffer Output: JSON string (min 2048 bytes)
 * @param buffer_size Size of json_buffer
 * @return true if JSON generated successfully
 *
 * COMPLEXITY: O(1)
 */
bool explanation_to_json(
    const natural_explanation_t* explanation,
    char* json_buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EXPLANATIONS_H
