/**
 * @file nimcp_financial_regret_bridge.h
 * @brief Financial Regret Bridge - Counterfactual regret analysis and lesson extraction
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for analyzing trading regret, computing counterfactual outcomes,
 *       and extracting actionable lessons from past decisions.
 *
 * WHY:  Trading decisions often result in regret when outcomes differ from
 *       expectations. This bridge enables:
 *       - Quantified regret analysis across trades
 *       - Counterfactual "what if" scenario computation
 *       - Systematic lesson extraction from mistakes
 *       - Pattern recognition across regret episodes
 *
 * HOW:  Analyzes completed trades to compute regret magnitude by comparing
 *       actual actions to optimal hindsight actions. Generates counterfactual
 *       scenarios and extracts lessons to improve future decision-making.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Financial Regret Bridge                                |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |      Trade History        |       |    Counterfactual Engine  |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Completed trades          |       | "What if I held?"         |        |
 * |  | Actions taken             |       | "What if I sized diff?"   |        |
 * |  | Outcomes recorded         |       | Alternative outcomes      |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |              Regret Analysis Engine                       |            |
 * |  +----------------------------------------------------------+            |
 * |  | actual_outcome vs counterfactual_outcome -> regret       |            |
 * |  | best_action identification                                |            |
 * |  | regret magnitude quantification                          |            |
 * |  +---------------------------+------------------------------+            |
 * |                              |                                            |
 * |                              v                                            |
 * |  +----------------------------------------------------------+            |
 * |  |              Lesson Extraction Engine                     |            |
 * |  +----------------------------------------------------------+            |
 * |  | Pattern recognition across regret episodes                |            |
 * |  | Actionable insight generation                             |            |
 * |  | Decision rule improvement                                 |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * REGRET TYPES:
 * - COMMISSION: Regret for action taken (should not have entered)
 * - OMISSION: Regret for action not taken (should have entered)
 * - TIMING: Regret for wrong timing (too early/late)
 * - SIZING: Regret for wrong position size (too small/large)
 * - EXIT: Regret for exit decision (held too long/short)
 *
 * @see nimcp_financial_metacognition_bridge.h
 * @see nimcp_counterfactual.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_REGRET_BRIDGE_H
#define NIMCP_FINANCIAL_REGRET_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_REGRET_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_REGRET_BRIDGE_MAGIC      0x46524754  /* 'FRGT' */

/** Bio-async module ID for financial regret bridge */
#define BIO_MODULE_FINANCIAL_REGRET        0x039B

/** Maximum trade history entries */
#define FIN_REGRET_MAX_HISTORY             512

/** Maximum lessons to store */
#define FIN_REGRET_MAX_LESSONS             128

/** Maximum counterfactual scenarios per analysis */
#define FIN_REGRET_MAX_COUNTERFACTUALS     8

/** Description/lesson string length */
#define FIN_REGRET_DESC_LEN                256

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_REGRET_ERROR_BASE              33500
#define FIN_REGRET_ERR_OK                  0
#define FIN_REGRET_ERR_NULL                (FIN_REGRET_ERROR_BASE + 1)
#define FIN_REGRET_ERR_INVALID_PARAM       (FIN_REGRET_ERROR_BASE + 2)
#define FIN_REGRET_ERR_NO_MEMORY           (FIN_REGRET_ERROR_BASE + 3)
#define FIN_REGRET_ERR_STATE               (FIN_REGRET_ERROR_BASE + 4)
#define FIN_REGRET_ERR_IMMUNE              (FIN_REGRET_ERROR_BASE + 5)
#define FIN_REGRET_ERR_BBB                 (FIN_REGRET_ERROR_BASE + 6)
#define FIN_REGRET_ERR_VALIDATION          (FIN_REGRET_ERROR_BASE + 7)
#define FIN_REGRET_ERR_INSUFFICIENT_DATA   (FIN_REGRET_ERROR_BASE + 8)
#define FIN_REGRET_ERR_CAPACITY            (FIN_REGRET_ERROR_BASE + 9)
#define FIN_REGRET_ERR_NOT_FOUND           (FIN_REGRET_ERROR_BASE + 10)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Trade direction
 */
typedef enum {
    FIN_TRADE_DIRECTION_LONG = 1,    /**< Long position (buy) */
    FIN_TRADE_DIRECTION_SHORT = -1,  /**< Short position (sell) */
    FIN_TRADE_DIRECTION_NEUTRAL = 0  /**< No position */
} fin_trade_direction_t;

/**
 * @brief Action types for trading decisions (guarded)
 */
#ifndef FIN_ACTION_TYPE_DEFINED
#define FIN_ACTION_TYPE_DEFINED
typedef enum {
    FIN_ACTION_NONE = 0,       /**< No action taken */
    FIN_ACTION_BUY,            /**< Buy/enter long */
    FIN_ACTION_SELL,           /**< Sell/enter short */
    FIN_ACTION_HOLD,           /**< Hold position */
    FIN_ACTION_EXIT,           /**< Exit position */
    FIN_ACTION_INCREASE,       /**< Increase position size */
    FIN_ACTION_DECREASE,       /**< Decrease position size */
    FIN_ACTION_COUNT
} fin_action_type_t;
#endif /* FIN_ACTION_TYPE_DEFINED */

/**
 * @brief Regret types
 */
typedef enum {
    FIN_REGRET_COMMISSION = 0, /**< Regret for action taken */
    FIN_REGRET_OMISSION,       /**< Regret for action not taken */
    FIN_REGRET_TIMING,         /**< Regret for timing */
    FIN_REGRET_SIZING,         /**< Regret for position size */
    FIN_REGRET_EXIT,           /**< Regret for exit decision */
    FIN_REGRET_TYPE_COUNT
} fin_regret_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_REGRET_STATE_UNINITIALIZED = 0,
    FIN_REGRET_STATE_INITIALIZED,
    FIN_REGRET_STATE_ACTIVE,
    FIN_REGRET_STATE_ANALYZING,
    FIN_REGRET_STATE_DEGRADED,
    FIN_REGRET_STATE_ERROR
} fin_regret_bridge_state_t;

/* ============================================================================
 * Core Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Trade record for regret analysis
 */
typedef struct {
    float price;           /**< Entry/exit price */
    float quantity;        /**< Position quantity */
    int direction;         /**< Trade direction (1=long, -1=short, 0=neutral) */
    float outcome;         /**< Trade outcome (profit/loss normalized) */
    uint64_t timestamp_ms; /**< Trade timestamp in milliseconds */
} fin_trade_t;

/**
 * @brief Action representation (guarded)
 *
 * NOTE: This provides a minimal fallback definition. For full functionality,
 * include nimcp_financial_bridge.h before this header.
 */
#ifndef FIN_ACTION_DEFINED
#define FIN_ACTION_DEFINED
typedef struct {
    fin_action_type_t type;  /**< Action type */
    char symbol[32];         /**< Symbol for the action */
    float magnitude;         /**< Action magnitude (e.g., position size change) */
    float position_weight;   /**< Position weight in portfolio */
    float leverage_ratio;    /**< Leverage ratio */
    float current_portfolio_risk; /**< Current portfolio risk */
    bool is_margin;          /**< Is margin trade */
    bool is_suitable;        /**< Is suitable for client */
    uint32_t client_age;     /**< Client age for suitability */
    char counterparty[64];   /**< Counterparty name */
    bool counterparty_sanctioned; /**< Is counterparty sanctioned */
    char notes[256];         /**< Additional notes */
} fin_action_t;
#endif /* FIN_ACTION_DEFINED */

/**
 * @brief Regret analysis result
 */
typedef struct {
    fin_trade_t trade;               /**< Original trade */
    fin_action_t action_taken;       /**< Action that was taken */
    fin_action_t best_action;        /**< Optimal hindsight action */
    float regret_magnitude;          /**< Regret magnitude [0, 1] */
    char counterfactual[256];        /**< Counterfactual description */
    char lesson[256];                /**< Extracted lesson */
} fin_regret_analysis_t;

/**
 * @brief Bridge statistics (as specified by user)
 */
typedef struct {
    uint64_t analyses;             /**< Total regret analyses performed */
    uint64_t counterfactuals;      /**< Total counterfactuals computed */
    uint64_t lessons_extracted;    /**< Total lessons extracted */
    uint64_t immune_checks;        /**< Immune system checks performed */
    uint64_t bbb_validations;      /**< BBB validations performed */
    uint64_t kg_messages_sent;     /**< KG messages published */
    uint64_t health_heartbeats;    /**< Health heartbeats sent */
} fin_regret_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Extended trade record with full context
 */
typedef struct {
    fin_trade_t trade;             /**< Core trade data */
    char symbol[16];               /**< Instrument symbol */
    char strategy[64];             /**< Strategy name */
    float entry_price;             /**< Entry price */
    float exit_price;              /**< Exit price */
    float stop_loss;               /**< Stop loss level */
    float take_profit;             /**< Take profit level */
    float max_favorable;           /**< Maximum favorable excursion */
    float max_adverse;             /**< Maximum adverse excursion */
    uint64_t hold_duration_ms;     /**< How long position was held */
    bool is_resolved;              /**< Whether trade is complete */
    char context[128];             /**< Trade context/reasoning */
} fin_trade_record_t;

/**
 * @brief Counterfactual scenario
 */
typedef struct {
    fin_action_t alternative_action; /**< Alternative action considered */
    float hypothetical_outcome;    /**< What outcome would have been */
    float outcome_difference;      /**< Difference from actual */
    float probability;             /**< Probability this was optimal */
    char description[FIN_REGRET_DESC_LEN]; /**< Scenario description */
} fin_counterfactual_scenario_t;

/**
 * @brief Extracted lesson
 */
typedef struct {
    uint32_t id;                   /**< Lesson ID */
    fin_regret_type_t regret_type; /**< Type of regret that led to lesson */
    char pattern[128];             /**< Recognized pattern */
    char lesson[FIN_REGRET_DESC_LEN]; /**< Lesson text */
    char action_item[128];         /**< Actionable recommendation */
    uint32_t occurrence_count;     /**< How many times pattern occurred */
    float confidence;              /**< Confidence in lesson [0, 1] */
    uint64_t first_seen_ms;        /**< First occurrence timestamp */
    uint64_t last_seen_ms;         /**< Last occurrence timestamp */
} fin_lesson_t;

/**
 * @brief Comprehensive regret assessment
 */
typedef struct {
    fin_regret_analysis_t analysis;  /**< Core analysis */
    fin_regret_type_t regret_type;   /**< Type of regret */
    float emotional_intensity;       /**< Emotional intensity [0, 1] */
    uint32_t counterfactual_count;   /**< Number of counterfactuals */
    fin_counterfactual_scenario_t counterfactuals[FIN_REGRET_MAX_COUNTERFACTUALS];
    fin_lesson_t* relevant_lessons;  /**< Pointer to relevant lessons */
    uint32_t relevant_lesson_count;  /**< Number of relevant lessons */
    char summary[FIN_REGRET_DESC_LEN]; /**< Assessment summary */
} fin_regret_assessment_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Analysis parameters */
    uint32_t min_trades_for_analysis;  /**< Minimum trades before analysis */
    uint32_t analysis_window_size;     /**< Sliding window for pattern detection */
    float regret_threshold;            /**< Minimum regret to trigger lesson */
    float lesson_confidence_threshold; /**< Minimum confidence for lesson */

    /* Counterfactual parameters */
    uint32_t max_counterfactuals;      /**< Maximum scenarios to generate */
    float counterfactual_probability_threshold; /**< Min probability to consider */

    /* Lesson extraction parameters */
    uint32_t min_pattern_occurrences;  /**< Min occurrences to form lesson */
    float pattern_similarity_threshold; /**< Similarity threshold for patterns */

    /* Integration settings */
    bool enable_immune_integration;    /**< Enable immune system checks */
    bool enable_bbb_validation;        /**< Enable BBB validation */
    bool enable_kg_messaging;          /**< Enable KG messaging */
    bool enable_health_monitoring;     /**< Enable health heartbeats */

    /* Emotional weighting */
    bool enable_emotional_weighting;   /**< Weight regret by emotional intensity */
    float emotional_decay_rate;        /**< How fast emotional intensity decays */

    /* Logging */
    bool verbose_logging;              /**< Verbose debug output */
} fin_regret_config_t;

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
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial regret bridge handle
 */
typedef struct financial_regret_bridge financial_regret_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_default_config(fin_regret_config_t* config);

/**
 * @brief Create financial regret bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_regret_bridge_t* financial_regret_bridge_create(
    const fin_regret_config_t* config
);

/**
 * @brief Destroy financial regret bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_regret_bridge_destroy(financial_regret_bridge_t* bridge);

/**
 * @brief Reset bridge state and history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_reset(financial_regret_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_regret_bridge_set_immune(
    financial_regret_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_regret_bridge_set_bbb(
    financial_regret_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_regret_bridge_set_health_agent(
    financial_regret_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_regret_bridge_set_kg_wiring(
    financial_regret_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_regret_bridge_set_logger(
    financial_regret_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_regret_bridge_set_security(
    financial_regret_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_regret_bridge_set_ethics(
    financial_regret_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_regret_bridge_set_lgss(
    financial_regret_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_regret_bridge_set_coordinator(
    financial_regret_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_regret_bridge_set_bio_router(
    financial_regret_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Trade History API
 * ============================================================================ */

/**
 * @brief Record a completed trade
 *
 * @param bridge Bridge handle
 * @param record Trade record
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_record_trade(
    financial_regret_bridge_t* bridge,
    const fin_trade_record_t* record
);

/**
 * @brief Get trade count in history
 *
 * @param bridge Bridge handle
 * @return Number of trades in history
 */
uint32_t financial_regret_bridge_get_trade_count(
    const financial_regret_bridge_t* bridge
);

/**
 * @brief Clear trade history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_clear_history(
    financial_regret_bridge_t* bridge
);

/* ============================================================================
 * Core API - Regret Analysis
 * ============================================================================ */

/**
 * @brief Analyze regret from a trade
 *
 * Computes regret magnitude by comparing actual outcome to optimal
 * hindsight outcome. Identifies what the best action would have been.
 *
 * @param bridge Bridge handle
 * @param trade Trade to analyze (uses fin_trade_t for simple interface)
 * @param action_taken Action that was taken
 * @param analysis Output analysis result
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_analyze(
    financial_regret_bridge_t* bridge,
    const fin_trade_t* trade,
    const fin_action_t* action_taken,
    fin_regret_analysis_t* analysis
);

/**
 * @brief Analyze regret with full trade record
 *
 * @param bridge Bridge handle
 * @param record Full trade record
 * @param assessment Output comprehensive assessment
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_analyze_full(
    financial_regret_bridge_t* bridge,
    const fin_trade_record_t* record,
    fin_regret_assessment_t* assessment
);

/* ============================================================================
 * Core API - Counterfactual Analysis
 * ============================================================================ */

/**
 * @brief Compute counterfactual outcome
 *
 * Generates "what if" scenarios for alternative actions.
 *
 * @param bridge Bridge handle
 * @param trade Original trade
 * @param alternative_action What if this action was taken
 * @param hypothetical_outcome Output: what outcome would have been
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_counterfactual(
    financial_regret_bridge_t* bridge,
    const fin_trade_t* trade,
    const fin_action_t* alternative_action,
    float* hypothetical_outcome
);

/**
 * @brief Generate multiple counterfactual scenarios
 *
 * @param bridge Bridge handle
 * @param trade Trade to analyze
 * @param scenarios Output array of scenarios
 * @param max_scenarios Maximum scenarios to generate
 * @param num_scenarios Output: actual number generated
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_generate_counterfactuals(
    financial_regret_bridge_t* bridge,
    const fin_trade_record_t* trade,
    fin_counterfactual_scenario_t* scenarios,
    uint32_t max_scenarios,
    uint32_t* num_scenarios
);

/* ============================================================================
 * Core API - Lesson Extraction
 * ============================================================================ */

/**
 * @brief Extract lesson from regret analysis
 *
 * Identifies patterns in the regret and generates actionable lessons.
 *
 * @param bridge Bridge handle
 * @param analysis Regret analysis result
 * @param lesson Output lesson
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_extract_lesson(
    financial_regret_bridge_t* bridge,
    const fin_regret_analysis_t* analysis,
    fin_lesson_t* lesson
);

/**
 * @brief Get all extracted lessons
 *
 * @param bridge Bridge handle
 * @param lessons Output array
 * @param max_lessons Maximum lessons to retrieve
 * @param num_lessons Output: actual number retrieved
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_get_lessons(
    financial_regret_bridge_t* bridge,
    fin_lesson_t* lessons,
    uint32_t max_lessons,
    uint32_t* num_lessons
);

/**
 * @brief Get lesson count
 *
 * @param bridge Bridge handle
 * @return Number of lessons stored
 */
uint32_t financial_regret_bridge_get_lesson_count(
    const financial_regret_bridge_t* bridge
);

/**
 * @brief Find lessons by regret type
 *
 * @param bridge Bridge handle
 * @param regret_type Type of regret to filter by
 * @param lessons Output array
 * @param max_lessons Maximum lessons to retrieve
 * @param num_lessons Output: actual number found
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_find_lessons_by_type(
    financial_regret_bridge_t* bridge,
    fin_regret_type_t regret_type,
    fin_lesson_t* lessons,
    uint32_t max_lessons,
    uint32_t* num_lessons
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_regret_bridge_state_t financial_regret_bridge_get_state(
    const financial_regret_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_get_stats(
    const financial_regret_bridge_t* bridge,
    fin_regret_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_regret_bridge_reset_stats(
    financial_regret_bridge_t* bridge
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_regret_bridge_get_last_error(void);

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
int financial_regret_bridge_heartbeat(
    financial_regret_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_regret_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get action type name
 *
 * @param action Action type
 * @return String name (static)
 */
const char* fin_regret_action_name(fin_action_type_t action);

/**
 * @brief Get regret type name
 *
 * @param type Regret type
 * @return String name (static)
 */
const char* fin_regret_type_name(fin_regret_type_t type);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_regret_state_name(fin_regret_bridge_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_regret_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_training_begin(
    financial_regret_bridge_t* bridge
);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_training_end(
    financial_regret_bridge_t* bridge
);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_regret_bridge_training_step(
    financial_regret_bridge_t* bridge,
    float progress
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_REGRET_BRIDGE_H */
