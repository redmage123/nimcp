/**
 * @file nimcp_financial_world_model_bridge.h
 * @brief Financial World Model Bridge for Market State Prediction
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for maintaining and predicting financial market world states
 *       including counterfactual analysis and policy rollout simulation
 *
 * WHY:  Effective trading requires mental models of market dynamics that can:
 *       - Track current market state across multiple assets
 *       - Predict forward market trajectories under different scenarios
 *       - Evaluate counterfactual "what-if" scenarios
 *       - Roll out trading policies to estimate expected performance
 *
 * HOW:  Maintains a world state representation (prices, volatilities, regime)
 *       and provides prediction, counterfactual, and rollout operations.
 *       Integrates with immune system, BBB validation, KG messaging, and
 *       health monitoring for enterprise-grade reliability.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Financial World Model Bridge                            |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Market Data Feed     |       |  World State Manager  |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Price Updates         |       | Asset Prices          |                |
 * |  | Volatility Estimates  |       | Volatilities          |                |
 * |  | Regime Indicators     |       | Market Regime         |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |              Prediction Engine                           |            |
 * |  |  state -> forward_model -> predicted_state_trajectory    |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +---------------------------+  +---------------------------+            |
 * |  |   Counterfactual Engine   |  |    Policy Rollout Engine  |            |
 * |  | "what if X happened?"     |  | "what if I do action A?"  |            |
 * |  +---------------------------+  +---------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_financial_autobio_bridge.h
 * @see nimcp_parietal_training_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_WORLD_MODEL_BRIDGE_H
#define NIMCP_FINANCIAL_WORLD_MODEL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_WORLD_MODEL_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_WORLD_MODEL_BRIDGE_MAGIC      0x46574D42  /* 'FWMB' */

/** Bio-async module ID for financial world model bridge */
#define BIO_MODULE_FINANCIAL_WORLD_MODEL        0x0397

/** Maximum assets tracked in world state */
#define FIN_WORLD_MAX_ASSETS                    256

/** Maximum trajectory length for predictions */
#define FIN_WORLD_MAX_TRAJECTORY                1024

/** Maximum counterfactual scenarios */
#define FIN_WORLD_MAX_COUNTERFACTUALS           64

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_WORLD_ERROR_BASE                    33200
#define FIN_WORLD_ERR_OK                        0
#define FIN_WORLD_ERR_NULL                      (FIN_WORLD_ERROR_BASE + 1)
#define FIN_WORLD_ERR_INVALID_PARAM             (FIN_WORLD_ERROR_BASE + 2)
#define FIN_WORLD_ERR_NO_MEMORY                 (FIN_WORLD_ERROR_BASE + 3)
#define FIN_WORLD_ERR_NOT_FOUND                 (FIN_WORLD_ERROR_BASE + 4)
#define FIN_WORLD_ERR_CAPACITY                  (FIN_WORLD_ERROR_BASE + 5)
#define FIN_WORLD_ERR_STATE                     (FIN_WORLD_ERROR_BASE + 6)
#define FIN_WORLD_ERR_IMMUNE                    (FIN_WORLD_ERROR_BASE + 7)
#define FIN_WORLD_ERR_BBB                       (FIN_WORLD_ERROR_BASE + 8)
#define FIN_WORLD_ERR_PREDICTION                (FIN_WORLD_ERROR_BASE + 9)
#define FIN_WORLD_ERR_ROLLOUT                   (FIN_WORLD_ERROR_BASE + 10)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Market regime classification
 *
 * Markets cycle through different regimes with distinct statistical properties.
 * Regime detection influences prediction models and risk parameters.
 */
typedef enum {
    FIN_REGIME_BULL = 0,        /**< Upward trending market */
    FIN_REGIME_BEAR,            /**< Downward trending market */
    FIN_REGIME_SIDEWAYS,        /**< Range-bound, low trend market */
    FIN_REGIME_CRISIS,          /**< High volatility, regime breakdown */
    FIN_REGIME_COUNT
} fin_market_regime_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_WORLD_STATE_UNINITIALIZED = 0,
    FIN_WORLD_STATE_INITIALIZED,
    FIN_WORLD_STATE_ACTIVE,
    FIN_WORLD_STATE_DEGRADED,
    FIN_WORLD_STATE_ERROR
} fin_world_bridge_state_t;

/**
 * @brief Prediction model type
 */
typedef enum {
    FIN_PRED_MODEL_RANDOM_WALK = 0,   /**< Simple random walk */
    FIN_PRED_MODEL_MEAN_REVERT,       /**< Mean-reverting (Ornstein-Uhlenbeck) */
    FIN_PRED_MODEL_MOMENTUM,          /**< Trend-following momentum */
    FIN_PRED_MODEL_REGIME_SWITCH,     /**< Regime-switching model */
    FIN_PRED_MODEL_COUNT
} fin_prediction_model_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Financial world state snapshot
 *
 * Captures complete market state at a point in time, including prices,
 * volatilities, and regime classification.
 */
typedef struct {
    float* asset_prices;            /**< Array of asset prices */
    float* volatilities;            /**< Array of asset volatilities */
    int regime;                     /**< Current market regime (fin_market_regime_t) */
    uint32_t num_assets;            /**< Number of assets tracked */
    uint64_t timestamp_ms;          /**< State timestamp in milliseconds */
} fin_world_state_t;

/**
 * @brief Counterfactual analysis result
 *
 * Contains the trajectory resulting from a counterfactual scenario,
 * including cumulative returns, expected Sharpe ratio, and probability.
 */
typedef struct {
    fin_world_state_t* trajectory;  /**< Array of predicted states */
    uint32_t trajectory_len;        /**< Number of states in trajectory */
    float* cumulative_returns;      /**< Cumulative returns at each step */
    float expected_sharpe;          /**< Expected annualized Sharpe ratio */
    float probability;              /**< Probability of this scenario [0.0-1.0] */
} fin_counterfactual_result_t;

/**
 * @brief Policy rollout action
 */
typedef struct {
    uint32_t asset_index;           /**< Asset to act on */
    float position_delta;           /**< Position change (-1.0 to 1.0 normalized) */
    float stop_loss;                /**< Stop loss level (0 = none) */
    float take_profit;              /**< Take profit level (0 = none) */
} fin_policy_action_t;

/**
 * @brief Policy rollout result
 */
typedef struct {
    fin_world_state_t* trajectory;  /**< Trajectory under policy */
    uint32_t trajectory_len;        /**< Number of states */
    float* portfolio_values;        /**< Portfolio value at each step */
    float final_pnl;                /**< Final P&L */
    float max_drawdown;             /**< Maximum drawdown during rollout */
    float expected_sharpe;          /**< Expected Sharpe ratio */
    uint32_t num_trades;            /**< Number of trades executed */
} fin_rollout_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t state_updates;         /**< Total state updates received */
    uint64_t predictions;           /**< Total predictions made */
    uint64_t counterfactuals;       /**< Counterfactual analyses run */
    uint64_t rollouts;              /**< Policy rollouts executed */
    uint64_t immune_checks;         /**< Immune system checks */
    uint64_t bbb_validations;       /**< BBB validations performed */
    uint64_t kg_messages_sent;      /**< KG messages published */
    uint64_t health_heartbeats;     /**< Health heartbeats sent */
} fin_world_model_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Model settings */
    uint32_t max_assets;                    /**< Maximum assets to track */
    uint32_t default_trajectory_len;        /**< Default prediction horizon */
    fin_prediction_model_t default_model;   /**< Default prediction model */

    /* Simulation parameters */
    float volatility_scaling;               /**< Volatility scaling factor */
    float mean_reversion_rate;              /**< Mean reversion rate (if applicable) */
    uint32_t monte_carlo_samples;           /**< Monte Carlo sample count */

    /* Integration settings */
    bool enable_immune_integration;         /**< Enable immune system */
    bool enable_bbb_validation;             /**< Enable BBB validation */
    bool enable_kg_messaging;               /**< Enable KG messaging */
    bool enable_health_monitoring;          /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;                   /**< Verbose debug output */
} fin_world_model_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief State update callback
 */
typedef void (*fin_world_state_callback_t)(
    const fin_world_state_t* state,
    void* user_data
);

/**
 * @brief Prediction complete callback
 */
typedef void (*fin_world_prediction_callback_t)(
    const fin_world_state_t* predicted_states,
    uint32_t num_states,
    void* user_data
);

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial world model bridge handle
 */
typedef struct financial_world_model_bridge financial_world_model_bridge_t;

/* ============================================================================
 * Forward Declarations for Security Subsystems
 * ============================================================================ */

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef ETHICS_ENGINE_T_DEFINED
#define ETHICS_ENGINE_T_DEFINED
typedef struct ethics_engine_struct* ethics_engine_t;
#endif

#ifndef BRAIN_CYCLE_COORDINATOR_T_DEFINED
#define BRAIN_CYCLE_COORDINATOR_T_DEFINED
typedef struct brain_cycle_coordinator brain_cycle_coordinator_t;
#endif

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_world_model_bridge_default_config(fin_world_model_config_t* config);

/**
 * @brief Create financial world model bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_world_model_bridge_t* financial_world_model_bridge_create(
    const fin_world_model_config_t* config
);

/**
 * @brief Destroy financial world model bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_world_model_bridge_destroy(financial_world_model_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_reset(financial_world_model_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_world_model_bridge_set_immune(
    financial_world_model_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_world_model_bridge_set_bbb(
    financial_world_model_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_world_model_bridge_set_health_agent(
    financial_world_model_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_world_model_bridge_set_kg_wiring(
    financial_world_model_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_world_model_bridge_set_logger(
    financial_world_model_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_world_model_bridge_set_security(
    financial_world_model_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_world_model_bridge_set_ethics(
    financial_world_model_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_world_model_bridge_set_lgss(
    financial_world_model_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_world_model_bridge_set_coordinator(
    financial_world_model_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_world_model_bridge_set_bio_router(
    financial_world_model_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * World State API
 * ============================================================================ */

/**
 * @brief Set current world state
 *
 * Updates the internal world state from external market data.
 *
 * @param bridge Bridge handle
 * @param state New world state to set
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_set_state(
    financial_world_model_bridge_t* bridge,
    const fin_world_state_t* state
);

/**
 * @brief Get current world state
 *
 * @param bridge Bridge handle
 * @param state Output state (caller allocates arrays)
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_get_state(
    const financial_world_model_bridge_t* bridge,
    fin_world_state_t* state
);

/**
 * @brief Update specific asset price
 *
 * @param bridge Bridge handle
 * @param asset_index Index of asset to update
 * @param price New price
 * @param volatility New volatility estimate
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_update_asset(
    financial_world_model_bridge_t* bridge,
    uint32_t asset_index,
    float price,
    float volatility
);

/**
 * @brief Set market regime
 *
 * @param bridge Bridge handle
 * @param regime New market regime
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_set_regime(
    financial_world_model_bridge_t* bridge,
    fin_market_regime_t regime
);

/* ============================================================================
 * Prediction API
 * ============================================================================ */

/**
 * @brief Predict forward from current state
 *
 * Generates a trajectory of predicted future states using the configured
 * prediction model.
 *
 * @param bridge Bridge handle
 * @param horizon Number of steps to predict forward
 * @param model Prediction model to use
 * @param trajectory Output array (caller allocates, must have horizon capacity)
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_predict_forward(
    financial_world_model_bridge_t* bridge,
    uint32_t horizon,
    fin_prediction_model_t model,
    fin_world_state_t* trajectory
);

/**
 * @brief Predict forward with Monte Carlo sampling
 *
 * Returns multiple possible trajectories with associated probabilities.
 *
 * @param bridge Bridge handle
 * @param horizon Steps to predict
 * @param num_samples Number of Monte Carlo samples
 * @param trajectories Output array of trajectories (caller allocates)
 * @param probabilities Output array of probabilities (caller allocates)
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_predict_monte_carlo(
    financial_world_model_bridge_t* bridge,
    uint32_t horizon,
    uint32_t num_samples,
    fin_world_state_t** trajectories,
    float* probabilities
);

/* ============================================================================
 * Counterfactual API
 * ============================================================================ */

/**
 * @brief Run counterfactual analysis
 *
 * Evaluates "what if?" scenarios by perturbing the world state and
 * predicting forward.
 *
 * @param bridge Bridge handle
 * @param initial_perturbation Perturbation to apply to current state
 * @param horizon Prediction horizon
 * @param result Output counterfactual result (caller frees with free_counterfactual)
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_counterfactual(
    financial_world_model_bridge_t* bridge,
    const fin_world_state_t* initial_perturbation,
    uint32_t horizon,
    fin_counterfactual_result_t* result
);

/**
 * @brief Free counterfactual result resources
 *
 * @param result Result to free
 */
void financial_world_model_bridge_free_counterfactual(
    fin_counterfactual_result_t* result
);

/* ============================================================================
 * Policy Rollout API
 * ============================================================================ */

/**
 * @brief Roll out a trading policy
 *
 * Simulates executing a sequence of actions and returns the resulting
 * portfolio trajectory and performance metrics.
 *
 * @param bridge Bridge handle
 * @param actions Array of actions to execute
 * @param num_actions Number of actions
 * @param initial_capital Starting capital
 * @param result Output rollout result (caller frees with free_rollout)
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_rollout_policy(
    financial_world_model_bridge_t* bridge,
    const fin_policy_action_t* actions,
    uint32_t num_actions,
    float initial_capital,
    fin_rollout_result_t* result
);

/**
 * @brief Free rollout result resources
 *
 * @param result Result to free
 */
void financial_world_model_bridge_free_rollout(fin_rollout_result_t* result);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set state update callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_set_state_callback(
    financial_world_model_bridge_t* bridge,
    fin_world_state_callback_t callback,
    void* user_data
);

/**
 * @brief Set prediction callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_set_prediction_callback(
    financial_world_model_bridge_t* bridge,
    fin_world_prediction_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_world_bridge_state_t financial_world_model_bridge_get_bridge_state(
    const financial_world_model_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_get_stats(
    const financial_world_model_bridge_t* bridge,
    fin_world_model_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_world_model_bridge_reset_stats(
    financial_world_model_bridge_t* bridge
);

/**
 * @brief Get number of assets currently tracked
 *
 * @param bridge Bridge handle
 * @return Number of assets
 */
uint32_t financial_world_model_bridge_get_num_assets(
    const financial_world_model_bridge_t* bridge
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_world_model_bridge_get_last_error(void);

/* ============================================================================
 * Health Integration
 * ============================================================================ */

/**
 * @brief Send heartbeat
 *
 * @param bridge Bridge handle
 * @param operation Current operation
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_world_model_bridge_heartbeat(
    financial_world_model_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Training Hooks (B23 Upgrade Compatibility)
 * ============================================================================ */

/**
 * @brief Begin training phase
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int financial_world_model_bridge_training_begin(
    financial_world_model_bridge_t* bridge
);

/**
 * @brief End training phase
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int financial_world_model_bridge_training_end(
    financial_world_model_bridge_t* bridge
);

/**
 * @brief Training step with progress
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int financial_world_model_bridge_training_step(
    financial_world_model_bridge_t* bridge,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get regime name
 *
 * @param regime Market regime
 * @return String name (static)
 */
const char* fin_world_regime_name(fin_market_regime_t regime);

/**
 * @brief Get bridge state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_world_bridge_state_name(fin_world_bridge_state_t state);

/**
 * @brief Get prediction model name
 *
 * @param model Prediction model
 * @return String name (static)
 */
const char* fin_world_model_name(fin_prediction_model_t model);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_world_model_bridge_version(void);

/**
 * @brief Allocate world state arrays
 *
 * Convenience function to allocate asset_prices and volatilities arrays.
 *
 * @param state State to allocate for
 * @param num_assets Number of assets
 * @return 0 on success, -1 on allocation failure
 */
int fin_world_state_alloc(fin_world_state_t* state, uint32_t num_assets);

/**
 * @brief Free world state arrays
 *
 * @param state State to free
 */
void fin_world_state_free(fin_world_state_t* state);

/**
 * @brief Copy world state
 *
 * @param dst Destination state (must have arrays allocated)
 * @param src Source state
 * @return 0 on success, -1 on error
 */
int fin_world_state_copy(fin_world_state_t* dst, const fin_world_state_t* src);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_WORLD_MODEL_BRIDGE_H */
