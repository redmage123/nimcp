/**
 * @file nimcp_financial_temporal_credit_bridge.h
 * @brief Financial Temporal Credit Assignment Bridge - Learning Credit Attribution
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for temporal credit assignment in financial decision-making,
 *       attributing outcomes to past decisions through eligibility traces
 *       and credit propagation mechanisms.
 *
 * WHY:  In trading and investment, decisions made minutes/hours/days ago
 *       contribute to current outcomes. This bridge:
 *       - Assigns credit to historical decisions based on final outcomes
 *       - Maintains eligibility traces that decay over time
 *       - Supports TD-lambda style credit propagation
 *       - Enables learning from delayed rewards/punishments
 *       This implements the credit assignment problem solution for financial RL.
 *
 * HOW:  Decision history is maintained with timestamps and eligibility.
 *       When an outcome is observed:
 *       - Compute eligibility trace for each past decision
 *       - Assign credit proportional to eligibility and causality
 *       - Update learning signals for pattern strengthening/weakening
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Temporal Credit Bridge                              |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Decision History     |       |  Eligibility Traces   |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Type, magnitude       |       | Lambda decay          |                |
 * |  | Timestamp             |       | Trace values          |                |
 * |  | Initial eligibility   |       | Causal weights        |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Credit Assignment Engine                        |            |
 * |  |  outcome × eligibility × causality → credit per decision |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          Credit Assignment Result                        |            |
 * |  |  - Per-decision credit scores                            |            |
 * |  |  - Learning signals for pattern updates                  |            |
 * |  |  - Propagated error signals                              |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * LEARNING INTEGRATION:
 * - TD(lambda): Temporal difference with eligibility traces
 * - Q-learning: State-action value updates
 * - Policy gradient: Credit for policy parameters
 * - Actor-critic: Separate value and policy credit
 *
 * @see nimcp_financial_consolidation_bridge.h
 * @see nimcp_financial_basal_ganglia_bridge.h
 * @see nimcp_parietal_training_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_TEMPORAL_CREDIT_BRIDGE_H
#define NIMCP_FINANCIAL_TEMPORAL_CREDIT_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_TEMPORAL_CREDIT_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_TEMPORAL_CREDIT_BRIDGE_MAGIC      0x46544342  /* 'FTCB' */

/** Bio-async module ID for financial temporal credit bridge */
#define BIO_MODULE_FINANCIAL_TEMPORAL_CREDIT        0x039C

/** Maximum decisions in history buffer */
#define FIN_TEMPORAL_CREDIT_MAX_DECISIONS           2048

/** Default eligibility trace decay (lambda) */
#define FIN_TEMPORAL_CREDIT_DEFAULT_LAMBDA          0.9f

/** Default discount factor (gamma) */
#define FIN_TEMPORAL_CREDIT_DEFAULT_GAMMA           0.99f

/** Default learning rate for credit-based updates */
#define FIN_TEMPORAL_CREDIT_DEFAULT_LR              0.01f

/** Default eligibility trace threshold (below which trace is zeroed) */
#define FIN_TEMPORAL_CREDIT_DEFAULT_TRACE_THRESHOLD 0.001f

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_TEMPORAL_CREDIT_ERROR_BASE              33600
#define FIN_TEMPORAL_CREDIT_ERR_OK                  0
#define FIN_TEMPORAL_CREDIT_ERR_NULL                (FIN_TEMPORAL_CREDIT_ERROR_BASE + 1)
#define FIN_TEMPORAL_CREDIT_ERR_INVALID_PARAM       (FIN_TEMPORAL_CREDIT_ERROR_BASE + 2)
#define FIN_TEMPORAL_CREDIT_ERR_NO_MEMORY           (FIN_TEMPORAL_CREDIT_ERROR_BASE + 3)
#define FIN_TEMPORAL_CREDIT_ERR_STATE               (FIN_TEMPORAL_CREDIT_ERROR_BASE + 4)
#define FIN_TEMPORAL_CREDIT_ERR_IMMUNE              (FIN_TEMPORAL_CREDIT_ERROR_BASE + 5)
#define FIN_TEMPORAL_CREDIT_ERR_BBB                 (FIN_TEMPORAL_CREDIT_ERROR_BASE + 6)
#define FIN_TEMPORAL_CREDIT_ERR_HISTORY_FULL        (FIN_TEMPORAL_CREDIT_ERROR_BASE + 7)
#define FIN_TEMPORAL_CREDIT_ERR_NO_DECISIONS        (FIN_TEMPORAL_CREDIT_ERROR_BASE + 8)
#define FIN_TEMPORAL_CREDIT_ERR_INVALID_TIME        (FIN_TEMPORAL_CREDIT_ERROR_BASE + 9)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_TEMPORAL_CREDIT_OP_STATE_UNINITIALIZED = 0,
    FIN_TEMPORAL_CREDIT_OP_STATE_INITIALIZED,
    FIN_TEMPORAL_CREDIT_OP_STATE_ACTIVE,
    FIN_TEMPORAL_CREDIT_OP_STATE_ASSIGNING,
    FIN_TEMPORAL_CREDIT_OP_STATE_DEGRADED,
    FIN_TEMPORAL_CREDIT_OP_STATE_ERROR
} fin_temporal_credit_op_state_t;

/**
 * @brief Decision types for financial context
 */
typedef enum {
    FIN_DECISION_TYPE_BUY = 0,           /**< Buy/long entry */
    FIN_DECISION_TYPE_SELL,              /**< Sell/short entry */
    FIN_DECISION_TYPE_HOLD,              /**< Hold current position */
    FIN_DECISION_TYPE_EXIT_LONG,         /**< Exit long position */
    FIN_DECISION_TYPE_EXIT_SHORT,        /**< Exit short position (cover) */
    FIN_DECISION_TYPE_INCREASE_POSITION, /**< Add to existing position */
    FIN_DECISION_TYPE_DECREASE_POSITION, /**< Reduce position size */
    FIN_DECISION_TYPE_STOP_LOSS,         /**< Stop loss triggered */
    FIN_DECISION_TYPE_TAKE_PROFIT,       /**< Take profit triggered */
    FIN_DECISION_TYPE_REBALANCE,         /**< Portfolio rebalancing */
    FIN_DECISION_TYPE_HEDGE,             /**< Hedging action */
    FIN_DECISION_TYPE_CUSTOM,            /**< Custom decision type */
    FIN_DECISION_TYPE_COUNT
} fin_decision_type_t;

/**
 * @brief Credit assignment method
 */
typedef enum {
    FIN_CREDIT_METHOD_TD_LAMBDA = 0,     /**< TD(lambda) - default */
    FIN_CREDIT_METHOD_MONTE_CARLO,       /**< Monte Carlo (full episode) */
    FIN_CREDIT_METHOD_N_STEP,            /**< N-step return */
    FIN_CREDIT_METHOD_GAE,               /**< Generalized Advantage Estimation */
    FIN_CREDIT_METHOD_REWARD_SHAPING,    /**< Reward shaping with potential */
    FIN_CREDIT_METHOD_CUSTOM             /**< Custom credit function */
} fin_credit_method_t;

/**
 * @brief Trace replacement strategy
 */
typedef enum {
    FIN_TRACE_REPLACE_ACCUMULATING = 0,  /**< Add to existing trace */
    FIN_TRACE_REPLACE_REPLACING,         /**< Replace trace to 1.0 */
    FIN_TRACE_REPLACE_DUTCH              /**< Dutch traces (accumulating with cap) */
} fin_trace_replacement_t;

/* ============================================================================
 * Core Data Structures (as specified)
 * ============================================================================ */

/**
 * @brief Individual financial decision record
 *
 * Represents a decision made at a point in time with its magnitude
 * and current eligibility trace value.
 */
typedef struct {
    int decision_type;        /**< Decision type (fin_decision_type_t) */
    float magnitude;          /**< Decision magnitude (position size, $ amount) */
    uint64_t timestamp_ms;    /**< Decision timestamp in milliseconds */
    float eligibility;        /**< Current eligibility trace value [0-1] */
} fin_decision_t;

/**
 * @brief Decision history buffer
 *
 * Contains the sequence of decisions for credit assignment.
 */
typedef struct {
    fin_decision_t* decisions;   /**< Array of decision records */
    uint32_t num_decisions;      /**< Number of decisions in history */
} fin_decision_history_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t credit_assignments;   /**< Total credit assignment operations */
    uint64_t eligibility_traces;   /**< Total eligibility trace computations */
    uint64_t immune_checks;        /**< Immune system checks performed */
    uint64_t bbb_validations;      /**< BBB validations performed */
    uint64_t kg_messages_sent;     /**< KG messages published */
    uint64_t health_heartbeats;    /**< Health heartbeats sent */
} fin_temporal_credit_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Extended decision with context
 */
typedef struct {
    fin_decision_t decision;         /**< Base decision record */

    /* State context at decision time */
    float state_features[16];        /**< State feature vector */
    uint32_t num_features;           /**< Number of state features */

    /* Action context */
    uint32_t action_id;              /**< Unique action identifier */
    float confidence;                /**< Decision confidence [0-1] */
    float expected_return;           /**< Expected return at decision time */

    /* Pattern associations */
    uint32_t* pattern_ids;           /**< Associated pattern IDs */
    uint32_t num_patterns;           /**< Number of associated patterns */

    /* Credit tracking */
    float assigned_credit;           /**< Credit assigned so far */
    float cumulative_return;         /**< Return accumulated after decision */
    uint32_t credit_updates;         /**< Number of credit updates received */

    /* Metadata */
    char symbol[32];                 /**< Trading symbol */
    char reason[64];                 /**< Decision reason/label */
} fin_extended_decision_t;

/**
 * @brief Credit assignment result for a single decision
 */
typedef struct {
    uint32_t decision_index;         /**< Index in history */
    float credit;                    /**< Assigned credit [-inf, inf] */
    float eligibility_at_outcome;    /**< Eligibility when outcome observed */
    float temporal_discount;         /**< Temporal discount factor applied */
    float causal_weight;             /**< Causal contribution weight */
} fin_credit_result_t;

/**
 * @brief Full credit assignment result
 */
typedef struct {
    fin_credit_result_t* results;    /**< Array of per-decision credits */
    uint32_t num_results;            /**< Number of results */
    float total_credit;              /**< Sum of all credits */
    float outcome_value;             /**< The observed outcome */
    uint64_t outcome_timestamp_ms;   /**< When outcome was observed */
} fin_credit_assignment_result_t;

/**
 * @brief Eligibility trace computation result
 */
typedef struct {
    float* traces;                   /**< Trace values for each decision */
    uint32_t num_traces;             /**< Number of trace values */
    float total_eligibility;         /**< Sum of all eligibilities */
    uint64_t compute_time_us;        /**< Computation time in microseconds */
} fin_eligibility_result_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* TD(lambda) parameters */
    float lambda;                    /**< Eligibility trace decay rate [0-1] */
    float gamma;                     /**< Temporal discount factor [0-1] */
    float learning_rate;             /**< Learning rate for credit updates */
    float trace_threshold;           /**< Minimum trace value (zero below this) */

    /* Credit method settings */
    fin_credit_method_t method;      /**< Credit assignment method */
    fin_trace_replacement_t trace_replace; /**< Trace replacement strategy */
    uint32_t n_step;                 /**< N for n-step returns (if using) */

    /* History settings */
    uint32_t max_history_size;       /**< Maximum decisions in history */
    uint64_t history_retention_ms;   /**< How long to retain decisions */
    bool auto_prune_history;         /**< Auto-prune old decisions */

    /* Causality settings */
    bool enable_causal_filtering;    /**< Filter by causal relevance */
    float causal_threshold;          /**< Minimum causal weight to assign credit */
    bool use_temporal_window;        /**< Limit credit window */
    uint64_t temporal_window_ms;     /**< Maximum lookback time */

    /* Integration settings */
    bool enable_immune_integration;  /**< Enable immune system checks */
    bool enable_bbb_validation;      /**< Enable BBB validation */
    bool enable_kg_messaging;        /**< Enable KG messaging */
    bool enable_health_monitoring;   /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;            /**< Verbose debug output */
} fin_temporal_credit_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Credit assigned callback
 *
 * Called when credit is assigned to a decision.
 */
typedef void (*fin_credit_assigned_callback_t)(
    const fin_decision_t* decision,
    float credit,
    float eligibility,
    void* user_data
);

/**
 * @brief Eligibility decayed callback
 *
 * Called when eligibility traces are updated.
 */
typedef void (*fin_eligibility_decayed_callback_t)(
    uint32_t num_decisions,
    float total_eligibility,
    void* user_data
);

/**
 * @brief Custom credit function callback
 *
 * For FIN_CREDIT_METHOD_CUSTOM, this function computes credit.
 */
typedef float (*fin_custom_credit_fn_t)(
    const fin_decision_t* decision,
    float outcome,
    uint64_t outcome_time_ms,
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
 * @brief Opaque financial temporal credit bridge handle
 */
typedef struct financial_temporal_credit_bridge financial_temporal_credit_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_default_config(fin_temporal_credit_config_t* config);

/**
 * @brief Create financial temporal credit bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_temporal_credit_bridge_t* financial_temporal_credit_bridge_create(
    const fin_temporal_credit_config_t* config
);

/**
 * @brief Destroy financial temporal credit bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_temporal_credit_bridge_destroy(financial_temporal_credit_bridge_t* bridge);

/**
 * @brief Reset bridge state (clear history and traces)
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_reset(financial_temporal_credit_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_temporal_credit_bridge_set_immune(
    financial_temporal_credit_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_temporal_credit_bridge_set_bbb(
    financial_temporal_credit_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_temporal_credit_bridge_set_health_agent(
    financial_temporal_credit_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_temporal_credit_bridge_set_kg_wiring(
    financial_temporal_credit_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_temporal_credit_bridge_set_logger(
    financial_temporal_credit_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_temporal_credit_bridge_set_security(
    financial_temporal_credit_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_temporal_credit_bridge_set_ethics(
    financial_temporal_credit_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_temporal_credit_bridge_set_lgss(
    financial_temporal_credit_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_temporal_credit_bridge_set_coordinator(
    financial_temporal_credit_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_temporal_credit_bridge_set_bio_router(
    financial_temporal_credit_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Decision History Management
 * ============================================================================ */

/**
 * @brief Record a decision in history
 *
 * @param bridge Bridge handle
 * @param decision Decision to record
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_record_decision(
    financial_temporal_credit_bridge_t* bridge,
    const fin_decision_t* decision
);

/**
 * @brief Record an extended decision with context
 *
 * @param bridge Bridge handle
 * @param decision Extended decision to record
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_record_extended_decision(
    financial_temporal_credit_bridge_t* bridge,
    const fin_extended_decision_t* decision
);

/**
 * @brief Get decision history
 *
 * @param bridge Bridge handle
 * @param history Output history (caller allocates decisions array)
 * @param max_decisions Maximum decisions to return
 * @return Number of decisions returned, or negative error code
 */
int financial_temporal_credit_bridge_get_history(
    const financial_temporal_credit_bridge_t* bridge,
    fin_decision_history_t* history,
    uint32_t max_decisions
);

/**
 * @brief Clear decision history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_clear_history(
    financial_temporal_credit_bridge_t* bridge
);

/**
 * @brief Get decision count
 *
 * @param bridge Bridge handle
 * @return Number of decisions in history
 */
uint32_t financial_temporal_credit_bridge_get_decision_count(
    const financial_temporal_credit_bridge_t* bridge
);

/**
 * @brief Prune old decisions from history
 *
 * @param bridge Bridge handle
 * @param older_than_ms Remove decisions older than this timestamp
 * @return Number of decisions pruned
 */
uint32_t financial_temporal_credit_bridge_prune_history(
    financial_temporal_credit_bridge_t* bridge,
    uint64_t older_than_ms
);

/* ============================================================================
 * Core API - Credit Assignment
 * ============================================================================ */

/**
 * @brief Assign credit to decisions based on observed outcome
 *
 * This is the main credit assignment function. When an outcome is observed
 * (e.g., trade closed, return realized), call this to assign credit to
 * all eligible past decisions.
 *
 * @param bridge Bridge handle
 * @param outcome Observed outcome value (profit/loss, return, etc.)
 * @param outcome_time_ms Timestamp when outcome was observed
 * @param result Output credit assignment result (caller must free)
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_assign(
    financial_temporal_credit_bridge_t* bridge,
    float outcome,
    uint64_t outcome_time_ms,
    fin_credit_assignment_result_t* result
);

/**
 * @brief Assign credit with causal filtering
 *
 * Like assign(), but only considers decisions causally related to the outcome.
 *
 * @param bridge Bridge handle
 * @param outcome Observed outcome value
 * @param outcome_time_ms Timestamp when outcome was observed
 * @param causal_decision_ids Array of decision indices considered causal
 * @param num_causal_ids Number of causal decision IDs
 * @param result Output credit assignment result
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_assign_causal(
    financial_temporal_credit_bridge_t* bridge,
    float outcome,
    uint64_t outcome_time_ms,
    const uint32_t* causal_decision_ids,
    uint32_t num_causal_ids,
    fin_credit_assignment_result_t* result
);

/* ============================================================================
 * Core API - Eligibility Traces
 * ============================================================================ */

/**
 * @brief Compute eligibility traces for all decisions
 *
 * Computes the current eligibility trace value for each decision in history,
 * based on time elapsed and the lambda decay parameter.
 *
 * @param bridge Bridge handle
 * @param reference_time_ms Reference time for trace computation (0 = now)
 * @param result Output eligibility result (caller must free)
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_eligibility_trace(
    financial_temporal_credit_bridge_t* bridge,
    uint64_t reference_time_ms,
    fin_eligibility_result_t* result
);

/**
 * @brief Update eligibility traces (decay step)
 *
 * Called periodically to decay all eligibility traces by lambda.
 * This is the "between-step" update in TD(lambda).
 *
 * @param bridge Bridge handle
 * @param time_delta_ms Time elapsed since last update
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_decay_traces(
    financial_temporal_credit_bridge_t* bridge,
    uint64_t time_delta_ms
);

/**
 * @brief Boost eligibility for a specific decision
 *
 * Used when a decision is "revisited" (e.g., same pattern recognized).
 * The trace is boosted according to the trace replacement strategy.
 *
 * @param bridge Bridge handle
 * @param decision_index Index of decision in history
 * @param boost_amount Amount to boost (or 1.0 for replacing traces)
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_boost_eligibility(
    financial_temporal_credit_bridge_t* bridge,
    uint32_t decision_index,
    float boost_amount
);

/**
 * @brief Get eligibility for a specific decision
 *
 * @param bridge Bridge handle
 * @param decision_index Index of decision in history
 * @return Current eligibility value, or -1.0f on error
 */
float financial_temporal_credit_bridge_get_eligibility(
    const financial_temporal_credit_bridge_t* bridge,
    uint32_t decision_index
);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Free a credit assignment result
 *
 * @param result Result to free (NULL safe)
 */
void financial_temporal_credit_result_free(fin_credit_assignment_result_t* result);

/**
 * @brief Initialize a credit assignment result (before use)
 *
 * @param result Result to initialize
 */
void financial_temporal_credit_result_init(fin_credit_assignment_result_t* result);

/**
 * @brief Free an eligibility result
 *
 * @param result Result to free (NULL safe)
 */
void financial_temporal_eligibility_result_free(fin_eligibility_result_t* result);

/**
 * @brief Initialize an eligibility result (before use)
 *
 * @param result Result to initialize
 */
void financial_temporal_eligibility_result_init(fin_eligibility_result_t* result);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set credit assigned callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_credit_callback(
    financial_temporal_credit_bridge_t* bridge,
    fin_credit_assigned_callback_t callback,
    void* user_data
);

/**
 * @brief Set eligibility decayed callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_decay_callback(
    financial_temporal_credit_bridge_t* bridge,
    fin_eligibility_decayed_callback_t callback,
    void* user_data
);

/**
 * @brief Set custom credit function
 *
 * For FIN_CREDIT_METHOD_CUSTOM, set the custom credit computation function.
 *
 * @param bridge Bridge handle
 * @param fn Custom credit function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_custom_credit_fn(
    financial_temporal_credit_bridge_t* bridge,
    fin_custom_credit_fn_t fn,
    void* user_data
);

/* ============================================================================
 * Configuration Updates
 * ============================================================================ */

/**
 * @brief Set lambda (eligibility decay rate)
 *
 * @param bridge Bridge handle
 * @param lambda New lambda value [0-1]
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_lambda(
    financial_temporal_credit_bridge_t* bridge,
    float lambda
);

/**
 * @brief Set gamma (temporal discount factor)
 *
 * @param bridge Bridge handle
 * @param gamma New gamma value [0-1]
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_gamma(
    financial_temporal_credit_bridge_t* bridge,
    float gamma
);

/**
 * @brief Set learning rate
 *
 * @param bridge Bridge handle
 * @param learning_rate New learning rate
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_learning_rate(
    financial_temporal_credit_bridge_t* bridge,
    float learning_rate
);

/**
 * @brief Set credit assignment method
 *
 * @param bridge Bridge handle
 * @param method New credit assignment method
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_set_method(
    financial_temporal_credit_bridge_t* bridge,
    fin_credit_method_t method
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_temporal_credit_op_state_t financial_temporal_credit_bridge_get_op_state(
    const financial_temporal_credit_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_temporal_credit_bridge_get_stats(
    const financial_temporal_credit_bridge_t* bridge,
    fin_temporal_credit_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_temporal_credit_bridge_reset_stats(
    financial_temporal_credit_bridge_t* bridge
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_temporal_credit_bridge_get_last_error(void);

/**
 * @brief Get current lambda value
 *
 * @param bridge Bridge handle
 * @return Current lambda value
 */
float financial_temporal_credit_bridge_get_lambda(
    const financial_temporal_credit_bridge_t* bridge
);

/**
 * @brief Get current gamma value
 *
 * @param bridge Bridge handle
 * @return Current gamma value
 */
float financial_temporal_credit_bridge_get_gamma(
    const financial_temporal_credit_bridge_t* bridge
);

/**
 * @brief Get total eligibility (sum of all traces)
 *
 * @param bridge Bridge handle
 * @return Total eligibility
 */
float financial_temporal_credit_bridge_get_total_eligibility(
    const financial_temporal_credit_bridge_t* bridge
);

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
int financial_temporal_credit_bridge_heartbeat(
    financial_temporal_credit_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_temporal_credit_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get operational state name
 *
 * @param state Operational state
 * @return String name (static)
 */
const char* fin_temporal_credit_op_state_name(fin_temporal_credit_op_state_t state);

/**
 * @brief Get decision type name
 *
 * @param type Decision type
 * @return String name (static)
 */
const char* fin_temporal_credit_decision_type_name(fin_decision_type_t type);

/**
 * @brief Get credit method name
 *
 * @param method Credit method
 * @return String name (static)
 */
const char* fin_temporal_credit_method_name(fin_credit_method_t method);

/**
 * @brief Get trace replacement strategy name
 *
 * @param trace_replace Trace replacement strategy
 * @return String name (static)
 */
const char* fin_temporal_credit_trace_replace_name(fin_trace_replacement_t trace_replace);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_temporal_credit_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_TEMPORAL_CREDIT_BRIDGE_H */
