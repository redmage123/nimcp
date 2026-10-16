//=============================================================================
// cognitive_processor.h - Cognitive Assessment Module
//=============================================================================
/**
 * @file cognitive_processor.h
 * @brief Single Responsibility: Apply cognitive assessments to neural output
 *
 * WHAT: Evaluates neural outputs using cognitive modules (introspection, ethics, salience, curiosity, logic)
 * WHY:  Separates cognitive assessment from neural inference (SRP)
 * HOW:  Applies each cognitive module independently and aggregates results
 *
 * RESPONSIBILITIES:
 * - Compute confidence and uncertainty (introspection)
 * - Validate ethical compliance (ethics)
 * - Evaluate input salience/novelty (salience)
 * - Assess exploration value (curiosity)
 * - Apply logical reasoning (neural logic gates)
 *
 * NON-RESPONSIBILITIES (delegated to other modules):
 * - Sensory processing
 * - Multimodal integration
 * - Neural network inference
 */

#ifndef NIMCP_COGNITIVE_PROCESSOR_H
#define NIMCP_COGNITIVE_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;

//=============================================================================
// Network Output Structure (Input to Cognitive Processing)
//=============================================================================

/**
 * @brief Raw neural network output
 */
typedef struct {
    const float* output_vector;   /**< Network output activations */
    uint32_t output_size;         /**< Size of output vector */
    uint32_t spikes_generated;    /**< Total spikes during inference */
    uint64_t inference_time_us;   /**< Inference time in microseconds */
} network_output_t;

//=============================================================================
// Cognitive Annotations Structure
//=============================================================================

/**
 * @brief Cognitive assessments of neural output
 */
typedef struct {
    // Introspection (confidence/uncertainty)
    float confidence;             /**< Confidence in output (0-1) */
    float uncertainty;            /**< Epistemic uncertainty (0-1) */

    // Ethics
    bool ethical_approved;        /**< Output is ethically acceptable */

    // Salience
    float salience_score;         /**< Input importance (0-1) */
    float novelty_score;          /**< Novelty/surprise (0-1) */
    float urgency_score;          /**< Temporal urgency (0-1) */

    // Curiosity
    float exploration_bonus;      /**< Value of exploring this input */
    float information_gain;       /**< Expected information gain */

    // Logic (future enhancement)
    bool logic_valid;             /**< Logical constraints satisfied */
} cognitive_annotations_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Apply cognitive assessments to neural output
 *
 * WHAT: Single responsibility - cognitive assessment only
 * WHY:  Isolated, testable cognitive evaluation
 * HOW:  Applies each cognitive module independently
 *
 * @param brain Brain with initialized cognitive modules
 * @param net_output Raw neural network output
 * @param integrated_features Integrated multimodal features (for context)
 * @param integrated_dim Dimension of integrated features
 * @param timestamp_ms Current timestamp in milliseconds
 * @param annotations Output cognitive annotations (allocated by caller)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = output_size (dominated by cognitive module processing)
 * THREAD-SAFETY: Read-only access to brain (thread-safe if brain is read-only)
 */
bool cognitive_process_output(
    const brain_t brain,
    const network_output_t* net_output,
    const float* integrated_features,
    uint32_t integrated_dim,
    uint64_t timestamp_ms,
    cognitive_annotations_t* annotations);

/**
 * @brief Initialize cognitive annotations structure
 *
 * @param annotations Annotations structure to initialize
 */
void cognitive_annotations_init(cognitive_annotations_t* annotations);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COGNITIVE_PROCESSOR_H
