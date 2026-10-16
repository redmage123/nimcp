//=============================================================================
// nimcp_financial_predictive_bridge.h - Financial Predictive Coding Bridge
//=============================================================================
/**
 * @file nimcp_financial_predictive_bridge.h
 * @brief Predictive coding for financial decision making via Free Energy Principle
 *
 * WHAT: Implements predictive coding framework for financial market analysis,
 *       enabling hierarchical Bayesian inference, expected free energy computation,
 *       and active inference for optimal action selection.
 *
 * WHY:  Financial markets are inherently uncertain environments where traditional
 *       models fail. Predictive coding provides a principled approach to:
 *       - Maintain probabilistic beliefs about market states
 *       - Minimize prediction errors through belief updating
 *       - Select actions that minimize expected free energy
 *       - Balance exploration (information gain) vs exploitation (goal pursuit)
 *
 * HOW:  The bridge implements the Free Energy Principle (FEP) where:
 *       - Predictions are made based on internal generative models
 *       - Prediction errors drive belief updates (perception)
 *       - Expected Free Energy (EFE) guides action selection (action)
 *       - Active inference minimizes surprise by choosing uncertainty-reducing actions
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |              FINANCIAL PREDICTIVE CODING BRIDGE                   |
 * +------------------------------------------------------------------+
 * |                                                                   |
 * |   +-------------------+    +-------------------+                  |
 * |   |  GENERATIVE MODEL |    |  BELIEF STATE     |                  |
 * |   |                   |    |                   |                  |
 * |   | Prior P(x)        |    | Predictions       |                  |
 * |   | Likelihood P(y|x) |    | Precisions        |                  |
 * |   | Dynamics P(x'|x)  |    | Prediction Errors |                  |
 * |   +--------+----------+    +--------+----------+                  |
 * |            |                        |                             |
 * |            v                        v                             |
 * |   +------------------------------------------+                    |
 * |   |        PREDICTION ERROR MINIMIZATION     |                    |
 * |   |   PE = Precision * (Actual - Predicted)  |                    |
 * |   +------------------------------------------+                    |
 * |            |                        |                             |
 * |            v                        v                             |
 * |   +-------------------+    +-------------------+                  |
 * |   | BELIEF UPDATE     |    | ACTION SELECTION  |                  |
 * |   | (Perception)      |    | (Active Inference)|                  |
 * |   +-------------------+    +-------------------+                  |
 * |            |                        |                             |
 * |            v                        v                             |
 * |   +------------------------------------------+                    |
 * |   |    EXPECTED FREE ENERGY (EFE)            |                    |
 * |   |    G(π) = Epistemic + Pragmatic          |                    |
 * |   +------------------------------------------+                    |
 * |                                                                   |
 * +------------------------------------------------------------------+
 * ```
 *
 * THEORETICAL FOUNDATION:
 * =======================
 * Free Energy F = D_KL[Q(x) || P(x|y)] + H[y]
 *
 * Where:
 * - Q(x) is the approximate posterior (beliefs about hidden states)
 * - P(x|y) is the true posterior
 * - D_KL is Kullback-Leibler divergence
 * - H[y] is entropy of observations
 *
 * Expected Free Energy for policy π:
 * G(π) = E_Q[log Q(x|π) - log P(x,y|π)]
 *      = Epistemic_value + Pragmatic_value
 *
 * Epistemic: Information gain about states
 * Pragmatic: Expected deviation from preferred outcomes
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#ifndef NIMCP_FINANCIAL_PREDICTIVE_BRIDGE_H
#define NIMCP_FINANCIAL_PREDICTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Bio-async module identifier for financial predictive bridge */
#define BIO_MODULE_FINANCIAL_PREDICTIVE     0x03A1

/** Maximum number of assets in predictive state */
#define FIN_PREDICTIVE_MAX_ASSETS           256

/** Maximum prediction horizon (time steps) */
#define FIN_PREDICTIVE_MAX_HORIZON          64

/** Maximum number of policies for active inference */
#define FIN_PREDICTIVE_MAX_POLICIES         32

/** Maximum state dimension for belief states */
#define FIN_PREDICTIVE_MAX_STATE_DIM        128

//=============================================================================
// Error Codes
//=============================================================================

#define FIN_PREDICTIVE_ERROR_BASE           35000
#define FIN_PREDICTIVE_ERR_OK               0
#define FIN_PREDICTIVE_ERR_NULL             (FIN_PREDICTIVE_ERROR_BASE + 1)
#define FIN_PREDICTIVE_ERR_INVALID_PARAM    (FIN_PREDICTIVE_ERROR_BASE + 2)
#define FIN_PREDICTIVE_ERR_NO_MEMORY        (FIN_PREDICTIVE_ERROR_BASE + 3)
#define FIN_PREDICTIVE_ERR_NOT_INITIALIZED  (FIN_PREDICTIVE_ERROR_BASE + 4)
#define FIN_PREDICTIVE_ERR_PREDICTION       (FIN_PREDICTIVE_ERROR_BASE + 5)
#define FIN_PREDICTIVE_ERR_UPDATE           (FIN_PREDICTIVE_ERROR_BASE + 6)
#define FIN_PREDICTIVE_ERR_EFE              (FIN_PREDICTIVE_ERROR_BASE + 7)
#define FIN_PREDICTIVE_ERR_ACTIVE_INFERENCE (FIN_PREDICTIVE_ERROR_BASE + 8)
#define FIN_PREDICTIVE_ERR_SUBSYSTEM        (FIN_PREDICTIVE_ERROR_BASE + 9)
#define FIN_PREDICTIVE_ERR_VALIDATION       (FIN_PREDICTIVE_ERROR_BASE + 10)
#define FIN_PREDICTIVE_ERR_CONVERGENCE      (FIN_PREDICTIVE_ERROR_BASE + 11)

//=============================================================================
// Enumerations
//=============================================================================

/** Bridge operational state */
typedef enum {
    FIN_PREDICTIVE_STATE_UNINITIALIZED = 0,
    FIN_PREDICTIVE_STATE_IDLE,
    FIN_PREDICTIVE_STATE_PREDICTING,
    FIN_PREDICTIVE_STATE_UPDATING,
    FIN_PREDICTIVE_STATE_COMPUTING_EFE,
    FIN_PREDICTIVE_STATE_ACTIVE_INFERENCE,
    FIN_PREDICTIVE_STATE_ERROR
} fin_predictive_op_state_t;

/** Action types for active inference */
typedef enum {
    FIN_ACTION_HOLD = 0,            /**< Maintain current position */
    FIN_ACTION_BUY,                 /**< Increase position */
    FIN_ACTION_SELL,                /**< Decrease position */
    FIN_ACTION_HEDGE,               /**< Add hedging position */
    FIN_ACTION_REBALANCE,           /**< Rebalance portfolio */
    FIN_ACTION_OBSERVE,             /**< Pure observation (epistemic) */
    FIN_ACTION_COUNT
} fin_action_type_t;

/** Prediction model types */
typedef enum {
    FIN_MODEL_RANDOM_WALK,          /**< Random walk with drift */
    FIN_MODEL_MEAN_REVERSION,       /**< Ornstein-Uhlenbeck */
    FIN_MODEL_MOMENTUM,             /**< Trend-following */
    FIN_MODEL_REGIME_SWITCHING,     /**< Hidden Markov model */
    FIN_MODEL_HIERARCHICAL,         /**< Multi-level Bayesian */
    FIN_MODEL_COUNT
} fin_model_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Core predictive state containing predictions and precisions
 *
 * This structure holds the current belief state about financial assets,
 * including point predictions, precision (inverse variance) estimates,
 * and computed prediction errors.
 */
typedef struct {
    float* predictions;             /**< Predicted prices/returns [num_assets * horizon] */
    float* precisions;              /**< Confidence in each prediction (inverse variance) */
    float* prediction_errors;       /**< Actual - predicted values */
    uint32_t num_assets;            /**< Number of assets being tracked */
    uint32_t horizon;               /**< Prediction horizon (time steps) */
} fin_predictive_state_t;

/**
 * @brief Bridge statistics for monitoring and diagnostics
 */
typedef struct {
    uint64_t predictions_made;      /**< Total predictions generated */
    uint64_t updates;               /**< Total belief updates performed */
    uint64_t efe_computations;      /**< Expected free energy computations */
    uint64_t active_inferences;     /**< Active inference selections */
    uint64_t immune_checks;         /**< Immune system validations */
    uint64_t bbb_validations;       /**< Blood-brain barrier validations */
    uint64_t kg_messages_sent;      /**< Knowledge graph messages sent */
    uint64_t health_heartbeats;     /**< Health agent heartbeats */
} fin_predictive_bridge_stats_t;

/**
 * @brief Expected Free Energy result for a policy/action
 */
typedef struct {
    fin_action_type_t action;       /**< The evaluated action */
    float total_efe;                /**< Total expected free energy G(π) */
    float epistemic_value;          /**< Information gain component */
    float pragmatic_value;          /**< Goal-achievement component */
    float complexity_cost;          /**< Model complexity penalty */
    float probability;              /**< Policy probability after softmax */
    float confidence;               /**< Confidence in this evaluation */
} fin_efe_result_t;

/**
 * @brief Active inference result containing selected action and alternatives
 */
typedef struct {
    fin_action_type_t selected_action;      /**< Chosen action */
    float selected_action_efe;              /**< EFE of selected action */
    float* action_weights;                  /**< Weights for each asset */
    uint32_t num_weights;                   /**< Number of weight values */
    float expected_return;                  /**< Expected return under action */
    float expected_risk;                    /**< Expected risk under action */
    float exploration_bonus;                /**< Epistemic contribution */
    fin_efe_result_t* all_actions;          /**< EFE for all evaluated actions */
    uint32_t num_actions;                   /**< Number of evaluated actions */
} fin_active_inference_result_t;

/**
 * @brief Market observation for belief updating
 */
typedef struct {
    float* prices;                  /**< Current prices [num_assets] */
    float* volumes;                 /**< Current volumes [num_assets] */
    float* returns;                 /**< Recent returns [num_assets] */
    float* volatilities;            /**< Current volatilities [num_assets] */
    uint32_t num_assets;            /**< Number of assets */
    uint64_t timestamp_us;          /**< Observation timestamp */
} fin_market_observation_t;

/**
 * @brief Preferred outcome specification for pragmatic value
 */
typedef struct {
    float target_return;            /**< Target portfolio return */
    float max_drawdown;             /**< Maximum acceptable drawdown */
    float target_sharpe;            /**< Target Sharpe ratio */
    float risk_tolerance;           /**< Risk tolerance [0-1] */
    float* asset_preferences;       /**< Per-asset preferences [-1 to 1] */
    uint32_t num_assets;            /**< Number of assets */
} fin_preferred_outcome_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Bridge configuration structure
 */
typedef struct {
    /* Model settings */
    fin_model_type_t model_type;    /**< Prediction model type */
    uint32_t num_assets;            /**< Number of assets to track */
    uint32_t prediction_horizon;    /**< Prediction horizon (steps) */

    /* Precision settings */
    float initial_precision;        /**< Initial precision estimate */
    float min_precision;            /**< Minimum precision (avoid division by zero) */
    float max_precision;            /**< Maximum precision (cap confidence) */
    float precision_learning_rate;  /**< Rate of precision adaptation */

    /* Belief update settings */
    float belief_learning_rate;     /**< Rate of belief updates */
    float prediction_error_gain;    /**< Gain on prediction errors */

    /* Active inference settings */
    float efe_temperature;          /**< Softmax temperature for action selection */
    float exploration_weight;       /**< Balance epistemic vs pragmatic (0-1) */
    float complexity_weight;        /**< Weight on model complexity term */
    uint32_t efe_num_samples;       /**< Monte Carlo samples for EFE */

    /* Modulation sensitivity */
    float inflammation_sensitivity; /**< Sensitivity to inflammation [0-2] */
    float fatigue_sensitivity;      /**< Sensitivity to fatigue [0-2] */

    /* Security settings */
    bool enable_bbb_validation;     /**< Enable blood-brain barrier checks */
    bool enable_immune_validation;  /**< Enable immune system checks */
} fin_predictive_config_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct financial_predictive_bridge financial_predictive_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 * @return Default configuration with sensible values
 */
fin_predictive_config_t financial_predictive_bridge_default_config(void);

/**
 * @brief Create financial predictive bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
financial_predictive_bridge_t* financial_predictive_bridge_create(
    const fin_predictive_config_t* config);

/**
 * @brief Destroy bridge and free resources
 * @param bridge Bridge handle
 */
void financial_predictive_bridge_destroy(financial_predictive_bridge_t* bridge);

/**
 * @brief Get current bridge state
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_predictive_op_state_t financial_predictive_bridge_get_state(
    const financial_predictive_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_predictive_bridge_reset(financial_predictive_bridge_t* bridge);

//=============================================================================
// Subsystem Setters
//=============================================================================

/**
 * @brief Set immune system for validation
 * @param bridge Bridge handle
 * @param immune Immune system handle (NULL to disable)
 * @return 0 on success
 */
int financial_predictive_bridge_set_immune(financial_predictive_bridge_t* bridge,
                                            void* immune);

/**
 * @brief Set blood-brain barrier for data validation
 * @param bridge Bridge handle
 * @param bbb BBB handle (NULL to disable)
 * @return 0 on success
 */
int financial_predictive_bridge_set_bbb(financial_predictive_bridge_t* bridge,
                                         void* bbb);

/**
 * @brief Enable/disable BBB validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_predictive_bridge_enable_bbb_validation(
    financial_predictive_bridge_t* bridge, bool enable);

/**
 * @brief Enable/disable immune validation
 * @param bridge Bridge handle
 * @param enable True to enable validation
 * @return 0 on success
 */
int financial_predictive_bridge_enable_immune_validation(
    financial_predictive_bridge_t* bridge, bool enable);

/**
 * @brief Set KG wiring for inter-module communication
 * @param bridge Bridge handle
 * @param kg KG wiring handle
 * @return 0 on success
 */
int financial_predictive_bridge_set_kg_wiring(financial_predictive_bridge_t* bridge,
                                               void* kg);

/**
 * @brief Set health agent for heartbeat monitoring
 * @param bridge Bridge handle
 * @param health_agent Health agent handle
 * @return 0 on success
 */
int financial_predictive_bridge_set_health_agent(financial_predictive_bridge_t* bridge,
                                                  void* health_agent);

/**
 * @brief Set logger for debug/trace output
 * @param bridge Bridge handle
 * @param logger Logger handle
 * @return 0 on success
 */
int financial_predictive_bridge_set_logger(financial_predictive_bridge_t* bridge,
                                            void* logger);

//=============================================================================
// Core Predictive Coding API
//=============================================================================

/**
 * @brief Make predictions for future prices/returns
 *
 * Generates predictions based on current beliefs and generative model.
 * Prediction precision reflects confidence in each estimate.
 *
 * @param bridge Bridge handle
 * @param observation Current market observation
 * @param state Output predictive state (caller allocates)
 * @return 0 on success, error code on failure
 */
int financial_predictive_bridge_predict(
    financial_predictive_bridge_t* bridge,
    const fin_market_observation_t* observation,
    fin_predictive_state_t* state);

/**
 * @brief Update beliefs with new market observations
 *
 * Computes prediction errors and updates beliefs using precision-weighted
 * prediction error minimization.
 *
 * @param bridge Bridge handle
 * @param observation New market observation
 * @param state Current predictive state (updated in place)
 * @return 0 on success, error code on failure
 */
int financial_predictive_bridge_update(
    financial_predictive_bridge_t* bridge,
    const fin_market_observation_t* observation,
    fin_predictive_state_t* state);

/**
 * @brief Compute expected free energy for an action
 *
 * Evaluates the expected free energy G(π) for a given action, decomposed
 * into epistemic (information gain) and pragmatic (goal achievement) components.
 *
 * G(π) = E_Q[log Q(x|π) - log P(x,y|π)]
 *      = KL[Q(x|π) || P(x)] - E_Q[log P(y|x,π)]
 *      = Epistemic + Pragmatic
 *
 * @param bridge Bridge handle
 * @param action Action to evaluate
 * @param state Current predictive state
 * @param preferred Preferred outcome specification
 * @param result Output EFE result (caller allocates)
 * @return 0 on success, error code on failure
 */
int financial_predictive_bridge_expected_free_energy(
    financial_predictive_bridge_t* bridge,
    fin_action_type_t action,
    const fin_predictive_state_t* state,
    const fin_preferred_outcome_t* preferred,
    fin_efe_result_t* result);

/**
 * @brief Select optimal action via active inference
 *
 * Evaluates all possible actions and selects the one minimizing expected
 * free energy. Action probabilities are computed via softmax:
 * P(π) = softmax(-G(π) / temperature)
 *
 * @param bridge Bridge handle
 * @param state Current predictive state
 * @param preferred Preferred outcome specification
 * @param result Output active inference result (caller allocates)
 * @return 0 on success, error code on failure
 */
int financial_predictive_bridge_active_inference(
    financial_predictive_bridge_t* bridge,
    const fin_predictive_state_t* state,
    const fin_preferred_outcome_t* preferred,
    fin_active_inference_result_t* result);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Allocate predictive state structure
 * @param num_assets Number of assets
 * @param horizon Prediction horizon
 * @return Allocated state or NULL on error
 */
fin_predictive_state_t* financial_predictive_state_create(
    uint32_t num_assets, uint32_t horizon);

/**
 * @brief Free predictive state structure
 * @param state State to free
 */
void financial_predictive_state_destroy(fin_predictive_state_t* state);

/**
 * @brief Allocate active inference result structure
 * @param num_assets Number of assets
 * @param num_actions Number of actions to evaluate
 * @return Allocated result or NULL on error
 */
fin_active_inference_result_t* financial_predictive_result_create(
    uint32_t num_assets, uint32_t num_actions);

/**
 * @brief Free active inference result structure
 * @param result Result to free
 */
void financial_predictive_result_destroy(fin_active_inference_result_t* result);

//=============================================================================
// Precision Modulation API
//=============================================================================

/**
 * @brief Set inflammation level (reduces precision)
 * @param bridge Bridge handle
 * @param level Inflammation level [0-1]
 * @return 0 on success
 */
int financial_predictive_bridge_set_inflammation(
    financial_predictive_bridge_t* bridge, float level);

/**
 * @brief Set fatigue level (reduces precision)
 * @param bridge Bridge handle
 * @param level Fatigue level [0-1]
 * @return 0 on success
 */
int financial_predictive_bridge_set_fatigue(
    financial_predictive_bridge_t* bridge, float level);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics (caller allocates)
 * @return 0 on success
 */
int financial_predictive_bridge_get_stats(
    const financial_predictive_bridge_t* bridge,
    fin_predictive_bridge_stats_t* stats);

/**
 * @brief Reset statistics counters
 * @param bridge Bridge handle
 */
void financial_predictive_bridge_reset_stats(financial_predictive_bridge_t* bridge);

/**
 * @brief Get last error message
 * @return Thread-local error message string
 */
const char* financial_predictive_bridge_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_PREDICTIVE_BRIDGE_H */
