/**
 * @file nimcp_curiosity_reasoning_bridge.h
 * @brief Curiosity-Reasoning Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Bidirectional integration between curiosity and reasoning systems
 * WHY:  Curiosity drives exploration of reasoning space; novel conclusions
 *       generate curiosity; epistemic uncertainty guides inquiry.
 * HOW:  Curiosity level biases reasoning exploration; novel conclusions
 *       trigger curiosity signals; uncertainty is shared bidirectionally.
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic signals from VTA/SNc drive curiosity and exploration
 * - Prefrontal cortex integrates uncertainty with reasoning processes
 * - Information-seeking behavior optimizes epistemic value
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CURIOSITY_REASONING_BRIDGE_H
#define NIMCP_CURIOSITY_REASONING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CURIOSITY_REASONING_MAX_TOPICS    256
#define CURIOSITY_REASONING_MIN_LEVEL     0.0f
#define CURIOSITY_REASONING_MAX_LEVEL     1.0f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct curiosity_reasoning_bridge curiosity_reasoning_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Reasoning context for curiosity-driven exploration
 */
typedef struct {
    uint64_t context_id;                 /**< Unique context identifier */
    float uncertainty;                   /**< Current epistemic uncertainty */
    float novelty;                       /**< Novelty of current exploration */
    uint64_t depth;                      /**< Reasoning depth */
} curiosity_reasoning_context_t;

/**
 * @brief Configuration for Curiosity-Reasoning bridge
 */
typedef struct {
    float exploration_bias;              /**< Bias toward exploration vs exploitation */
    float novelty_threshold;             /**< Novelty threshold for curiosity trigger */
    float uncertainty_weight;            /**< Weight of uncertainty in exploration */
} curiosity_reasoning_config_t;

/**
 * @brief Statistics for Curiosity-Reasoning bridge
 */
typedef struct {
    uint64_t explorations_driven;        /**< Explorations driven by curiosity */
    uint64_t novel_conclusions;          /**< Novel conclusions detected */
    uint64_t uncertainty_shared;         /**< Uncertainty sharing events */
    float avg_curiosity_level;           /**< Average curiosity level */
    float avg_novelty_score;             /**< Average novelty of conclusions */
} curiosity_reasoning_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Curiosity-Reasoning configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with exploration-exploitation balance
 * HOW:  Set balanced exploration bias, standard thresholds
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int curiosity_reasoning_bridge_default_config(curiosity_reasoning_config_t* config);

/**
 * @brief Create Curiosity-Reasoning bridge
 *
 * WHAT: Initialize Curiosity-Reasoning integration bridge
 * WHY:  Enable curiosity-driven reasoning exploration
 * HOW:  Allocate bridge, initialize topic tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
curiosity_reasoning_bridge_t* curiosity_reasoning_bridge_create(
    const curiosity_reasoning_config_t* config
);

/**
 * @brief Destroy Curiosity-Reasoning bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free topic storage, clear state
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void curiosity_reasoning_bridge_destroy(curiosity_reasoning_bridge_t* bridge);

/* ============================================================================
 * Curiosity -> Reasoning Direction
 * ============================================================================ */

/**
 * @brief Drive reasoning exploration based on curiosity
 *
 * WHAT: Use curiosity level to guide reasoning exploration
 * WHY:  High curiosity promotes exploration of novel paths
 * HOW:  Bias reasoning toward unexplored or uncertain areas
 *
 * @param bridge Curiosity-Reasoning bridge
 * @param context Current reasoning context
 * @param curiosity_level Current curiosity level [0, 1]
 * @return 0 on success, -1 on error
 */
int curiosity_reasoning_drive_exploration(
    curiosity_reasoning_bridge_t* bridge,
    const curiosity_reasoning_context_t* context,
    float curiosity_level
);

/**
 * @brief Share epistemic uncertainty with reasoning
 *
 * WHAT: Communicate uncertainty about a topic to reasoning system
 * WHY:  Uncertainty guides inquiry and hypothesis generation
 * HOW:  Update topic uncertainty, influence reasoning priorities
 *
 * @param bridge Curiosity-Reasoning bridge
 * @param topic_id Topic identifier
 * @param uncertainty_level Epistemic uncertainty [0, 1]
 * @return 0 on success, -1 on error
 */
int curiosity_reasoning_share_uncertainty(
    curiosity_reasoning_bridge_t* bridge,
    uint64_t topic_id,
    float uncertainty_level
);

/* ============================================================================
 * Reasoning -> Curiosity Direction
 * ============================================================================ */

/**
 * @brief Signal novel conclusion to curiosity system
 *
 * WHAT: Notify curiosity system of a novel reasoning conclusion
 * WHY:  Novel conclusions can trigger further curiosity
 * HOW:  Evaluate novelty, potentially boost curiosity
 *
 * @param bridge Curiosity-Reasoning bridge
 * @param conclusion_id Conclusion identifier
 * @param novelty_score Novelty of the conclusion [0, 1]
 * @return 0 on success, -1 on error
 */
int curiosity_reasoning_on_novel_conclusion(
    curiosity_reasoning_bridge_t* bridge,
    uint64_t conclusion_id,
    float novelty_score
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get exploration priority for a topic
 *
 * WHAT: Query curiosity-driven priority for exploring a topic
 * WHY:  Prioritize reasoning about high-curiosity topics
 * HOW:  Combine curiosity, novelty, and uncertainty factors
 *
 * @param bridge Curiosity-Reasoning bridge
 * @param topic_id Topic identifier
 * @return Exploration priority [0, 1], -1.0f on error
 */
float curiosity_reasoning_get_exploration_priority(
    curiosity_reasoning_bridge_t* bridge,
    uint64_t topic_id
);

/* ============================================================================
 * Stats API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Curiosity-Reasoning bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int curiosity_reasoning_bridge_get_stats(
    const curiosity_reasoning_bridge_t* bridge,
    curiosity_reasoning_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_REASONING_BRIDGE_H */
