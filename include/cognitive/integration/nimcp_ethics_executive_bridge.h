/**
 * @file nimcp_ethics_executive_bridge.h
 * @brief Ethics-Executive Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Bidirectional integration between ethics and executive systems
 * WHY:  Ethics constrains executive action selection; executive functions
 *       evaluate and implement ethical constraints.
 * HOW:  Ethics provides constraints and vetoes; executive evaluates actions
 *       against ethical standards before execution.
 *
 * BIOLOGICAL BASIS:
 * - Ventromedial prefrontal cortex integrates emotional/ethical valuation
 * - Dorsolateral PFC implements executive control over behavior
 * - ACC monitors for conflict between goals and ethical constraints
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ETHICS_EXECUTIVE_BRIDGE_H
#define NIMCP_ETHICS_EXECUTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ETHICS_EXECUTIVE_MAX_CONSTRAINTS  32
#define ETHICS_EXECUTIVE_MAX_ACTIONS      128
#define ETHICS_EXECUTIVE_SCORE_MIN        0.0f
#define ETHICS_EXECUTIVE_SCORE_MAX        1.0f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct ethics_executive_bridge ethics_executive_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Ethical constraint on an action
 */
typedef struct {
    uint64_t constraint_id;              /**< Unique constraint identifier */
    char description[128];               /**< Human-readable description */
    float severity;                      /**< Severity of violation [0, 1] */
    bool is_hard_constraint;             /**< Hard (veto) vs soft constraint */
} ethics_constraint_t;

/**
 * @brief Ethical constraints output
 */
typedef struct {
    ethics_constraint_t constraints[ETHICS_EXECUTIVE_MAX_CONSTRAINTS];
    size_t constraint_count;             /**< Number of constraints */
    bool action_permitted;               /**< Whether action is permitted */
    float overall_ethical_score;         /**< Combined ethical score [0, 1] */
} ethics_constraints_out_t;

/**
 * @brief Action evaluation result
 */
typedef struct {
    uint64_t action_id;                  /**< Action being evaluated */
    float ethical_score;                 /**< Ethical score [0, 1] */
    bool is_permitted;                   /**< Whether action is permitted */
    bool was_vetoed;                     /**< Whether action was vetoed */
    size_t violated_constraint_count;    /**< Number of violated constraints */
} ethics_action_evaluation_t;

/**
 * @brief Configuration for Ethics-Executive bridge
 */
typedef struct {
    float ethical_threshold;             /**< Min score for action permission */
    bool veto_enabled;                   /**< Enable ethical veto capability */
    float constraint_strictness;         /**< How strictly to apply constraints */
} ethics_executive_config_t;

/**
 * @brief Statistics for Ethics-Executive bridge
 */
typedef struct {
    uint64_t actions_constrained;        /**< Actions with constraints applied */
    uint64_t actions_vetoed;             /**< Actions vetoed on ethical grounds */
    uint64_t evaluations_performed;      /**< Total ethical evaluations */
    float avg_ethical_score;             /**< Average ethical score */
    float veto_rate;                     /**< Percentage of actions vetoed */
} ethics_executive_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Ethics-Executive configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced ethical strictness
 * HOW:  Set threshold at 0.5, enable veto, moderate strictness
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int ethics_executive_bridge_default_config(ethics_executive_config_t* config);

/**
 * @brief Create Ethics-Executive bridge
 *
 * WHAT: Initialize Ethics-Executive integration bridge
 * WHY:  Enable ethical constraints on executive actions
 * HOW:  Allocate bridge, initialize constraint storage
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
ethics_executive_bridge_t* ethics_executive_bridge_create(
    const ethics_executive_config_t* config
);

/**
 * @brief Destroy Ethics-Executive bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free constraint storage, clear state
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void ethics_executive_bridge_destroy(ethics_executive_bridge_t* bridge);

/* ============================================================================
 * Ethics -> Executive Direction
 * ============================================================================ */

/**
 * @brief Get ethical constraints for an action
 *
 * WHAT: Retrieve ethical constraints applicable to an action
 * WHY:  Executive needs to know constraints before acting
 * HOW:  Evaluate action against ethical rules, return constraints
 *
 * @param bridge Ethics-Executive bridge
 * @param action_id Action to constrain
 * @param constraints_out Output constraints
 * @return 0 on success, -1 on error
 */
int ethics_executive_constrain_action(
    ethics_executive_bridge_t* bridge,
    uint64_t action_id,
    ethics_constraints_out_t* constraints_out
);

/**
 * @brief Veto an unethical action
 *
 * WHAT: Prevent execution of an unethical action
 * WHY:  Hard ethical constraints must block certain actions
 * HOW:  Mark action as vetoed, notify executive system
 *
 * @param bridge Ethics-Executive bridge
 * @param action_id Action to veto
 * @return 0 on success (action vetoed), -1 on error
 */
int ethics_executive_veto_action(
    ethics_executive_bridge_t* bridge,
    uint64_t action_id
);

/* ============================================================================
 * Executive -> Ethics Direction
 * ============================================================================ */

/**
 * @brief Evaluate action ethically
 *
 * WHAT: Get ethical evaluation of a proposed action
 * WHY:  Executive queries ethics before deciding
 * HOW:  Score action against ethical framework
 *
 * @param bridge Ethics-Executive bridge
 * @param action_id Action to evaluate
 * @param ethical_score_out Output ethical score [0, 1]
 * @return 0 on success, -1 on error
 */
int ethics_executive_evaluate_action(
    ethics_executive_bridge_t* bridge,
    uint64_t action_id,
    float* ethical_score_out
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get ethically permitted actions
 *
 * WHAT: Query actions that pass ethical evaluation
 * WHY:  Executive can choose only from permitted actions
 * HOW:  Filter actions by ethical threshold
 *
 * @param bridge Ethics-Executive bridge
 * @param actions Output array of permitted action IDs
 * @param max_count Maximum number of results
 * @return Number of permitted actions, -1 on error
 */
int ethics_executive_get_permitted_actions(
    ethics_executive_bridge_t* bridge,
    uint64_t actions[],
    size_t max_count
);

/* ============================================================================
 * Stats API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Ethics-Executive bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int ethics_executive_bridge_get_stats(
    const ethics_executive_bridge_t* bridge,
    ethics_executive_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ETHICS_EXECUTIVE_BRIDGE_H */
