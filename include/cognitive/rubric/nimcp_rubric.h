/**
 * @file nimcp_rubric.h
 * @brief Cognitive Output Rubric — Human-Style Quality Evaluation
 *
 * Two-tier evaluation of brain_decide() output quality:
 *   Tier 1 (Structural): internal consistency, confidence calibration,
 *          completeness, reasoning chain, epistemic quality, ethical alignment
 *   Tier 2 (Qualitative): originality, integration depth, communication clarity,
 *          engagement quality, empathetic accuracy, information density
 *
 * Grade scale: A+ (≥0.93) through F (<0.50)
 */

#ifndef NIMCP_RUBRIC_H
#define NIMCP_RUBRIC_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations — avoid pulling in full headers */
typedef struct brain_struct* brain_t;
typedef struct brain_decision brain_decision_t;

/*=============================================================================
 * Configuration
 *============================================================================*/

typedef struct {
    float tier1_weight;           /**< Weight for structural tier (default 0.5) */
    float tier2_weight;           /**< Weight for qualitative tier (default 0.5) */
    bool  skip_missing_subsystems; /**< Score 0.5 for missing subsystems (default true) */
} rubric_config_t;

/*=============================================================================
 * Result Structures
 *============================================================================*/

typedef struct {
    float internal_consistency;      /**< [0-1] Output vector self-agreement */
    float confidence_calibration;    /**< [0-1] Confidence vs running accuracy */
    float completeness;              /**< [0-1] Non-zero output coverage */
    float reasoning_chain_quality;   /**< [0-1] Explanation depth & structure */
    float epistemic_quality;         /**< [0-1] Evidence quality (epistemic filter) */
    float ethical_alignment;         /**< [0-1] Golden Rule alignment (ethics engine) */
    float tier1_score;               /**< Weighted aggregate of above */
} rubric_tier1_t;

typedef struct {
    float originality;               /**< [0-1] Creative novelty (aesthetic evaluator) */
    float integration_depth;         /**< [0-1] Cross-system breadth */
    float communication_clarity;     /**< [0-1] Label specificity + readability */
    float engagement_quality;        /**< [0-1] Berlyne hedonic+arousal (aesthetic) */
    float empathetic_accuracy;       /**< [0-1] Mirror neuron match quality */
    float information_density;       /**< [0-1] Normalized Shannon entropy */
    float tier2_score;               /**< Weighted aggregate of above */
} rubric_tier2_t;

/** Bitmask for which subsystems were available during evaluation */
#define RUBRIC_HAS_EPISTEMIC    (1u << 0)
#define RUBRIC_HAS_ETHICS       (1u << 1)
#define RUBRIC_HAS_AESTHETIC    (1u << 2)
#define RUBRIC_HAS_MIRROR       (1u << 3)

typedef struct {
    rubric_tier1_t tier1;
    rubric_tier2_t tier2;
    float    overall_score;          /**< [0-1] Weighted combination */
    char     grade;                  /**< A/B/C/D/F */
    char     grade_modifier;         /**< +/-/space */
    uint32_t subsystems_available;   /**< Bitmask of available subsystems */
    uint64_t evaluation_time_us;     /**< Wall-clock evaluation time */
} rubric_result_t;

/*=============================================================================
 * Evaluator Lifecycle
 *============================================================================*/

typedef struct rubric_evaluator rubric_evaluator_t;

/**
 * @brief Create rubric evaluator with given configuration
 * @param config Configuration (NULL for defaults)
 * @return Evaluator or NULL on allocation failure
 */
rubric_evaluator_t* rubric_evaluator_create(const rubric_config_t* config);

/**
 * @brief Destroy rubric evaluator and free resources
 */
void rubric_evaluator_destroy(rubric_evaluator_t* eval);

/*=============================================================================
 * Evaluation
 *============================================================================*/

/**
 * @brief Evaluate a brain decision using the two-tier rubric
 *
 * @param eval      Rubric evaluator (created via rubric_evaluator_create)
 * @param brain     Brain that produced the decision
 * @param decision  Decision to evaluate (from brain_decide)
 * @param result    Output: rubric scores and grade
 * @return 0 on success, -1 on error
 */
int rubric_evaluate_decision(rubric_evaluator_t* eval,
                             brain_t brain,
                             const brain_decision_t* decision,
                             rubric_result_t* result);

/**
 * @brief Fill rubric_config_t with default values
 */
void rubric_config_defaults(rubric_config_t* config);

#endif /* NIMCP_RUBRIC_H */
