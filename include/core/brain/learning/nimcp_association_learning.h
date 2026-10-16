//=============================================================================
// nimcp_association_learning.h - Association Learning Module
//=============================================================================
/**
 * @file nimcp_association_learning.h
 * @brief Learn A→B implications from co-occurrence statistics
 *
 * SOLE RESPONSIBILITY: Associative learning via statistical correlation
 *
 * WHAT: Learn probabilistic associations between concepts
 * WHY:  Capture implicit relationships without explicit rules
 * HOW:  Track co-occurrence, compute correlation, update strengths
 *
 * STRICT SRP:
 * - ONLY learns associations (no rules, no circuits)
 * - ONLY deals with A→B implications
 * - Does NOT perform reasoning (delegates to inference module)
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_ASSOCIATION_LEARNING_H
#define NIMCP_ASSOCIATION_LEARNING_H

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
 * @brief Association statistics
 */
typedef struct {
    uint32_t AB_count;         // Times A and B co-occurred
    uint32_t A_count;          // Times A occurred alone
    uint32_t B_count;          // Times B occurred alone
    uint32_t total_count;      // Total observations
} association_stats_t;

/**
 * @brief Learned association
 */
typedef struct {
    char antecedent[256];      // A in "A→B"
    char consequent[256];      // B in "A→B"
    float strength;            // Association strength [0.0-1.0]
    float confidence;          // P(B|A)
} association_t;

//=============================================================================
// Public API - Association Learning
//=============================================================================

/**
 * @brief Learn association between concepts A and B
 *
 * SOLE PURPOSE: Update association strength from co-occurrence
 *
 * ALGORITHM:
 * 1. Increment co-occurrence counter
 * 2. Compute P(B|A) = count(A,B) / count(A)
 * 3. Update association strength in brain
 *
 * USE CASE: Concept co-activation (e.g., "rain" → "umbrella")
 *
 * @param brain Brain handle
 * @param A Antecedent concept
 * @param B Consequent concept
 * @param cooccurrence_count Number of times A and B occurred together
 * @return true on success, false on error
 */
bool brain_learn_association(brain_t brain, const char* A, const char* B,
                              uint32_t cooccurrence_count);

/**
 * @brief Compute association confidence P(B|A)
 *
 * SOLE PURPOSE: Statistical confidence estimation
 *
 * FORMULA: confidence = count(A,B) / count(A)
 * - Conditional probability: "Given A, how often does B occur?"
 *
 * @param A Antecedent concept
 * @param B Consequent concept
 * @param stats Co-occurrence statistics
 * @return Confidence value [0.0-1.0]
 */
float compute_association_confidence(const char* A, const char* B,
                                     const association_stats_t* stats);

/**
 * @brief Update association strength based on outcome
 *
 * SOLE PURPOSE: Reinforcement-based strength adjustment
 *
 * ALGORITHM:
 * - Positive outcome: strength += learning_rate * (1.0 - strength)
 * - Negative outcome: strength -= learning_rate * strength
 * - Decay over time: strength *= decay_factor
 *
 * @param brain Brain handle
 * @param A Antecedent concept
 * @param B Consequent concept
 * @param outcome Reinforcement signal [-1.0, 1.0]
 * @return Updated strength value
 */
float update_association_strength(brain_t brain, const char* A, const char* B,
                                   float outcome);

/**
 * @brief Get association strength between A and B
 *
 * SOLE PURPOSE: Query learned association
 *
 * @param brain Brain handle
 * @param A Antecedent concept
 * @param B Consequent concept
 * @return Association strength [0.0-1.0], or -1.0 if not found
 */
float get_association_strength(brain_t brain, const char* A, const char* B);

/**
 * @brief Decay all association strengths (temporal forgetting)
 *
 * SOLE PURPOSE: Implement forgetting curve
 *
 * ALGORITHM: strength *= decay_factor (typically 0.95-0.99)
 *
 * @param brain Brain handle
 * @param decay_factor Decay multiplier [0.0-1.0]
 * @return Number of associations decayed
 */
uint32_t decay_all_associations(brain_t brain, float decay_factor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASSOCIATION_LEARNING_H */
