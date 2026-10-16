/**
 * @file nimcp_financial_reasoning_bridge.h
 * @brief Financial Reasoning Bridge - Forward/Backward Chaining for Trading Rules
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for rule-based reasoning in financial decision-making using
 *       forward and backward chaining inference engines. Integrates legendary
 *       investor wisdom (Graham, Buffett, Soros, etc.) as trading rules.
 *
 * WHY:  Quantitative trading benefits from combining:
 *       - Forward chaining: Given market facts, derive trading signals
 *       - Backward chaining: Given a goal (e.g., "should buy?"), verify conditions
 *       This dual-reasoning approach enables both reactive signal generation
 *       and goal-directed hypothesis testing.
 *
 * HOW:  Rules are stored in an internal rule base. Forward chaining iterates
 *       through rules, firing those whose conditions match current facts.
 *       Backward chaining starts from a goal and recursively verifies sub-goals.
 *       Human-readable reasoning chains explain the inference path.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Reasoning Bridge                                   |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Rule Base            |       |  Working Memory       |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | IF condition          |       | Current market facts  |                |
 * |  | THEN action           |       | Portfolio state       |                |
 * |  | Confidence + Source   |       | Derived assertions    |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Inference Engine                                 |            |
 * |  |  Forward Chain: facts -> rules -> new facts/signals      |            |
 * |  |  Backward Chain: goal -> rules -> sub-goals -> verify    |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          Reasoning Result                                 |            |
 * |  |  - Triggered rules                                       |            |
 * |  |  - Derived signals (BUY/SELL/HOLD/etc.)                 |            |
 * |  |  - Human-readable reasoning chain                        |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * RULE SOURCES:
 * - Graham: Value investing fundamentals (P/E, book value, margin of safety)
 * - Buffett: Quality + moat + management + fair price
 * - Soros: Reflexivity, market psychology, regime changes
 * - Lynch: Growth at reasonable price (GARP), know what you own
 * - Dalio: All-weather, risk parity, macro cycles
 * - Simons: Quantitative signals, mean reversion, momentum
 * - Technical: RSI, MACD, Bollinger, support/resistance
 * - Custom: User-defined rules
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_financial_investor_archetype.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_REASONING_BRIDGE_H
#define NIMCP_FINANCIAL_REASONING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_REASONING_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_REASONING_BRIDGE_MAGIC      0x46524242  /* 'FRRB' */

/** Bio-async module ID for financial reasoning bridge */
#define BIO_MODULE_FINANCIAL_REASONING        0x039A

/** Maximum rules in rule base */
#define FIN_REASONING_MAX_RULES               256

/** Maximum triggered rules per inference */
#define FIN_REASONING_MAX_TRIGGERED           64

/** Maximum derived signals per inference */
#define FIN_REASONING_MAX_SIGNALS             32

/** Maximum facts in working memory */
#define FIN_REASONING_MAX_FACTS               128

/** Maximum recursion depth for backward chaining */
#define FIN_REASONING_MAX_DEPTH               16

/** String field lengths */
#define FIN_REASONING_CONDITION_LEN           256
#define FIN_REASONING_ACTION_LEN              256
#define FIN_REASONING_SOURCE_LEN              64
#define FIN_REASONING_CHAIN_LEN               1024
#define FIN_REASONING_FACT_LEN                256
#define FIN_REASONING_DESC_LEN                128

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_REASONING_ERROR_BASE              33400
#define FIN_REASONING_ERR_OK                  0
#define FIN_REASONING_ERR_NULL                (FIN_REASONING_ERROR_BASE + 1)
#define FIN_REASONING_ERR_INVALID_PARAM       (FIN_REASONING_ERROR_BASE + 2)
#define FIN_REASONING_ERR_NO_MEMORY           (FIN_REASONING_ERROR_BASE + 3)
#define FIN_REASONING_ERR_STATE               (FIN_REASONING_ERROR_BASE + 4)
#define FIN_REASONING_ERR_IMMUNE              (FIN_REASONING_ERROR_BASE + 5)
#define FIN_REASONING_ERR_BBB                 (FIN_REASONING_ERROR_BASE + 6)
#define FIN_REASONING_ERR_RULE_FULL           (FIN_REASONING_ERROR_BASE + 7)
#define FIN_REASONING_ERR_FACT_FULL           (FIN_REASONING_ERROR_BASE + 8)
#define FIN_REASONING_ERR_DEPTH_EXCEEDED      (FIN_REASONING_ERROR_BASE + 9)
#define FIN_REASONING_ERR_NOT_FOUND           (FIN_REASONING_ERROR_BASE + 10)
#define FIN_REASONING_ERR_PARSE               (FIN_REASONING_ERROR_BASE + 11)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_REASONING_OP_STATE_UNINITIALIZED = 0,
    FIN_REASONING_OP_STATE_INITIALIZED,
    FIN_REASONING_OP_STATE_ACTIVE,
    FIN_REASONING_OP_STATE_DEGRADED,
    FIN_REASONING_OP_STATE_ERROR
} fin_reasoning_op_state_t;

/**
 * @brief Trading signal types derived from reasoning
 */
typedef enum {
    FIN_SIGNAL_NONE = 0,
    FIN_SIGNAL_STRONG_BUY,
    FIN_SIGNAL_BUY,
    FIN_SIGNAL_WEAK_BUY,
    FIN_SIGNAL_HOLD,
    FIN_SIGNAL_WEAK_SELL,
    FIN_SIGNAL_SELL,
    FIN_SIGNAL_STRONG_SELL,
    FIN_SIGNAL_AVOID,            /**< Stay away entirely */
    FIN_SIGNAL_ACCUMULATE,       /**< Buy on dips */
    FIN_SIGNAL_DISTRIBUTE,       /**< Sell on rallies */
    FIN_SIGNAL_HEDGE,            /**< Hedge existing position */
    FIN_SIGNAL_WAIT,             /**< Wait for better entry */
    FIN_SIGNAL_COUNT
} fin_signal_type_t;

/**
 * @brief Rule source categories
 */
typedef enum {
    FIN_RULE_SOURCE_CUSTOM = 0,
    FIN_RULE_SOURCE_GRAHAM,      /**< Benjamin Graham value investing */
    FIN_RULE_SOURCE_BUFFETT,     /**< Warren Buffett quality investing */
    FIN_RULE_SOURCE_SOROS,       /**< George Soros reflexivity */
    FIN_RULE_SOURCE_LYNCH,       /**< Peter Lynch GARP */
    FIN_RULE_SOURCE_DALIO,       /**< Ray Dalio macro/risk parity */
    FIN_RULE_SOURCE_SIMONS,      /**< Jim Simons quantitative */
    FIN_RULE_SOURCE_TECHNICAL,   /**< Technical analysis */
    FIN_RULE_SOURCE_FUNDAMENTAL, /**< Fundamental analysis */
    FIN_RULE_SOURCE_SENTIMENT,   /**< Sentiment analysis */
    FIN_RULE_SOURCE_COUNT
} fin_rule_source_t;

/**
 * @brief Inference mode
 */
typedef enum {
    FIN_INFERENCE_FORWARD = 0,   /**< Forward chaining (data-driven) */
    FIN_INFERENCE_BACKWARD,      /**< Backward chaining (goal-driven) */
    FIN_INFERENCE_HYBRID         /**< Combined approach */
} fin_inference_mode_t;

/**
 * @brief Verification result for backward chaining
 */
typedef enum {
    FIN_VERIFY_UNKNOWN = 0,      /**< Cannot determine */
    FIN_VERIFY_TRUE,             /**< Condition verified true */
    FIN_VERIFY_FALSE,            /**< Condition verified false */
    FIN_VERIFY_PARTIAL           /**< Partially verified (some evidence) */
} fin_verify_result_t;

/* ============================================================================
 * Core Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Trading rule: IF condition THEN action
 */
typedef struct {
    char condition[FIN_REASONING_CONDITION_LEN];  /**< IF condition */
    char action[FIN_REASONING_ACTION_LEN];        /**< THEN action */
    float confidence;                              /**< Rule confidence [0-1] */
    char source[FIN_REASONING_SOURCE_LEN];        /**< Rule source (Graham, Buffett, etc.) */
} fin_trading_rule_t;

/**
 * @brief Reasoning result from inference
 */
typedef struct {
    fin_trading_rule_t* triggered_rules;          /**< Array of triggered rules */
    uint32_t num_triggered;                        /**< Number of triggered rules */
    int* derived_signals;                          /**< Signal types derived (fin_signal_type_t) */
    uint32_t num_signals;                          /**< Number of derived signals */
    char reasoning_chain[FIN_REASONING_CHAIN_LEN]; /**< Human-readable explanation */
} fin_reasoning_result_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t rules_added;             /**< Total rules added */
    uint64_t forward_inferences;      /**< Forward chaining calls */
    uint64_t backward_verifications;  /**< Backward chaining calls */
    uint64_t signals_derived;         /**< Total signals derived */
    uint64_t immune_checks;           /**< Immune system checks performed */
    uint64_t bbb_validations;         /**< BBB validations performed */
    uint64_t kg_messages_sent;        /**< KG messages published */
    uint64_t health_heartbeats;       /**< Health heartbeats sent */
} fin_reasoning_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Fact in working memory
 */
typedef struct {
    char name[FIN_REASONING_FACT_LEN];  /**< Fact name/predicate */
    float value;                         /**< Numeric value */
    bool is_boolean;                     /**< True if fact is boolean */
    bool bool_value;                     /**< Boolean value if is_boolean */
    uint64_t timestamp_ms;               /**< When fact was asserted */
    float confidence;                    /**< Confidence in fact [0-1] */
} fin_fact_t;

/**
 * @brief Extended rule with metadata
 */
typedef struct {
    fin_trading_rule_t rule;             /**< Base rule */
    uint32_t rule_id;                    /**< Unique rule ID */
    fin_rule_source_t source_type;       /**< Categorized source */
    uint32_t priority;                   /**< Rule priority (higher = first) */
    bool enabled;                        /**< Rule enabled flag */
    uint64_t fire_count;                 /**< Times rule has fired */
    uint64_t last_fired_ms;              /**< Last fire timestamp */
} fin_rule_entry_t;

/**
 * @brief Backward chaining verification request
 */
typedef struct {
    char goal[FIN_REASONING_CONDITION_LEN];  /**< Goal to verify */
    uint32_t max_depth;                       /**< Max recursion depth */
    bool explain;                             /**< Generate explanation */
} fin_verify_request_t;

/**
 * @brief Backward chaining verification response
 */
typedef struct {
    fin_verify_result_t result;              /**< Verification result */
    float confidence;                         /**< Overall confidence [0-1] */
    uint32_t rules_checked;                   /**< Rules examined */
    uint32_t depth_reached;                   /**< Max depth reached */
    char explanation[FIN_REASONING_CHAIN_LEN]; /**< Reasoning explanation */
} fin_verify_response_t;

/**
 * @brief Market context for inference
 */
typedef struct {
    /* Price metrics */
    float current_price;
    float price_change_pct;
    float price_52w_high;
    float price_52w_low;

    /* Valuation metrics */
    float pe_ratio;
    float pb_ratio;
    float ps_ratio;
    float peg_ratio;

    /* Technical indicators */
    float rsi;              /**< Relative Strength Index [0-100] */
    float macd;             /**< MACD value */
    float macd_signal;      /**< MACD signal line */
    float sma_20;           /**< 20-day SMA */
    float sma_50;           /**< 50-day SMA */
    float sma_200;          /**< 200-day SMA */
    float bollinger_upper;
    float bollinger_lower;

    /* Volume metrics */
    float volume_ratio;     /**< Current/average volume */

    /* Fundamental metrics */
    float roe;              /**< Return on equity */
    float debt_equity;      /**< Debt to equity */
    float current_ratio;    /**< Current ratio */
    float earnings_growth;  /**< YoY earnings growth */
    float revenue_growth;   /**< YoY revenue growth */

    /* Sentiment */
    float sentiment_score;  /**< Market sentiment [-1, 1] */

    /* Volatility */
    float volatility;       /**< Historical volatility */
    float vix;              /**< VIX level if available */
} fin_market_context_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Inference settings */
    uint32_t max_iterations;          /**< Max forward chain iterations */
    uint32_t max_depth;               /**< Max backward chain depth */
    float min_confidence;             /**< Minimum rule confidence to fire */
    bool enable_conflict_resolution;  /**< Resolve conflicting rules */
    bool prefer_higher_confidence;    /**< Prefer higher confidence rules */

    /* Rule loading */
    bool load_graham_rules;           /**< Load Graham value rules */
    bool load_buffett_rules;          /**< Load Buffett quality rules */
    bool load_technical_rules;        /**< Load technical analysis rules */
    bool load_sentiment_rules;        /**< Load sentiment rules */

    /* Integration settings */
    bool enable_immune_integration;   /**< Enable immune system checks */
    bool enable_bbb_validation;       /**< Enable BBB validation */
    bool enable_kg_messaging;         /**< Enable KG messaging */
    bool enable_health_monitoring;    /**< Enable health heartbeats */

    /* Explanation settings */
    bool verbose_explanations;        /**< Detailed reasoning chains */
    bool include_rule_sources;        /**< Include rule source in explanation */

    /* Logging */
    bool verbose_logging;             /**< Verbose debug output */
} fin_reasoning_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Rule fired callback
 */
typedef void (*fin_reasoning_rule_callback_t)(
    const fin_rule_entry_t* rule,
    const fin_reasoning_result_t* result,
    void* user_data
);

/**
 * @brief Signal derived callback
 */
typedef void (*fin_reasoning_signal_callback_t)(
    fin_signal_type_t signal,
    float confidence,
    const char* explanation,
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
 * @brief Opaque financial reasoning bridge handle
 */
typedef struct financial_reasoning_bridge financial_reasoning_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_default_config(fin_reasoning_config_t* config);

/**
 * @brief Create financial reasoning bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_reasoning_bridge_t* financial_reasoning_bridge_create(
    const fin_reasoning_config_t* config
);

/**
 * @brief Destroy financial reasoning bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_reasoning_bridge_destroy(financial_reasoning_bridge_t* bridge);

/**
 * @brief Reset bridge state (clear working memory, keep rules)
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_reset(financial_reasoning_bridge_t* bridge);

/**
 * @brief Clear all rules from rule base
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_clear_rules(financial_reasoning_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_reasoning_bridge_set_immune(
    financial_reasoning_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_reasoning_bridge_set_bbb(
    financial_reasoning_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_reasoning_bridge_set_health_agent(
    financial_reasoning_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_reasoning_bridge_set_kg_wiring(
    financial_reasoning_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_reasoning_bridge_set_logger(
    financial_reasoning_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_reasoning_bridge_set_security(
    financial_reasoning_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_reasoning_bridge_set_ethics(
    financial_reasoning_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_reasoning_bridge_set_lgss(
    financial_reasoning_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_reasoning_bridge_set_coordinator(
    financial_reasoning_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_reasoning_bridge_set_bio_router(
    financial_reasoning_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Rule Management
 * ============================================================================ */

/**
 * @brief Add a trading rule to the rule base
 *
 * @param bridge Bridge handle
 * @param rule Trading rule to add
 * @return Rule ID on success (>=0), negative error code on failure
 */
int financial_reasoning_bridge_add_rule(
    financial_reasoning_bridge_t* bridge,
    const fin_trading_rule_t* rule
);

/**
 * @brief Add a rule with extended metadata
 *
 * @param bridge Bridge handle
 * @param rule Rule with metadata
 * @return Rule ID on success (>=0), negative error code on failure
 */
int financial_reasoning_bridge_add_rule_ex(
    financial_reasoning_bridge_t* bridge,
    const fin_rule_entry_t* rule
);

/**
 * @brief Remove a rule by ID
 *
 * @param bridge Bridge handle
 * @param rule_id Rule ID to remove
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_remove_rule(
    financial_reasoning_bridge_t* bridge,
    uint32_t rule_id
);

/**
 * @brief Enable/disable a rule
 *
 * @param bridge Bridge handle
 * @param rule_id Rule ID
 * @param enabled Enable state
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_set_rule_enabled(
    financial_reasoning_bridge_t* bridge,
    uint32_t rule_id,
    bool enabled
);

/**
 * @brief Get rule count
 *
 * @param bridge Bridge handle
 * @return Number of rules in rule base
 */
uint32_t financial_reasoning_bridge_get_rule_count(
    const financial_reasoning_bridge_t* bridge
);

/**
 * @brief Load predefined rules by source type
 *
 * @param bridge Bridge handle
 * @param source Rule source category
 * @return Number of rules loaded, or negative error code
 */
int financial_reasoning_bridge_load_rules(
    financial_reasoning_bridge_t* bridge,
    fin_rule_source_t source
);

/* ============================================================================
 * Core API - Working Memory (Facts)
 * ============================================================================ */

/**
 * @brief Assert a fact into working memory
 *
 * @param bridge Bridge handle
 * @param fact Fact to assert
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_assert_fact(
    financial_reasoning_bridge_t* bridge,
    const fin_fact_t* fact
);

/**
 * @brief Assert numeric fact by name
 *
 * @param bridge Bridge handle
 * @param name Fact name
 * @param value Numeric value
 * @param confidence Confidence [0-1]
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_assert_numeric(
    financial_reasoning_bridge_t* bridge,
    const char* name,
    float value,
    float confidence
);

/**
 * @brief Assert boolean fact by name
 *
 * @param bridge Bridge handle
 * @param name Fact name
 * @param value Boolean value
 * @param confidence Confidence [0-1]
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_assert_bool(
    financial_reasoning_bridge_t* bridge,
    const char* name,
    bool value,
    float confidence
);

/**
 * @brief Retract a fact from working memory
 *
 * @param bridge Bridge handle
 * @param name Fact name to retract
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_retract_fact(
    financial_reasoning_bridge_t* bridge,
    const char* name
);

/**
 * @brief Clear all facts from working memory
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_clear_facts(
    financial_reasoning_bridge_t* bridge
);

/**
 * @brief Load facts from market context
 *
 * @param bridge Bridge handle
 * @param context Market context
 * @return Number of facts loaded, or negative error code
 */
int financial_reasoning_bridge_load_context(
    financial_reasoning_bridge_t* bridge,
    const fin_market_context_t* context
);

/* ============================================================================
 * Core API - Inference
 * ============================================================================ */

/**
 * @brief Derive trading signals using forward chaining
 *
 * Iterates through rules, firing those whose conditions match current facts.
 * Continues until no new facts/signals are derived or max iterations reached.
 *
 * @param bridge Bridge handle
 * @param result Output reasoning result (caller must free triggered_rules and derived_signals)
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_derive_signals(
    financial_reasoning_bridge_t* bridge,
    fin_reasoning_result_t* result
);

/**
 * @brief Verify a condition using backward chaining
 *
 * Starts from goal and recursively verifies sub-goals by matching rules.
 *
 * @param bridge Bridge handle
 * @param request Verification request
 * @param response Output verification response
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_verify_condition(
    financial_reasoning_bridge_t* bridge,
    const fin_verify_request_t* request,
    fin_verify_response_t* response
);

/**
 * @brief Run hybrid inference (forward then backward)
 *
 * First derives signals via forward chaining, then verifies key conclusions
 * via backward chaining for higher confidence.
 *
 * @param bridge Bridge handle
 * @param goals Array of goals to verify after forward pass
 * @param num_goals Number of goals
 * @param result Output reasoning result
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_hybrid_inference(
    financial_reasoning_bridge_t* bridge,
    const char** goals,
    uint32_t num_goals,
    fin_reasoning_result_t* result
);

/**
 * @brief Get the strongest signal from last inference
 *
 * @param bridge Bridge handle
 * @param out_signal Output signal type
 * @param out_confidence Output confidence
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_get_recommendation(
    financial_reasoning_bridge_t* bridge,
    fin_signal_type_t* out_signal,
    float* out_confidence
);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Free a reasoning result
 *
 * @param result Result to free (NULL safe)
 */
void financial_reasoning_result_free(fin_reasoning_result_t* result);

/**
 * @brief Initialize a reasoning result (before use)
 *
 * @param result Result to initialize
 */
void financial_reasoning_result_init(fin_reasoning_result_t* result);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set rule fired callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_set_rule_callback(
    financial_reasoning_bridge_t* bridge,
    fin_reasoning_rule_callback_t callback,
    void* user_data
);

/**
 * @brief Set signal derived callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_set_signal_callback(
    financial_reasoning_bridge_t* bridge,
    fin_reasoning_signal_callback_t callback,
    void* user_data
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
fin_reasoning_op_state_t financial_reasoning_bridge_get_op_state(
    const financial_reasoning_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_reasoning_bridge_get_stats(
    const financial_reasoning_bridge_t* bridge,
    fin_reasoning_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_reasoning_bridge_reset_stats(financial_reasoning_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_reasoning_bridge_get_last_error(void);

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
int financial_reasoning_bridge_heartbeat(
    financial_reasoning_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_reasoning_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get operational state name
 *
 * @param state Operational state
 * @return String name (static)
 */
const char* fin_reasoning_op_state_name(fin_reasoning_op_state_t state);

/**
 * @brief Get signal type name
 *
 * @param signal Signal type
 * @return String name (static)
 */
const char* fin_reasoning_signal_name(fin_signal_type_t signal);

/**
 * @brief Get rule source name
 *
 * @param source Rule source
 * @return String name (static)
 */
const char* fin_reasoning_source_name(fin_rule_source_t source);

/**
 * @brief Get verification result name
 *
 * @param result Verification result
 * @return String name (static)
 */
const char* fin_reasoning_verify_name(fin_verify_result_t result);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_reasoning_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_REASONING_BRIDGE_H */
