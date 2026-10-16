/**
 * @file nimcp_basal_ganglia_fep_bridge.h
 * @brief Basal Ganglia-to-Free Energy Principle Integration Bridge
 *
 * WHAT: Bridges basal ganglia action selection to Free Energy Principle framework
 * WHY:  Action selection as expected free energy minimization; habits as low-free-energy policies
 * HOW:  Generative model of action outcomes, active inference for motor control
 *
 * BIOLOGICAL BASIS:
 * - Action selection = minimizing expected free energy across action options
 * - Reward prediction error (dopamine) = FEP surprise signal
 * - Direct pathway (D1) = precision on action being beneficial
 * - Indirect pathway (D2) = precision on action being costly/risky
 * - Habit formation = crystallization of low-free-energy policies
 * - Goal-directed behavior = active inference with explicit outcome models
 * - Conflict = entropy in action posterior (uncertainty about best action)
 *
 * FEP MAPPING:
 * - Generative model: Predicts action outcomes (reward, cost, state transitions)
 * - Prediction error: Reward prediction error (δ = r - V) via dopamine
 * - Precision: D1/D2 balance, confidence in action value estimates
 * - Active inference: Policy selection to minimize expected free energy
 * - Habits: Cached policies with low expected free energy
 *
 * References:
 * - Friston et al. (2016). Active inference and learning
 * - FitzGerald et al. (2015). Dopamine, reward learning, and active inference
 * - Pezzulo et al. (2018). Active inference and the basal ganglia
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BASAL_GANGLIA_FEP_BRIDGE_H
#define NIMCP_BASAL_GANGLIA_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct basal_ganglia;
typedef struct basal_ganglia basal_ganglia_t;

//=============================================================================
// Constants
//=============================================================================

#define BG_FEP_MAX_ACTIONS 64           /**< Max actions to evaluate */
#define BG_FEP_MAX_OUTCOMES 16          /**< Max outcome dimensions */
#define BG_FEP_DEFAULT_PRECISION 1.0f   /**< Default precision weight */
#define BG_FEP_NUM_POLICY_MODELS 4      /**< Number of policy inference models */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Policy inference models for action selection
 */
typedef enum {
    BG_FEP_MODEL_EXPLORE = 0,        /**< Exploratory policy (high epistemic value) */
    BG_FEP_MODEL_EXPLOIT,            /**< Exploitative policy (high pragmatic value) */
    BG_FEP_MODEL_HABIT,              /**< Habitual policy (cached low-FE) */
    BG_FEP_MODEL_CAUTIOUS            /**< Risk-averse policy (minimize worst case) */
} bg_fep_model_t;

/**
 * @brief Action evaluation state
 */
typedef enum {
    BG_FEP_ACTION_PENDING = 0,       /**< Action not yet evaluated */
    BG_FEP_ACTION_EVALUATING,        /**< Action being evaluated */
    BG_FEP_ACTION_SELECTED,          /**< Action selected for execution */
    BG_FEP_ACTION_EXECUTING,         /**< Action being executed */
    BG_FEP_ACTION_FEEDBACK           /**< Awaiting outcome feedback */
} bg_fep_action_state_t;

/**
 * @brief Precision source for action valuation
 */
typedef enum {
    BG_FEP_PRECISION_FIXED = 0,      /**< Fixed precision weights */
    BG_FEP_PRECISION_DOPAMINE,       /**< Precision from dopamine level */
    BG_FEP_PRECISION_ADAPTIVE,       /**< Adaptive based on recent prediction errors */
    BG_FEP_PRECISION_HIERARCHICAL    /**< Hierarchical precision (cortical input) */
} bg_fep_precision_mode_t;

/**
 * @brief Decision confidence level
 */
typedef enum {
    BG_FEP_CONFIDENCE_LOW = 0,       /**< Low confidence (high entropy) */
    BG_FEP_CONFIDENCE_MEDIUM,        /**< Medium confidence */
    BG_FEP_CONFIDENCE_HIGH           /**< High confidence (low entropy) */
} bg_fep_confidence_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Prediction error components for action selection
 */
typedef struct {
    float reward_error;              /**< Reward prediction error (δ = r - V) */
    float state_error;               /**< State transition prediction error */
    float cost_error;                /**< Action cost prediction error */
    float outcome_error;             /**< Overall outcome prediction error */
    float total_free_energy;         /**< Total variational free energy */
    float precision_weighted_error;  /**< Precision-weighted sum of errors */
} bg_fep_errors_t;

/**
 * @brief Expected free energy for an action
 */
typedef struct {
    uint32_t action_id;              /**< Action identifier */
    float pragmatic_value;           /**< Expected reward/value (G_pragmatic) */
    float epistemic_value;           /**< Information gain (G_epistemic) */
    float cost;                      /**< Expected action cost */
    float risk;                      /**< Risk/variance of outcome */
    float expected_free_energy;      /**< Total expected FE (lower = better) */
    float posterior_probability;     /**< Softmax probability for selection */
} bg_fep_action_eval_t;

/**
 * @brief Active inference state for action selection
 */
typedef struct {
    float beliefs[BG_FEP_MAX_ACTIONS];    /**< Posterior beliefs about action values */
    float precision[BG_FEP_MAX_ACTIONS];  /**< Precision for each action belief */
    uint32_t num_actions;                  /**< Number of available actions */
    bg_fep_action_eval_t* evaluations;     /**< Detailed action evaluations */
    float policy_entropy;                  /**< Entropy of action distribution */
    uint32_t selected_action;              /**< Currently selected action */
    bg_fep_confidence_t confidence;        /**< Decision confidence */
} bg_fep_inference_t;

/**
 * @brief Outcome model for action predictions
 */
typedef struct {
    float reward_mean;               /**< Expected reward */
    float reward_variance;           /**< Reward variance */
    float* state_transition;         /**< State transition probabilities */
    uint32_t num_states;             /**< Number of outcome states */
    float cost_mean;                 /**< Expected cost */
    float success_probability;       /**< Probability of success */
} bg_fep_outcome_model_t;

/**
 * @brief Configuration for basal ganglia FEP bridge
 */
typedef struct {
    /* Model settings */
    bg_fep_model_t default_model;           /**< Default policy model */
    bool auto_model_selection;               /**< Auto-select based on context */
    float model_switch_threshold;            /**< Evidence threshold for switching */

    /* Precision settings */
    bg_fep_precision_mode_t precision_mode;  /**< Precision weighting mode */
    float action_precision;                  /**< Default action precision */
    float outcome_precision;                 /**< Outcome prediction precision */
    float prior_precision;                   /**< Prior belief precision */

    /* Free energy computation */
    float epistemic_weight;                  /**< Weight for epistemic value (exploration) */
    float pragmatic_weight;                  /**< Weight for pragmatic value (reward) */
    float cost_weight;                       /**< Weight for action cost */
    float risk_sensitivity;                  /**< Risk sensitivity parameter */

    /* Habit parameters */
    float habit_prior_boost;                 /**< Prior boost for habitual actions */
    float habit_precision_boost;             /**< Precision boost for habits */
    float habit_threshold;                   /**< Threshold for habit crystallization */

    /* Softmax temperature */
    float selection_temperature;             /**< Temperature for action selection */

    /* Learning rates */
    float reward_learning_rate;              /**< Learning rate for reward predictions */
    float transition_learning_rate;          /**< Learning rate for state transitions */
    float precision_learning_rate;           /**< Learning rate for precision updates */
} bg_fep_config_t;

/**
 * @brief Statistics for basal ganglia FEP bridge
 */
typedef struct {
    uint64_t total_inferences;               /**< Total inference cycles */
    uint64_t action_selections;              /**< Actions selected */
    uint64_t habit_selections;               /**< Habitual action selections */
    uint64_t explore_selections;             /**< Exploratory selections */
    float avg_free_energy;                   /**< Average free energy */
    float avg_prediction_error;              /**< Average prediction error */
    float avg_entropy;                       /**< Average policy entropy */
    float avg_precision;                     /**< Average precision */
    uint64_t model_switches;                 /**< Number of model switches */
    float exploration_ratio;                 /**< Ratio of exploratory actions */
} bg_fep_stats_t;

/**
 * @brief Opaque handle to basal ganglia FEP bridge
 */
typedef struct bg_fep_bridge bg_fep_bridge_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default configuration
 * @param config Configuration to initialize
 */
void bg_fep_default_config(bg_fep_config_t* config);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
bool bg_fep_validate_config(const bg_fep_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create basal ganglia FEP bridge
 * @param config Configuration (NULL for defaults)
 * @return New bridge instance or NULL on failure
 */
bg_fep_bridge_t* bg_fep_create(const bg_fep_config_t* config);

/**
 * @brief Destroy basal ganglia FEP bridge
 * @param bridge Bridge to destroy
 */
void bg_fep_destroy(bg_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int bg_fep_reset(bg_fep_bridge_t* bridge);

//=============================================================================
// Prediction Error Functions
//=============================================================================

/**
 * @brief Compute prediction errors from action outcome
 * @param bridge FEP bridge
 * @param action_id Action that was executed
 * @param actual_reward Actual reward received
 * @param expected_reward Expected reward
 * @param errors Output: prediction error components
 * @return 0 on success, -1 on error
 */
int bg_fep_compute_errors(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float actual_reward,
    float expected_reward,
    bg_fep_errors_t* errors
);

/**
 * @brief Get current free energy
 * @param bridge FEP bridge
 * @return Current variational free energy
 */
float bg_fep_get_free_energy(const bg_fep_bridge_t* bridge);

/**
 * @brief Get surprise (negative log model evidence)
 * @param bridge FEP bridge
 * @return Current surprise value
 */
float bg_fep_get_surprise(const bg_fep_bridge_t* bridge);

/**
 * @brief Update precision based on prediction errors
 * @param bridge FEP bridge
 * @param action_id Action to update
 * @param prediction_error Recent prediction error
 * @return 0 on success, -1 on error
 */
int bg_fep_update_precision(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float prediction_error
);

//=============================================================================
// Active Inference - Action Selection
//=============================================================================

/**
 * @brief Evaluate expected free energy for all actions
 * @param bridge FEP bridge
 * @param action_values Current action values (Q-values)
 * @param num_actions Number of actions
 * @param evaluations Output: action evaluations
 * @return 0 on success, -1 on error
 */
int bg_fep_evaluate_actions(
    bg_fep_bridge_t* bridge,
    const float* action_values,
    uint32_t num_actions,
    bg_fep_action_eval_t* evaluations
);

/**
 * @brief Select action via active inference
 * @param bridge FEP bridge
 * @param evaluations Action evaluations
 * @param num_actions Number of actions
 * @param selected Output: selected action ID
 * @return 0 on success, -1 on error
 */
int bg_fep_select_action(
    bg_fep_bridge_t* bridge,
    const bg_fep_action_eval_t* evaluations,
    uint32_t num_actions,
    uint32_t* selected
);

/**
 * @brief Get expected free energy for specific action
 * @param bridge FEP bridge
 * @param action_id Action to evaluate
 * @return Expected free energy (lower = better)
 */
float bg_fep_expected_free_energy(
    const bg_fep_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Get policy entropy (decision uncertainty)
 * @param bridge FEP bridge
 * @return Policy entropy (higher = more uncertain)
 */
float bg_fep_get_policy_entropy(const bg_fep_bridge_t* bridge);

/**
 * @brief Get current inference state
 * @param bridge FEP bridge
 * @param inference Output: inference state
 * @return 0 on success, -1 on error
 */
int bg_fep_get_inference_state(
    const bg_fep_bridge_t* bridge,
    bg_fep_inference_t* inference
);

//=============================================================================
// Generative Model Functions
//=============================================================================

/**
 * @brief Set current policy model
 * @param bridge FEP bridge
 * @param model Model to use
 * @return 0 on success, -1 on error
 */
int bg_fep_set_model(bg_fep_bridge_t* bridge, bg_fep_model_t model);

/**
 * @brief Get best policy model based on evidence
 * @param bridge FEP bridge
 * @return Best model according to model evidence
 */
bg_fep_model_t bg_fep_get_best_model(const bg_fep_bridge_t* bridge);

/**
 * @brief Get model evidence for policy model
 * @param bridge FEP bridge
 * @param model Model to query
 * @return Log model evidence
 */
float bg_fep_get_model_evidence(
    const bg_fep_bridge_t* bridge,
    bg_fep_model_t model
);

/**
 * @brief Update outcome model from experience
 * @param bridge FEP bridge
 * @param action_id Action taken
 * @param outcome_reward Observed reward
 * @param outcome_cost Observed cost
 * @param success Whether action succeeded
 * @return 0 on success, -1 on error
 */
int bg_fep_update_outcome_model(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float outcome_reward,
    float outcome_cost,
    bool success
);

/**
 * @brief Predict outcome for action
 * @param bridge FEP bridge
 * @param action_id Action to predict
 * @param outcome Output: predicted outcome
 * @return 0 on success, -1 on error
 */
int bg_fep_predict_outcome(
    const bg_fep_bridge_t* bridge,
    uint32_t action_id,
    bg_fep_outcome_model_t* outcome
);

//=============================================================================
// Habit-Related Functions
//=============================================================================

/**
 * @brief Mark action as habitual (boost prior/precision)
 * @param bridge FEP bridge
 * @param action_id Action to mark as habit
 * @param strength Habit strength [0-1]
 * @return 0 on success, -1 on error
 */
int bg_fep_set_habit(
    bg_fep_bridge_t* bridge,
    uint32_t action_id,
    float strength
);

/**
 * @brief Get habit prior boost for action
 * @param bridge FEP bridge
 * @param action_id Action to query
 * @return Habit prior boost (1.0 = no boost)
 */
float bg_fep_get_habit_prior(
    const bg_fep_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Clear habit for action
 * @param bridge FEP bridge
 * @param action_id Action to clear habit
 * @return 0 on success, -1 on error
 */
int bg_fep_clear_habit(bg_fep_bridge_t* bridge, uint32_t action_id);

/**
 * @brief Check if habit model is active
 * @param bridge FEP bridge
 * @return true if habit model active
 */
bool bg_fep_is_habit_mode(const bg_fep_bridge_t* bridge);

//=============================================================================
// Dopamine Integration
//=============================================================================

/**
 * @brief Set dopamine level (precision modulation)
 * @param bridge FEP bridge
 * @param dopamine_level Dopamine level [0-1]
 * @return 0 on success, -1 on error
 */
int bg_fep_set_dopamine(bg_fep_bridge_t* bridge, float dopamine_level);

/**
 * @brief Get dopamine-modulated precision
 * @param bridge FEP bridge
 * @return Current precision (dopamine-modulated)
 */
float bg_fep_get_dopamine_precision(const bg_fep_bridge_t* bridge);

/**
 * @brief Process reward prediction error (dopamine signal)
 * @param bridge FEP bridge
 * @param rpe Reward prediction error
 * @return 0 on success, -1 on error
 */
int bg_fep_process_rpe(bg_fep_bridge_t* bridge, float rpe);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect to basal ganglia system
 * @param bridge FEP bridge
 * @param bg Basal ganglia system
 * @return 0 on success, -1 on error
 */
int bg_fep_connect_basal_ganglia(
    bg_fep_bridge_t* bridge,
    basal_ganglia_t* bg
);

/**
 * @brief Connect to FEP orchestrator
 * @param bridge FEP bridge
 * @param orchestrator FEP orchestrator (void* for flexibility)
 * @return 0 on success, -1 on error
 */
int bg_fep_connect_orchestrator(
    bg_fep_bridge_t* bridge,
    void* orchestrator
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update FEP bridge (main update loop)
 * @param bridge FEP bridge
 * @param dt Time delta in milliseconds
 * @return 0 on success, -1 on error
 */
int bg_fep_update(bg_fep_bridge_t* bridge, float dt);

/**
 * @brief Synchronize with basal ganglia state
 * @param bridge FEP bridge
 * @return 0 on success, -1 on error
 */
int bg_fep_sync_with_bg(bg_fep_bridge_t* bridge);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge FEP bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int bg_fep_get_stats(
    const bg_fep_bridge_t* bridge,
    bg_fep_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge FEP bridge
 * @return 0 on success, -1 on error
 */
int bg_fep_reset_stats(bg_fep_bridge_t* bridge);

/**
 * @brief Get model name string
 * @param model Model type
 * @return Model name
 */
const char* bg_fep_model_name(bg_fep_model_t model);

/**
 * @brief Get action state name string
 * @param state Action state
 * @return State name
 */
const char* bg_fep_action_state_name(bg_fep_action_state_t state);

/**
 * @brief Get precision mode name string
 * @param mode Precision mode
 * @return Mode name
 */
const char* bg_fep_precision_mode_name(bg_fep_precision_mode_t mode);

/**
 * @brief Get confidence level name string
 * @param confidence Confidence level
 * @return Level name
 */
const char* bg_fep_confidence_name(bg_fep_confidence_t confidence);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_FEP_BRIDGE_H */
