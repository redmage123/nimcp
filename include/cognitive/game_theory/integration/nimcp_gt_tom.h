//=============================================================================
// nimcp_gt_tom.h - Theory of Mind Integration for Game Theory
//=============================================================================
/**
 * @file nimcp_gt_tom.h
 * @brief Theory of Mind (ToM) for opponent modeling in strategic games
 *
 * WHAT: Bayesian opponent modeling and mental state inference
 * WHY:  Strategic reasoning requires understanding opponent beliefs/goals
 * HOW:  Observe actions, update beliefs, predict behavior, compute best response
 *
 * BIOLOGICAL INSPIRATION:
 * - Prefrontal cortex (PFC) for mental state attribution
 * - Temporoparietal junction (TPJ) for perspective-taking
 * - Mirror neuron system for action understanding
 * - Bayesian brain hypothesis for belief updates
 *
 * CAPABILITIES:
 * - Opponent preference learning from observed actions
 * - Mental state inference (beliefs, desires, intentions)
 * - Type inference (cooperative, competitive, random, etc.)
 * - Recursive reasoning ("what do they think I will do?")
 * - Best response computation given opponent beliefs
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_TOM_H
#define NIMCP_GT_TOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of tracked opponents */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_TOM_MAX_OPPONENTS 32
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_TOM_MAX_OPPONENTS 16
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_TOM_MAX_OPPONENTS 8
#else
    #define NIMCP_TOM_MAX_OPPONENTS 4
#endif

/** Maximum action history per opponent */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_TOM_MAX_HISTORY 256
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_TOM_MAX_HISTORY 128
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_TOM_MAX_HISTORY 64
#else
    #define NIMCP_TOM_MAX_HISTORY 32
#endif

/** Number of possible actions to model */
#define NIMCP_TOM_MAX_ACTIONS 16

/** Maximum prediction depth for recursive reasoning */
#define NIMCP_TOM_MAX_RECURSION_DEPTH 3

/** Number of opponent types */
#define NIMCP_TOM_NUM_OPPONENT_TYPES 6

//=============================================================================
// Opponent Type Enumeration
//=============================================================================

/**
 * @brief Opponent behavioral type classification
 *
 * WHAT: Categories of opponent behavioral strategies
 * WHY:  Different types require different counter-strategies
 * HOW:  Bayesian inference from observed action patterns
 */
typedef enum {
    NIMCP_OPPONENT_COOPERATIVE = 0,   /**< Seeks mutual benefit */
    NIMCP_OPPONENT_COMPETITIVE,       /**< Maximizes own payoff at expense of others */
    NIMCP_OPPONENT_RANDOM,            /**< Random/unpredictable actions */
    NIMCP_OPPONENT_TIT_FOR_TAT,       /**< Reciprocates previous action */
    NIMCP_OPPONENT_RATIONAL,          /**< Nash equilibrium player */
    NIMCP_OPPONENT_UNKNOWN,           /**< Insufficient data for classification */
    NIMCP_OPPONENT_TYPE_COUNT
} nimcp_opponent_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Theory of Mind context
 */
typedef struct {
    // Bayesian update parameters
    float prior_cooperative;          /**< Prior P(cooperative) (default: 0.2) */
    float prior_competitive;          /**< Prior P(competitive) (default: 0.2) */
    float prior_random;               /**< Prior P(random) (default: 0.2) */
    float prior_tit_for_tat;          /**< Prior P(tit-for-tat) (default: 0.2) */
    float prior_rational;             /**< Prior P(rational) (default: 0.2) */

    // Learning parameters
    float learning_rate;              /**< Belief update rate (default: 0.1) */
    float decay_rate;                 /**< Observation decay rate (default: 0.01) */
    float confidence_threshold;       /**< Min confidence for predictions (default: 0.3) */

    // Recursion parameters
    uint32_t max_recursion_depth;     /**< Max depth for recursive reasoning (default: 2) */
    float recursion_discount;         /**< Discount per recursion level (default: 0.7) */

    // History parameters
    uint32_t history_window;          /**< Observation window size (default: 64) */
    bool enable_forgetting;           /**< Enable old observation decay (default: true) */

    // Thread safety
    bool thread_safe;                 /**< Enable mutex protection (default: true) */
} nimcp_gt_tom_config_t;

/**
 * @brief Probability distribution over opponent types
 */
typedef struct {
    float probabilities[NIMCP_TOM_NUM_OPPONENT_TYPES]; /**< P(type) for each type */
    nimcp_opponent_type_t most_likely;                 /**< Argmax of distribution */
    float entropy;                                     /**< Uncertainty in bits */
    float confidence;                                  /**< 1 - normalized entropy */
} nimcp_type_distribution_t;

/**
 * @brief Observed action with context
 */
typedef struct {
    uint32_t action_id;               /**< Action taken */
    uint64_t timestamp_ms;            /**< When action was taken */
    float payoff_received;            /**< Payoff from this action */
    float situation_context[8];       /**< Situational features */
    uint32_t round_number;            /**< Game round */
    uint32_t my_previous_action;      /**< What I did before this */
} nimcp_observed_action_t;

/**
 * @brief Action context for observation/prediction
 */
typedef struct {
    float situation_features[8];      /**< Situational context features */
    uint32_t round_number;            /**< Current round */
    uint32_t my_intended_action;      /**< My planned action */
    float available_payoffs[NIMCP_TOM_MAX_ACTIONS]; /**< Payoff matrix row */
    uint32_t num_available_actions;   /**< Number of valid actions */
} nimcp_action_context_t;

/**
 * @brief Inferred opponent preferences
 */
typedef struct {
    float action_preferences[NIMCP_TOM_MAX_ACTIONS]; /**< Preference weight per action */
    float payoff_sensitivity;         /**< How much opponent cares about payoff */
    float fairness_sensitivity;       /**< How much opponent cares about fairness */
    float risk_aversion;              /**< Risk preference (-1=seeking, +1=averse) */
    float cooperation_tendency;       /**< Tendency to cooperate (-1=defect, +1=coop) */
    float reciprocity_strength;       /**< Strength of tit-for-tat behavior */
} nimcp_opponent_preferences_t;

/**
 * @brief Inferred mental state of opponent
 *
 * WHAT: Representation of opponent's beliefs, desires, intentions
 * WHY:  Full ToM requires modeling mental states, not just behavior
 * HOW:  Inference from action patterns and game context
 */
typedef struct {
    // Beliefs (what opponent thinks about the world)
    float believed_my_type[NIMCP_TOM_NUM_OPPONENT_TYPES]; /**< What they think I am */
    float believed_my_next_action[NIMCP_TOM_MAX_ACTIONS]; /**< What they expect me to do */

    // Desires (what opponent wants)
    float goal_own_payoff;            /**< Weight on maximizing own payoff */
    float goal_other_harm;            /**< Weight on minimizing my payoff */
    float goal_fairness;              /**< Weight on fair outcomes */
    float goal_cooperation;           /**< Weight on mutual cooperation */

    // Intentions (what opponent plans to do)
    float intended_action[NIMCP_TOM_MAX_ACTIONS]; /**< Predicted action probabilities */
    uint32_t most_likely_action;      /**< Argmax of intended_action */
    float action_confidence;          /**< Confidence in intention inference */

    // Meta-cognition
    float reasoning_sophistication;   /**< How strategic is opponent (0-1) */
    uint32_t inferred_recursion_depth; /**< How many levels they reason */
} nimcp_mental_state_t;

/**
 * @brief Beliefs about a specific opponent
 */
typedef struct {
    nimcp_player_id_t opponent_id;    /**< Opponent identifier */
    bool active;                      /**< Is this opponent being tracked */

    // Type belief
    nimcp_type_distribution_t type_distribution;

    // Preference model
    nimcp_opponent_preferences_t preferences;

    // Mental state model
    nimcp_mental_state_t mental_state;

    // Observation history
    uint32_t num_observations;        /**< Total observations */
    uint32_t recent_cooperation_count; /**< Cooperation in recent window */
    uint32_t recent_defection_count;  /**< Defection in recent window */

    // Statistics
    float avg_payoff_received;        /**< Opponent's average payoff */
    float avg_payoff_given;           /**< Avg payoff I received vs this opponent */
    float correlation_with_me;        /**< Correlation between our actions */

    // Prediction accuracy
    uint32_t predictions_made;        /**< Total predictions */
    uint32_t predictions_correct;     /**< Correct predictions */
    float prediction_accuracy;        /**< Rolling accuracy */
} nimcp_opponent_belief_t;

/**
 * @brief Action prediction result
 */
typedef struct {
    float action_probabilities[NIMCP_TOM_MAX_ACTIONS]; /**< P(action) per action */
    uint32_t most_likely_action;      /**< Predicted action */
    float confidence;                 /**< Confidence in prediction */
    float expected_payoff;            /**< Expected payoff for me given prediction */
    nimcp_opponent_type_t assumed_type; /**< Type assumed for prediction */
} nimcp_action_prediction_t;

/**
 * @brief Opaque handle to Theory of Mind context
 */
typedef struct nimcp_gt_tom_struct* nimcp_gt_tom_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default ToM configuration
 *
 * WHAT: Return sensible default configuration
 * WHY:  Easy initialization without specifying all parameters
 * HOW:  Uniform priors, moderate learning rate, recursion depth 2
 *
 * @return Default configuration struct
 */
nimcp_gt_tom_config_t nimcp_gt_tom_default_config(void);

/**
 * @brief Create Theory of Mind context
 *
 * WHAT: Allocate and initialize ToM context
 * WHY:  Central context for all opponent modeling operations
 * HOW:  nimcp_calloc with mutex initialization
 *
 * @param config Configuration (NULL for defaults)
 * @return ToM context handle or NULL on failure
 */
nimcp_gt_tom_t nimcp_gt_tom_create(const nimcp_gt_tom_config_t* config);

/**
 * @brief Destroy Theory of Mind context
 *
 * WHAT: Free all resources associated with ToM context
 * WHY:  Proper cleanup to avoid memory leaks
 * HOW:  Free all opponent records, history, mutex, then context
 *
 * @param ctx ToM context to destroy (NULL is safe)
 */
void nimcp_gt_tom_destroy(nimcp_gt_tom_t ctx);

//=============================================================================
// Observation Functions
//=============================================================================

/**
 * @brief Record observed opponent action
 *
 * WHAT: Add observation to opponent's action history
 * WHY:  Learning requires observing opponent behavior
 * HOW:  Store action in history buffer, update running statistics
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param action Action taken by opponent
 * @param context Situational context of action
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_observe_action(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    uint32_t action,
    const nimcp_action_context_t* context
);

/**
 * @brief Update beliefs about opponent using Bayesian inference
 *
 * WHAT: Recompute type distribution and mental state from observations
 * WHY:  Beliefs should reflect accumulated evidence
 * HOW:  Bayes rule: P(type|actions) = P(actions|type) * P(type) / P(actions)
 *
 * @param ctx ToM context
 * @param opponent_id Opponent to update beliefs about
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_update_beliefs(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
);

//=============================================================================
// Prediction Functions
//=============================================================================

/**
 * @brief Predict opponent's next action
 *
 * WHAT: Generate probability distribution over opponent actions
 * WHY:  Strategic planning requires anticipating opponent behavior
 * HOW:  Weight predictions by type probabilities
 *
 * @param ctx ToM context
 * @param opponent_id Opponent to predict
 * @param situation Current situation context
 * @param prediction Output prediction
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_predict_action(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    const nimcp_action_context_t* situation,
    nimcp_action_prediction_t* prediction
);

/**
 * @brief Get type probability distribution for opponent
 *
 * WHAT: Retrieve current type beliefs
 * WHY:  May want type distribution for other reasoning
 * HOW:  Copy from internal opponent belief structure
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param dist Output type distribution
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_get_type_distribution(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_type_distribution_t* dist
);

//=============================================================================
// Inference Functions
//=============================================================================

/**
 * @brief Infer opponent preferences from behavior
 *
 * WHAT: Estimate what opponent values (payoff, fairness, risk, etc.)
 * WHY:  Preferences explain and predict behavior patterns
 * HOW:  Regression from action choices to utility parameters
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param preferences Output preferences
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_infer_preferences(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_opponent_preferences_t* preferences
);

/**
 * @brief Infer opponent mental state (beliefs, desires, intentions)
 *
 * WHAT: Full ToM inference of opponent's mind
 * WHY:  Deep understanding for sophisticated strategic reasoning
 * HOW:  Combine preference inference with context modeling
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param state Output mental state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_infer_mental_state(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_mental_state_t* state
);

//=============================================================================
// Recursive Reasoning Functions
//=============================================================================

/**
 * @brief Infer what opponent thinks I will do
 *
 * WHAT: Level-2 reasoning - opponent's prediction of my action
 * WHY:  Need to model their model of me for strategic advantage
 * HOW:  Simulate opponent's reasoning about me
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param my_prediction Output: what they predict I'll do
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_what_do_they_think_i_will_do(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_action_prediction_t* my_prediction
);

/**
 * @brief Compute best response to opponent's expected behavior
 *
 * WHAT: Optimal action given beliefs about opponent
 * WHY:  Core purpose of opponent modeling is to play better
 * HOW:  Expected payoff maximization over opponent action distribution
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param my_action Output: best response action
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_best_response_to_beliefs(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    uint32_t* my_action
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Reset beliefs about specific opponent
 *
 * WHAT: Clear all learned information about opponent
 * WHY:  Opponent may have changed strategy, or want fresh start
 * HOW:  Reset to prior distribution, clear history
 *
 * @param ctx ToM context
 * @param opponent_id Opponent to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_reset_opponent(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
);

/**
 * @brief Get confidence in opponent type inference
 *
 * WHAT: How certain we are about opponent's type
 * WHY:  Low confidence means predictions are unreliable
 * HOW:  Based on type distribution entropy and observation count
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @return Confidence in [0, 1], or -1.0 on error
 */
float nimcp_gt_tom_get_confidence(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
);

/**
 * @brief Get opponent belief structure (read-only)
 *
 * WHAT: Access complete opponent model
 * WHY:  For debugging, visualization, or external analysis
 * HOW:  Copy internal belief structure to output
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @param belief Output belief structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_gt_tom_get_opponent_belief(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id,
    nimcp_opponent_belief_t* belief
);

/**
 * @brief Get number of tracked opponents
 *
 * @param ctx ToM context
 * @return Number of opponents being tracked, or 0 on error
 */
uint32_t nimcp_gt_tom_get_opponent_count(nimcp_gt_tom_t ctx);

/**
 * @brief Check if opponent is being tracked
 *
 * @param ctx ToM context
 * @param opponent_id Opponent identifier
 * @return true if tracked, false otherwise
 */
bool nimcp_gt_tom_is_opponent_tracked(
    nimcp_gt_tom_t ctx,
    nimcp_player_id_t opponent_id
);

//=============================================================================
// Type Name Utilities
//=============================================================================

/**
 * @brief Get human-readable name for opponent type
 *
 * @param type Opponent type
 * @return Type name string
 */
const char* nimcp_opponent_type_name(nimcp_opponent_type_t type);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_TOM_H
