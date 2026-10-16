//=============================================================================
// nimcp_rule_learning.h - Inductive Rule Learning Module
//=============================================================================
/**
 * @file nimcp_rule_learning.h
 * @brief Learn logical rules from examples via inductive learning
 *
 * SOLE RESPONSIBILITY: Extract symbolic rules from labeled training examples
 *
 * WHAT: Inductive learning - generalize from specific examples to rules
 * WHY:  Enable symbolic knowledge acquisition from data
 * HOW:  Pattern extraction, confidence estimation, KB integration
 *
 * STRICT SRP:
 * - ONLY learns rules from examples (no associations, no circuits)
 * - ONLY deals with symbolic pattern extraction
 * - Delegates to KB for storage, does NOT manage KB directly
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_RULE_LEARNING_H
#define NIMCP_RULE_LEARNING_H

#include "core/brain/nimcp_brain.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Training example for rule learning
 */
typedef struct {
    const float* features;     // Input feature vector
    uint32_t num_features;     // Feature count
    const char* label;         // Ground truth label
    float weight;              // Example importance [0.0-1.0]
} rule_example_t;

/**
 * @brief Learned rule result
 */
typedef struct {
    char rule_str[512];        // Rule in symbolic format "IF X THEN Y"
    float confidence;          // Rule confidence [0.0-1.0]
    uint32_t support_count;    // Number of examples supporting rule
    uint32_t total_count;      // Total examples evaluated
} learned_rule_t;

//=============================================================================
// Public API - Rule Learning
//=============================================================================

/**
 * @brief Learn symbolic rule from labeled examples
 *
 * SOLE PURPOSE: Inductive learning - extract generalizable rules
 *
 * ALGORITHM:
 * 1. Group examples by label
 * 2. Extract common patterns within each group
 * 3. Formulate IF-THEN rules
 * 4. Compute confidence = support_count / total_count
 * 5. Add to brain's knowledge base
 *
 * EXAMPLE:
 * - Input: [(furry, meows) → "cat", (furry, barks) → "dog"]
 * - Output: "IF furry AND meows THEN cat" (confidence: 1.0)
 *
 * @param brain Brain handle
 * @param examples Training examples
 * @param labels Corresponding labels
 * @param count Number of examples
 * @return Number of rules learned, or -1 on error
 */
int brain_learn_rule_from_examples(brain_t brain, const rule_example_t* examples,
                                     const char** labels, uint32_t count);

/**
 * @brief Extract symbolic rule pattern from grouped examples
 *
 * SOLE PURPOSE: Pattern extraction from similar examples
 *
 * STRATEGY:
 * - Find common features across examples with same label
 * - Use threshold-based binarization (feature > 0.5 → TRUE)
 * - Generate conjunctive rule (AND of conditions)
 *
 * @param examples Examples with same label
 * @param count Number of examples
 * @param label Target label
 * @param rule_out Output buffer for rule string
 * @param rule_size Buffer size
 * @return true if pattern found, false otherwise
 */
bool extract_rule_pattern(const rule_example_t* examples, uint32_t count,
                          const char* label, char* rule_out, size_t rule_size);

/**
 * @brief Add learned rule to brain's knowledge base
 *
 * SOLE PURPOSE: KB integration (delegates to KB module)
 *
 * @param brain Brain handle
 * @param rule Rule string in symbolic format
 * @param confidence Rule confidence [0.0-1.0]
 * @return true on success, false on error
 */
bool add_learned_rule_to_kb(brain_t brain, const char* rule, float confidence);

/**
 * @brief Compute rule confidence from training statistics
 *
 * SOLE PURPOSE: Statistical confidence estimation
 *
 * FORMULA: confidence = (support_count + alpha) / (total_count + beta)
 * - alpha: Laplace smoothing (default: 1.0)
 * - beta: Regularization (default: 2.0)
 *
 * @param support_count Examples supporting rule
 * @param total_count Total examples evaluated
 * @return Confidence value [0.0-1.0]
 */
float compute_rule_confidence(uint32_t support_count, uint32_t total_count);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RULE_LEARNING_H */
