//=============================================================================
// nimcp_gt_learning.h - Strategic Learning for Repeated Games
//=============================================================================
/**
 * @file nimcp_gt_learning.h
 * @brief Strategic learning algorithms for game-theoretic interactions
 *
 * WHAT: Q-learning, regret minimization, fictitious play, opponent modeling
 * WHY:  Enable agents to learn optimal strategies through experience
 * HOW:  Reinforcement learning, counterfactual regret, belief updates
 *
 * BIOLOGICAL INSPIRATION:
 * - Dopaminergic reward prediction error (Q-learning)
 * - Orbitofrontal regret signals (CFR)
 * - Prefrontal opponent modeling (theory of mind)
 * - Hippocampal experience replay
 *
 * INTEGRATION: Hemispheric Brain (strategic coordination learning)
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_LEARNING_H
#define NIMCP_GT_LEARNING_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Codes (Using GT error range 25000-25999)
//=============================================================================

#define NIMCP_GT_ERROR_LEARNING_BASE        (NIMCP_GT_ERROR_BASE + 100)
#define NIMCP_GT_ERROR_INVALID_STATE_IDX    (NIMCP_GT_ERROR_LEARNING_BASE + 1)
#define NIMCP_GT_ERROR_INVALID_ACTION_IDX   (NIMCP_GT_ERROR_LEARNING_BASE + 2)
#define NIMCP_GT_ERROR_INVALID_INFO_SET     (NIMCP_GT_ERROR_LEARNING_BASE + 3)
#define NIMCP_GT_ERROR_EMPTY_HISTORY        (NIMCP_GT_ERROR_LEARNING_BASE + 4)
#define NIMCP_GT_ERROR_LEARNING_DIVERGED    (NIMCP_GT_ERROR_LEARNING_BASE + 5)

//=============================================================================
// Bio-Async Module IDs
//=============================================================================

#define BIO_MODULE_GT_LEARNING              (BIO_MODULE_GAME_THEORY_BASE + 10)
#define BIO_MODULE_GT_Q_LEARNING            (BIO_MODULE_GAME_THEORY_BASE + 11)
#define BIO_MODULE_GT_CFR                   (BIO_MODULE_GAME_THEORY_BASE + 12)
#define BIO_MODULE_GT_FICTITIOUS            (BIO_MODULE_GAME_THEORY_BASE + 13)
#define BIO_MODULE_GT_OPPONENT_MODEL        (BIO_MODULE_GAME_THEORY_BASE + 14)

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum states for Q-table (tier-scaled) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_GT_MAX_STATES 4096
    #define NIMCP_GT_MAX_ACTIONS 64
    #define NIMCP_GT_MAX_INFO_SETS 2048
    #define NIMCP_GT_MAX_HISTORY 1024
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_GT_MAX_STATES 1024
    #define NIMCP_GT_MAX_ACTIONS 32
    #define NIMCP_GT_MAX_INFO_SETS 512
    #define NIMCP_GT_MAX_HISTORY 256
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_GT_MAX_STATES 256
    #define NIMCP_GT_MAX_ACTIONS 16
    #define NIMCP_GT_MAX_INFO_SETS 128
    #define NIMCP_GT_MAX_HISTORY 64
#else
    #define NIMCP_GT_MAX_STATES 64
    #define NIMCP_GT_MAX_ACTIONS 8
    #define NIMCP_GT_MAX_INFO_SETS 32
    #define NIMCP_GT_MAX_HISTORY 16
#endif

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Learning algorithm type
 */
typedef enum {
    NIMCP_GT_LEARN_Q_LEARNING,        /**< Q-learning (off-policy TD) */
    NIMCP_GT_LEARN_SARSA,             /**< SARSA (on-policy TD) */
    NIMCP_GT_LEARN_CFR,               /**< Counterfactual regret minimization */
    NIMCP_GT_LEARN_FICTITIOUS_PLAY,   /**< Fictitious play (belief-based) */
    NIMCP_GT_LEARN_EXP3,              /**< EXP3 (adversarial bandits) */
    NIMCP_GT_LEARN_COUNT
} nimcp_gt_learn_method_t;

/**
 * @brief Exploration strategy
 */
typedef enum {
    NIMCP_GT_EXPLORE_EPSILON_GREEDY,  /**< Epsilon-greedy exploration */
    NIMCP_GT_EXPLORE_BOLTZMANN,       /**< Softmax/Boltzmann exploration */
    NIMCP_GT_EXPLORE_UCB,             /**< Upper confidence bound */
    NIMCP_GT_EXPLORE_THOMPSON,        /**< Thompson sampling */
    NIMCP_GT_EXPLORE_COUNT
} nimcp_gt_explore_strategy_t;

/**
 * @brief Learning rate schedule
 */
typedef enum {
    NIMCP_GT_SCHEDULE_CONSTANT,       /**< Fixed learning rate */
    NIMCP_GT_SCHEDULE_DECAY,          /**< Exponential decay */
    NIMCP_GT_SCHEDULE_POLYNOMIAL,     /**< Polynomial decay (1/(1+t)) */
    NIMCP_GT_SCHEDULE_ADAPTIVE,       /**< Adaptive based on variance */
    NIMCP_GT_SCHEDULE_COUNT
} nimcp_gt_lr_schedule_t;

/**
 * @brief Opponent type (for type inference)
 */
typedef enum {
    NIMCP_GT_OPPONENT_UNKNOWN,        /**< Unknown type */
    NIMCP_GT_OPPONENT_RANDOM,         /**< Random/uniform player */
    NIMCP_GT_OPPONENT_COOPERATIVE,    /**< Cooperative player */
    NIMCP_GT_OPPONENT_COMPETITIVE,    /**< Competitive/adversarial */
    NIMCP_GT_OPPONENT_TFTIT,          /**< Tit-for-tat */
    NIMCP_GT_OPPONENT_ADAPTIVE,       /**< Adaptive/learning player */
    NIMCP_GT_OPPONENT_COUNT
} nimcp_gt_opponent_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Learning configuration
 */
typedef struct {
    nimcp_gt_learn_method_t method;       /**< Learning algorithm */
    nimcp_gt_explore_strategy_t explore;  /**< Exploration strategy */
    nimcp_gt_lr_schedule_t lr_schedule;   /**< Learning rate schedule */

    /* Learning parameters */
    float learning_rate;              /**< Initial learning rate (alpha) */
    float discount_factor;            /**< Discount factor (gamma) */
    float exploration_rate;           /**< Initial exploration (epsilon) */
    float exploration_min;            /**< Minimum exploration rate */
    float exploration_decay;          /**< Exploration decay rate */
    float temperature;                /**< Boltzmann temperature */

    /* Schedule parameters */
    float lr_decay_rate;              /**< Learning rate decay */
    float lr_min;                     /**< Minimum learning rate */

    /* CFR parameters */
    bool use_linear_cfr;              /**< Use linear CFR weighting */
    bool use_cfr_plus;                /**< Use CFR+ (no negative regret) */
    float regret_matching_epsilon;    /**< Regret matching threshold */

    /* EXP3 parameters */
    float exp3_gamma;                 /**< EXP3 exploration parameter */

    /* Opponent modeling */
    bool enable_opponent_modeling;    /**< Enable opponent type inference */
    uint32_t model_window_size;       /**< History window for modeling */
    float type_prior_strength;        /**< Strength of type priors */

    /* Dimensions */
    uint32_t num_states;              /**< Number of states */
    uint32_t num_actions;             /**< Number of actions per state */
} nimcp_gt_learning_config_t;

/**
 * @brief Q-table entry (state-action value)
 */
typedef struct {
    float value;                      /**< Q(s, a) value */
    uint32_t visit_count;             /**< Number of updates */
    float variance;                   /**< Value variance estimate */
} nimcp_gt_q_entry_t;

/**
 * @brief Opaque Q-table handle
 */
typedef struct nimcp_gt_q_table_struct* nimcp_gt_q_table_t;

/**
 * @brief Regret table entry (for CFR)
 */
typedef struct {
    float* cumulative_regret;         /**< Cumulative regret per action */
    float* cumulative_strategy;       /**< Cumulative strategy profile */
    uint32_t num_actions;             /**< Actions at this info set */
    uint32_t iteration_count;         /**< CFR iterations seen */
} nimcp_gt_regret_entry_t;

/**
 * @brief Opaque regret table handle
 */
typedef struct nimcp_gt_regret_table_struct* nimcp_gt_regret_table_t;

/**
 * @brief Opponent model (beliefs about opponent type)
 */
typedef struct {
    float type_beliefs[NIMCP_GT_OPPONENT_COUNT];  /**< P(type | history) */
    nimcp_gt_opponent_type_t predicted_type;       /**< Most likely type */
    float prediction_confidence;                   /**< Confidence [0, 1] */

    /* Action prediction */
    float* action_predictions;        /**< P(action | type, state) */
    uint32_t num_actions;             /**< Number of opponent actions */

    /* History statistics */
    uint32_t* action_counts;          /**< Opponent action frequency */
    uint32_t total_observations;      /**< Total observations */

    /* Cooperation metrics */
    float cooperation_rate;           /**< Fraction of cooperative moves */
    float reciprocity_score;          /**< Response to our actions */
} nimcp_gt_opponent_model_t;

/**
 * @brief Learning statistics
 */
typedef struct {
    uint64_t updates;                 /**< Total updates performed */
    uint64_t actions_selected;        /**< Actions selected */
    uint64_t explorations;            /**< Exploratory actions */
    uint64_t exploitations;           /**< Greedy actions */
    float avg_reward;                 /**< Average reward received */
    float avg_q_value;                /**< Average Q-value */
    float current_learning_rate;      /**< Current learning rate */
    float current_exploration;        /**< Current exploration rate */
    float regret_sum;                 /**< Total regret (CFR) */
    float exploitability;             /**< Strategy exploitability */
} nimcp_gt_learning_stats_t;

/**
 * @brief Opaque learner handle
 */
typedef struct nimcp_gt_learner_struct* nimcp_gt_learner_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Get default learning configuration
 *
 * @return Default configuration with reasonable parameters
 */
nimcp_gt_learning_config_t nimcp_gt_learning_default_config(void);

/**
 * @brief Get learning method name
 */
const char* nimcp_gt_learn_method_name(nimcp_gt_learn_method_t method);

/**
 * @brief Get exploration strategy name
 */
const char* nimcp_gt_explore_strategy_name(nimcp_gt_explore_strategy_t strategy);

/**
 * @brief Get opponent type name
 */
const char* nimcp_gt_opponent_type_name(nimcp_gt_opponent_type_t type);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create learner context
 *
 * WHAT: Initialize learning agent with given configuration
 * WHY:  Prepare for strategic learning in repeated game
 * HOW:  Allocate Q-table, regret table, initialize parameters
 *
 * @param config Learning configuration
 * @param num_states Number of game states
 * @param num_actions Number of actions per state
 * @return Learner handle or NULL on failure
 */
nimcp_gt_learner_t nimcp_gt_learner_create(
    const nimcp_gt_learning_config_t* config,
    uint32_t num_states,
    uint32_t num_actions
);

/**
 * @brief Destroy learner context
 *
 * @param learner Learner handle (NULL safe)
 */
void nimcp_gt_learner_destroy(nimcp_gt_learner_t learner);

/**
 * @brief Reset learner to initial state
 *
 * Clears Q-values, regrets, statistics but preserves configuration.
 *
 * @param learner Learner handle
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_reset(nimcp_gt_learner_t learner);

//=============================================================================
// Core Learning Operations
//=============================================================================

/**
 * @brief Update learner with transition experience
 *
 * WHAT: Process (s, a, r, s') transition for learning
 * WHY:  Update value estimates based on experience
 * HOW:  Q-learning: Q(s,a) += alpha * (r + gamma*max_a' Q(s',a') - Q(s,a))
 *       SARSA: Q(s,a) += alpha * (r + gamma*Q(s',a') - Q(s,a))
 *
 * @param learner Learner handle
 * @param state Current state index
 * @param action Action taken
 * @param reward Reward received
 * @param next_state Next state index (can be same state)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_update(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action,
    float reward,
    uint32_t next_state
);

/**
 * @brief Update with full SARSA tuple
 *
 * SARSA-specific update using next action (on-policy).
 *
 * @param learner Learner handle
 * @param state Current state
 * @param action Action taken
 * @param reward Reward received
 * @param next_state Next state
 * @param next_action Next action (for SARSA)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_update_sarsa(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action,
    float reward,
    uint32_t next_state,
    uint32_t next_action
);

/**
 * @brief Select action using current policy
 *
 * WHAT: Choose action balancing exploration/exploitation
 * WHY:  Make decisions that learn while performing well
 * HOW:  Epsilon-greedy, Boltzmann, UCB, or Thompson sampling
 *
 * @param learner Learner handle
 * @param state Current state
 * @param action_out Output selected action
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_select_action(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t* action_out
);

/**
 * @brief Select greedy action (no exploration)
 *
 * @param learner Learner handle
 * @param state Current state
 * @param action_out Output best action
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_select_greedy(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t* action_out
);

/**
 * @brief Get Q-value for state-action pair
 *
 * @param learner Learner handle
 * @param state State index
 * @param action Action index
 * @return Q-value or 0.0 if invalid
 */
float nimcp_gt_learner_get_q_value(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action
);

/**
 * @brief Set Q-value for state-action pair
 *
 * @param learner Learner handle
 * @param state State index
 * @param action Action index
 * @param value New Q-value
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_set_q_value(
    nimcp_gt_learner_t learner,
    uint32_t state,
    uint32_t action,
    float value
);

/**
 * @brief Get current strategy (action probabilities) for state
 *
 * @param learner Learner handle
 * @param state State index
 * @param strategy_out Output array (size num_actions)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_get_strategy(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    float* strategy_out
);

//=============================================================================
// Counterfactual Regret Minimization (CFR)
//=============================================================================

/**
 * @brief Update CFR regrets for information set
 *
 * WHAT: Update regret and strategy sums for CFR
 * WHY:  Converge to Nash equilibrium in extensive-form games
 * HOW:  Accumulate counterfactual regret, regret-match for strategy
 *
 * FORMULA:
 *   regret_a += utility_a - sum_b(strategy_b * utility_b)
 *   strategy_a = max(0, regret_a) / sum_b(max(0, regret_b))
 *
 * @param learner Learner handle
 * @param info_set Information set identifier
 * @param actions Available actions at info set
 * @param num_actions Number of actions
 * @param utilities Utility for each action (counterfactual values)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_cfr_update(
    nimcp_gt_learner_t learner,
    uint32_t info_set,
    const uint32_t* actions,
    uint32_t num_actions,
    const float* utilities
);

/**
 * @brief Get current CFR strategy for information set
 *
 * Returns regret-matched strategy (or uniform if no regret).
 *
 * @param learner Learner handle
 * @param info_set Information set identifier
 * @param strategy_out Output strategy (size num_actions)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_cfr_get_strategy(
    const nimcp_gt_learner_t learner,
    uint32_t info_set,
    float* strategy_out
);

/**
 * @brief Get average CFR strategy (converged)
 *
 * Returns time-averaged strategy (Nash equilibrium approximation).
 *
 * @param learner Learner handle
 * @param info_set Information set identifier
 * @param strategy_out Output average strategy
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_cfr_get_average_strategy(
    const nimcp_gt_learner_t learner,
    uint32_t info_set,
    float* strategy_out
);

/**
 * @brief Get total CFR regret for information set
 *
 * @param learner Learner handle
 * @param info_set Information set identifier
 * @return Total positive regret or 0.0 if invalid
 */
float nimcp_gt_cfr_get_regret(
    const nimcp_gt_learner_t learner,
    uint32_t info_set
);

//=============================================================================
// Fictitious Play
//=============================================================================

/**
 * @brief Update beliefs with observed opponent action
 *
 * WHAT: Update empirical distribution of opponent actions
 * WHY:  Converge to mixed Nash in 2-player zero-sum games
 * HOW:  Count opponent actions, best-respond to empirical distribution
 *
 * @param learner Learner handle
 * @param opponent_action Observed opponent action
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_fictitious_play_update(
    nimcp_gt_learner_t learner,
    uint32_t opponent_action
);

/**
 * @brief Predict opponent's next action (empirical mode)
 *
 * @param learner Learner handle
 * @param prediction_out Output predicted action
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_fictitious_play_predict(
    const nimcp_gt_learner_t learner,
    uint32_t* prediction_out
);

/**
 * @brief Get empirical opponent action distribution
 *
 * @param learner Learner handle
 * @param distribution_out Output distribution (size num_actions)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_fictitious_play_get_distribution(
    const nimcp_gt_learner_t learner,
    float* distribution_out
);

/**
 * @brief Compute best response to current opponent model
 *
 * @param learner Learner handle
 * @param state Current state
 * @param payoff_matrix Row-major payoff matrix [actions x opponent_actions]
 * @param best_action_out Output best response action
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_fictitious_play_best_response(
    const nimcp_gt_learner_t learner,
    uint32_t state,
    const float* payoff_matrix,
    uint32_t* best_action_out
);

//=============================================================================
// EXP3 (Adversarial Bandits)
//=============================================================================

/**
 * @brief Update EXP3 weights with reward
 *
 * WHAT: Update action weights for adversarial setting
 * WHY:  Handle non-stationary or adversarial opponents
 * HOW:  Multiplicative weight update with unbiased estimator
 *
 * @param learner Learner handle
 * @param action Action that was taken
 * @param reward Reward received [0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_exp3_update(
    nimcp_gt_learner_t learner,
    uint32_t action,
    float reward
);

/**
 * @brief Select action using EXP3 distribution
 *
 * @param learner Learner handle
 * @param action_out Output selected action
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_exp3_select(
    nimcp_gt_learner_t learner,
    uint32_t* action_out
);

/**
 * @brief Get EXP3 action probabilities
 *
 * @param learner Learner handle
 * @param probabilities_out Output probabilities (size num_actions)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_exp3_get_probabilities(
    const nimcp_gt_learner_t learner,
    float* probabilities_out
);

//=============================================================================
// Opponent Modeling
//=============================================================================

/**
 * @brief Model opponent from action history
 *
 * WHAT: Infer opponent type and predict behavior
 * WHY:  Adapt strategy based on opponent characteristics
 * HOW:  Bayesian type inference, action pattern recognition
 *
 * @param learner Learner handle
 * @param history Array of past opponent actions
 * @param history_len Length of history
 * @param model_out Output opponent model
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_model_opponent(
    nimcp_gt_learner_t learner,
    const uint32_t* history,
    uint32_t history_len,
    nimcp_gt_opponent_model_t* model_out
);

/**
 * @brief Update opponent model with new observation
 *
 * @param learner Learner handle
 * @param our_action Our previous action
 * @param opponent_action Opponent's response
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_update_opponent_model(
    nimcp_gt_learner_t learner,
    uint32_t our_action,
    uint32_t opponent_action
);

/**
 * @brief Get current opponent model
 *
 * @param learner Learner handle
 * @param model_out Output opponent model (copied)
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_get_opponent_model(
    const nimcp_gt_learner_t learner,
    nimcp_gt_opponent_model_t* model_out
);

/**
 * @brief Predict opponent's next action
 *
 * @param learner Learner handle
 * @param our_action Our intended action
 * @param prediction_out Output predicted opponent action
 * @param confidence_out Output prediction confidence [0, 1]
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_predict_opponent_action(
    const nimcp_gt_learner_t learner,
    uint32_t our_action,
    uint32_t* prediction_out,
    float* confidence_out
);

//=============================================================================
// Learning Rate and Exploration Scheduling
//=============================================================================

/**
 * @brief Advance schedule step (decay rates)
 *
 * @param learner Learner handle
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_advance_schedule(nimcp_gt_learner_t learner);

/**
 * @brief Get current learning rate
 */
float nimcp_gt_learner_get_learning_rate(const nimcp_gt_learner_t learner);

/**
 * @brief Get current exploration rate
 */
float nimcp_gt_learner_get_exploration_rate(const nimcp_gt_learner_t learner);

/**
 * @brief Set learning rate directly
 */
nimcp_error_t nimcp_gt_learner_set_learning_rate(
    nimcp_gt_learner_t learner,
    float rate
);

/**
 * @brief Set exploration rate directly
 */
nimcp_error_t nimcp_gt_learner_set_exploration_rate(
    nimcp_gt_learner_t learner,
    float rate
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get learning statistics
 *
 * @param learner Learner handle
 * @param stats_out Output statistics
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_learner_get_stats(
    const nimcp_gt_learner_t learner,
    nimcp_gt_learning_stats_t* stats_out
);

/**
 * @brief Reset learning statistics
 *
 * @param learner Learner handle
 */
void nimcp_gt_learner_reset_stats(nimcp_gt_learner_t learner);

/**
 * @brief Check if learning has converged
 *
 * @param learner Learner handle
 * @param threshold Convergence threshold
 * @return true if Q-values stable within threshold
 */
bool nimcp_gt_learner_has_converged(
    const nimcp_gt_learner_t learner,
    float threshold
);

/**
 * @brief Compute exploitability of current strategy
 *
 * @param learner Learner handle
 * @param payoff_matrix Game payoff matrix
 * @param exploitability_out Output exploitability value
 * @return NIMCP_SUCCESS or error
 */
nimcp_error_t nimcp_gt_compute_exploitability(
    const nimcp_gt_learner_t learner,
    const float* payoff_matrix,
    float* exploitability_out
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Initialize opponent model structure
 *
 * @param model Model to initialize
 * @param num_actions Number of opponent actions
 */
void nimcp_gt_opponent_model_init(
    nimcp_gt_opponent_model_t* model,
    uint32_t num_actions
);

/**
 * @brief Cleanup opponent model structure
 *
 * @param model Model to cleanup
 */
void nimcp_gt_opponent_model_cleanup(nimcp_gt_opponent_model_t* model);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_LEARNING_H
