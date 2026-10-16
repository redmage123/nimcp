/**
 * @file nimcp_financial_basal_ganglia_bridge.h
 * @brief Financial Basal Ganglia Bridge - Decision Systems (Phase 6)
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling action selection and reinforcement learning in
 *       financial decision-making, following the basal ganglia's direct and
 *       indirect pathway model for action selection vs suppression.
 *
 * WHY:  Financial trading requires action selection under uncertainty.
 *       The basal ganglia model provides a neurobiologically-grounded
 *       framework for:
 *       - ACTION EVALUATION: Computing Q-values for all possible actions
 *       - ACTION SELECTION:  Softmax selection with exploration/exploitation
 *       - TEMPORAL CREDIT:   Long-term attribution for delayed rewards
 *       - HABIT DETECTION:   Distinguishing habitual from goal-directed actions
 *       This enables adaptive decision-making that balances speed (habits)
 *       with flexibility (goal-directed reasoning).
 *
 * HOW:  Actions are evaluated using Q-learning with temporal difference.
 *       Selection uses softmax with temperature (beta oscillation controls
 *       "paralysis by analysis"). Outcomes update Q-values via TD error.
 *       Habit formation is tracked by action repetition patterns.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Basal Ganglia Bridge (Decision Systems)            |
 * +===========================================================================+
 * |                                                                           |
 * |  +-------------------------+       +-------------------------+            |
 * |  |  Market State Input     |       |  Position State         |            |
 * |  +-------------------------+       +-------------------------+            |
 * |  | prices, volatility      |       | holdings, exposure      |            |
 * |  | momentum, regime        |       | profit/loss, risk       |            |
 * |  +------------+------------+       +------------+------------+            |
 * |               |                                 |                         |
 * |               v                                 v                         |
 * |  +-----------------------------------------------------------+           |
 * |  |                Action Evaluation (Q-Learning)              |           |
 * |  |  For each action: Q(s,a) = r + gamma * max_a' Q(s',a')    |           |
 * |  +-----------------------------------------------------------+           |
 * |               |                                                           |
 * |               v                                                           |
 * |  +-----------------------------------------------------------+           |
 * |  |           Action Selection (Softmax/Greedy)                |           |
 * |  |  P(a) = exp(Q(a)/tau) / sum(exp(Q/tau))                   |           |
 * |  |  beta_oscillation controls tau (high beta -> indecision)   |           |
 * |  +-----------------------------------------------------------+           |
 * |               |                                                           |
 * |               v                                                           |
 * |  +-----------------------------------------------------------+           |
 * |  |           Outcome Update (TD Learning)                     |           |
 * |  |  delta = r + gamma*Q(s',a*) - Q(s,a)                       |           |
 * |  |  Q(s,a) += alpha * delta                                   |           |
 * |  +-----------------------------------------------------------+           |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * BASAL GANGLIA MODEL:
 * - Striatum:    Receives cortical input, encodes action values (Q-values)
 * - Direct:      Go pathway - promotes selected action (D1 receptors)
 * - Indirect:    NoGo pathway - suppresses competing actions (D2 receptors)
 * - SNc/VTA:     Dopamine signals reward prediction error (TD error)
 * - GPe/STN:     Hyperdirect pathway for fast stopping (decision conflicts)
 * - Thalamus:    Output gating back to cortex for action execution
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_financial_motivation_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_BASAL_GANGLIA_BRIDGE_H
#define NIMCP_FINANCIAL_BASAL_GANGLIA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_BG_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_BG_BRIDGE_MAGIC      0x46424742  /* 'FBGB' */

/** Bio-async module ID for financial basal ganglia bridge */
#define BIO_MODULE_FINANCIAL_BG        0x0399

/** Maximum actions that can be tracked */
#define FIN_BG_MAX_ACTIONS             32

/** Maximum description/label length */
#define FIN_BG_DESC_LEN                128

/** Default number of state features for Q-learning */
#define FIN_BG_DEFAULT_STATE_DIM       16

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_BG_ERROR_BASE              33300
#define FIN_BG_ERR_OK                  0
#define FIN_BG_ERR_NULL                (FIN_BG_ERROR_BASE + 1)
#define FIN_BG_ERR_INVALID_PARAM       (FIN_BG_ERROR_BASE + 2)
#define FIN_BG_ERR_NO_MEMORY           (FIN_BG_ERROR_BASE + 3)
#define FIN_BG_ERR_STATE               (FIN_BG_ERROR_BASE + 4)
#define FIN_BG_ERR_IMMUNE              (FIN_BG_ERROR_BASE + 5)
#define FIN_BG_ERR_BBB                 (FIN_BG_ERROR_BASE + 6)
#define FIN_BG_ERR_SUBSYSTEM           (FIN_BG_ERROR_BASE + 7)
#define FIN_BG_ERR_COMPUTE             (FIN_BG_ERROR_BASE + 8)
#define FIN_BG_ERR_NO_ACTIONS          (FIN_BG_ERROR_BASE + 9)
#define FIN_BG_ERR_CONFLICT            (FIN_BG_ERROR_BASE + 10)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_BG_OP_STATE_UNINITIALIZED = 0,
    FIN_BG_OP_STATE_INITIALIZED,
    FIN_BG_OP_STATE_ACTIVE,
    FIN_BG_OP_STATE_DEGRADED,
    FIN_BG_OP_STATE_ERROR
} fin_bg_op_state_t;

/**
 * @brief Financial trading action types
 *
 * Represents the core action space for trading decisions.
 */
typedef enum {
    FIN_BG_ACTION_ENTER_LONG = 0,   /**< Enter long position (buy) */
    FIN_BG_ACTION_ENTER_SHORT,      /**< Enter short position (sell) */
    FIN_BG_ACTION_EXIT_POSITION,    /**< Exit current position */
    FIN_BG_ACTION_SCALE_IN,         /**< Add to existing position */
    FIN_BG_ACTION_SCALE_OUT,        /**< Reduce existing position */
    FIN_BG_ACTION_HOLD,             /**< Hold current position, no action */
    FIN_BG_ACTION_REBALANCE,        /**< Rebalance portfolio allocations */
    FIN_BG_ACTION_HEDGE,            /**< Add hedging position */
    FIN_BG_ACTION_COUNT             /**< Number of action types */
} fin_bg_action_t;

/* ============================================================================
 * Core Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Action value representation with Q-learning components
 *
 * Represents the evaluated value and properties of a single action.
 */
typedef struct {
    fin_bg_action_t action;         /**< Action type */
    float q_value;                  /**< Expected value (Q-value) */
    float probability;              /**< Selection probability (softmax) */
    bool is_habitual;               /**< Habit vs goal-directed flag */
    float temporal_credit;          /**< Long-term attribution strength */
} fin_bg_action_value_t;

/**
 * @brief Decision result containing evaluated actions and selection
 *
 * The complete output of the action evaluation and selection process.
 */
typedef struct {
    fin_bg_action_value_t* actions; /**< Array of action values */
    uint32_t num_actions;           /**< Number of actions evaluated */
    fin_bg_action_t selected;       /**< Selected action */
    float beta_oscillation;         /**< Paralysis by analysis indicator [0,1] */
    bool decision_conflict;         /**< Multiple high-value options detected */
} fin_bg_decision_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t action_evaluations;    /**< Total action evaluation calls */
    uint64_t action_selections;     /**< Total action selections made */
    uint64_t outcome_updates;       /**< Q-value updates from outcomes */
    uint64_t habit_detections;      /**< Habitual action detections */
    uint64_t conflicts_detected;    /**< Decision conflict events */
    uint64_t immune_checks;         /**< Immune system checks performed */
    uint64_t bbb_validations;       /**< BBB validations performed */
    uint64_t kg_messages_sent;      /**< KG messages published */
    uint64_t health_heartbeats;     /**< Health heartbeats sent */
} fin_bg_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Market state representation for action evaluation
 */
typedef struct {
    float price_change;             /**< Recent price change [-1,1] normalized */
    float volatility;               /**< Current volatility [0,1] */
    float momentum;                 /**< Price momentum [-1,1] */
    float regime_confidence;        /**< Regime detection confidence [0,1] */
    float sentiment;                /**< Market sentiment [-1,1] */
    float spread;                   /**< Bid-ask spread [0,1] normalized */
    uint64_t timestamp_ms;          /**< State timestamp */
} fin_bg_market_state_t;

/**
 * @brief Position state representation
 */
typedef struct {
    float position_size;            /**< Current position size [-1,1] (neg=short) */
    float unrealized_pnl;           /**< Unrealized P&L as fraction of capital */
    float realized_pnl;             /**< Realized P&L as fraction of capital */
    float exposure;                 /**< Total market exposure [0,1] */
    float avg_entry_price;          /**< Average entry price */
    uint32_t hold_duration_ms;      /**< Time in current position */
} fin_bg_position_state_t;

/**
 * @brief Combined state for Q-learning
 */
typedef struct {
    fin_bg_market_state_t market;   /**< Market state component */
    fin_bg_position_state_t position; /**< Position state component */
} fin_bg_state_t;

/**
 * @brief Outcome feedback for TD learning update
 */
typedef struct {
    fin_bg_action_t action_taken;   /**< Action that was executed */
    float reward;                   /**< Immediate reward signal */
    fin_bg_state_t next_state;      /**< Resulting state after action */
    bool terminal;                  /**< Episode terminal flag */
    uint64_t timestamp_ms;          /**< Outcome timestamp */
} fin_bg_outcome_t;

/**
 * @brief Habit analysis result
 */
typedef struct {
    bool is_habitual;               /**< Whether action is habitual */
    float habit_strength;           /**< Strength of habit [0,1] */
    uint32_t repetition_count;      /**< Consecutive repetitions */
    float goal_directed_score;      /**< Goal-directed evaluation score */
    char description[FIN_BG_DESC_LEN]; /**< Habit description */
} fin_bg_habit_result_t;

/**
 * @brief Conflict analysis result
 */
typedef struct {
    bool has_conflict;              /**< Whether conflict was detected */
    fin_bg_action_t action1;        /**< First conflicting action */
    fin_bg_action_t action2;        /**< Second conflicting action */
    float value_gap;                /**< Gap between top values */
    float conflict_intensity;       /**< Conflict intensity [0,1] */
    char resolution[FIN_BG_DESC_LEN]; /**< Suggested resolution */
} fin_bg_conflict_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Q-learning parameters */
    float learning_rate;            /**< TD learning rate alpha [0.0-1.0] */
    float discount_factor;          /**< Future reward discount gamma [0.0-1.0] */
    float eligibility_decay;        /**< Eligibility trace decay lambda [0.0-1.0] */

    /* Action selection parameters */
    float temperature;              /**< Softmax temperature tau (higher = more exploration) */
    float epsilon;                  /**< Epsilon for epsilon-greedy [0.0-1.0] */
    bool use_softmax;               /**< Use softmax (true) or epsilon-greedy (false) */

    /* Beta oscillation (indecision) parameters */
    float beta_threshold;           /**< Beta level triggering indecision [0.0-1.0] */
    float conflict_threshold;       /**< Q-value gap for conflict detection */

    /* Habit formation parameters */
    uint32_t habit_threshold_reps;  /**< Repetitions to consider habitual */
    float habit_decay_rate;         /**< Rate at which habit strength decays */

    /* Temporal credit assignment */
    uint32_t credit_window_ms;      /**< Time window for temporal credit */
    float credit_decay;             /**< Temporal credit decay factor */

    /* Integration settings */
    bool enable_immune_integration; /**< Enable immune system checks */
    bool enable_bbb_validation;     /**< Enable BBB validation */
    bool enable_kg_messaging;       /**< Enable KG messaging */
    bool enable_health_monitoring;  /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;           /**< Verbose debug output */
} fin_bg_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Action selected callback
 */
typedef void (*fin_bg_action_callback_t)(
    const fin_bg_decision_t* decision,
    void* user_data
);

/**
 * @brief Conflict detected callback
 */
typedef void (*fin_bg_conflict_callback_t)(
    const fin_bg_conflict_result_t* conflict,
    void* user_data
);

/**
 * @brief Habit detected callback
 */
typedef void (*fin_bg_habit_callback_t)(
    fin_bg_action_t action,
    const fin_bg_habit_result_t* habit,
    void* user_data
);

/* ============================================================================
 * Forward Declarations for Subsystems
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
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial basal ganglia bridge handle
 */
typedef struct financial_bg_bridge financial_bg_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_default_config(fin_bg_config_t* config);

/**
 * @brief Create financial basal ganglia bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_bg_bridge_t* financial_bg_bridge_create(
    const fin_bg_config_t* config
);

/**
 * @brief Destroy financial basal ganglia bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_bg_bridge_destroy(financial_bg_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_reset(financial_bg_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_bg_bridge_set_immune(
    financial_bg_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_bg_bridge_set_bbb(
    financial_bg_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_bg_bridge_set_health_agent(
    financial_bg_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_bg_bridge_set_kg_wiring(
    financial_bg_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_bg_bridge_set_logger(
    financial_bg_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_bg_bridge_set_security(
    financial_bg_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_bg_bridge_set_ethics(
    financial_bg_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_bg_bridge_set_lgss(
    financial_bg_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_bg_bridge_set_coordinator(
    financial_bg_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_bg_bridge_set_bio_router(
    financial_bg_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Action Evaluation & Selection
 * ============================================================================ */

/**
 * @brief Evaluate all possible actions for current state
 *
 * Computes Q-values, selection probabilities, and habit flags for all actions.
 * Does NOT select an action - call financial_bg_bridge_select_action() for that.
 *
 * @param bridge Bridge handle
 * @param state Current market and position state
 * @param out_decision Output decision structure (actions array must be pre-allocated)
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_evaluate_actions(
    financial_bg_bridge_t* bridge,
    const fin_bg_state_t* state,
    fin_bg_decision_t* out_decision
);

/**
 * @brief Select best action from evaluated decision
 *
 * Uses softmax or epsilon-greedy selection based on configuration.
 * Updates the decision's selected field and detects conflicts.
 *
 * @param bridge Bridge handle
 * @param decision Decision with evaluated actions (updated with selection)
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_select_action(
    financial_bg_bridge_t* bridge,
    fin_bg_decision_t* decision
);

/**
 * @brief Update Q-values from observed outcome (TD learning)
 *
 * Implements temporal difference learning: delta = r + gamma*Q(s',a*) - Q(s,a)
 * Updates eligibility traces and temporal credit assignment.
 *
 * @param bridge Bridge handle
 * @param prev_state State before action
 * @param outcome Outcome containing action, reward, and next state
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_update_from_outcome(
    financial_bg_bridge_t* bridge,
    const fin_bg_state_t* prev_state,
    const fin_bg_outcome_t* outcome
);

/**
 * @brief Test if action is goal-directed vs habitual
 *
 * Analyzes action repetition patterns and value-based selection to
 * determine if the action is habitual (automatic) or goal-directed (deliberate).
 *
 * @param bridge Bridge handle
 * @param action Action to test
 * @param state Current state context
 * @param out_result Output habit analysis result
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_test_goal_directed(
    financial_bg_bridge_t* bridge,
    fin_bg_action_t action,
    const fin_bg_state_t* state,
    fin_bg_habit_result_t* out_result
);

/* ============================================================================
 * Extended API
 * ============================================================================ */

/**
 * @brief Get current TD error (reward prediction error)
 *
 * @param bridge Bridge handle
 * @param out_td_error Output TD error
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_get_td_error(
    financial_bg_bridge_t* bridge,
    float* out_td_error
);

/**
 * @brief Get Q-value for specific state-action pair
 *
 * @param bridge Bridge handle
 * @param state State to query
 * @param action Action to query
 * @param out_q_value Output Q-value
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_get_q_value(
    financial_bg_bridge_t* bridge,
    const fin_bg_state_t* state,
    fin_bg_action_t action,
    float* out_q_value
);

/**
 * @brief Analyze decision conflict
 *
 * @param bridge Bridge handle
 * @param decision Decision to analyze
 * @param out_conflict Output conflict result
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_analyze_conflict(
    financial_bg_bridge_t* bridge,
    const fin_bg_decision_t* decision,
    fin_bg_conflict_result_t* out_conflict
);

/**
 * @brief Set exploration temperature
 *
 * @param bridge Bridge handle
 * @param temperature New temperature value
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_set_temperature(
    financial_bg_bridge_t* bridge,
    float temperature
);

/**
 * @brief Get current beta oscillation level
 *
 * @param bridge Bridge handle
 * @param out_beta Output beta level
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_get_beta_oscillation(
    financial_bg_bridge_t* bridge,
    float* out_beta
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set action selected callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_set_action_callback(
    financial_bg_bridge_t* bridge,
    fin_bg_action_callback_t callback,
    void* user_data
);

/**
 * @brief Set conflict detected callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_set_conflict_callback(
    financial_bg_bridge_t* bridge,
    fin_bg_conflict_callback_t callback,
    void* user_data
);

/**
 * @brief Set habit detected callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_set_habit_callback(
    financial_bg_bridge_t* bridge,
    fin_bg_habit_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Operational state enum
 */
fin_bg_op_state_t financial_bg_bridge_get_op_state(
    const financial_bg_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_bg_bridge_get_stats(
    const financial_bg_bridge_t* bridge,
    fin_bg_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_bg_bridge_reset_stats(financial_bg_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_bg_bridge_get_last_error(void);

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
int financial_bg_bridge_heartbeat(
    financial_bg_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_bg_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Decision Memory API
 * ============================================================================ */

/**
 * @brief Allocate action array for decision
 *
 * @param num_actions Number of actions to allocate
 * @return Allocated array or NULL on failure
 */
fin_bg_action_value_t* financial_bg_alloc_actions(uint32_t num_actions);

/**
 * @brief Free action array
 *
 * @param actions Array to free (NULL safe)
 */
void financial_bg_free_actions(fin_bg_action_value_t* actions);

/**
 * @brief Initialize decision structure
 *
 * @param decision Decision to initialize
 * @param num_actions Number of actions to support
 * @return 0 on success, error code on failure
 */
int financial_bg_init_decision(fin_bg_decision_t* decision, uint32_t num_actions);

/**
 * @brief Clean up decision structure
 *
 * @param decision Decision to clean up
 */
void financial_bg_cleanup_decision(fin_bg_decision_t* decision);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return String name (static)
 */
const char* fin_bg_action_name(fin_bg_action_t action);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_bg_op_state_name(fin_bg_op_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_bg_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_BASAL_GANGLIA_BRIDGE_H */
