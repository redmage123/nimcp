/**
 * @file nimcp_financial_consolidation_bridge.h
 * @brief Financial Consolidation Bridge - Memory Replay and Pattern Consolidation
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for consolidating financial trading patterns through memory replay,
 *       winner strengthening, and loser pruning. Implements sleep-like consolidation
 *       processes for trading pattern learning.
 *
 * WHY:  Successful trading requires:
 *       - Pattern replay: Reinforce profitable trading patterns during "rest"
 *       - Winner strengthening: Increase synaptic weights for winning patterns
 *       - Loser pruning: Remove or weaken losing patterns to prevent repetition
 *       This mirrors hippocampal memory consolidation during sleep.
 *
 * HOW:  Trade history is maintained with outcomes. During consolidation:
 *       - Profitable patterns are replayed and strengthened
 *       - Losing patterns are pruned or weakened
 *       - Pattern strengths are adjusted based on cumulative outcomes
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Consolidation Bridge                               |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Trade History        |       |  Pattern Memory       |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Price, qty, direction |       | Extracted patterns    |                |
 * |  | Outcome (+/-)         |       | Pattern strengths     |                |
 * |  | Timestamp             |       | Replay counts         |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Consolidation Engine                            |            |
 * |  |  Replay: Re-activate profitable patterns                 |            |
 * |  |  Strengthen: Increase winner pattern weights             |            |
 * |  |  Prune: Remove/weaken losing patterns                    |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                                             |
 * |             v                                                             |
 * |  +----------------------------------------------------------+            |
 * |  |          Consolidation Result                             |            |
 * |  |  - Patterns replayed count                                |            |
 * |  |  - Patterns pruned count                                  |            |
 * |  |  - Updated pattern strengths                              |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_financial_reasoning_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_CONSOLIDATION_BRIDGE_H
#define NIMCP_FINANCIAL_CONSOLIDATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_CONSOLIDATION_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_CONSOLIDATION_BRIDGE_MAGIC      0x46434242  /* 'FCBB' */

/** Bio-async module ID for financial consolidation bridge */
#define BIO_MODULE_FINANCIAL_CONSOLIDATION        0x039B

/** Maximum trades in history buffer */
#define FIN_CONSOLIDATION_MAX_TRADES              4096

/** Maximum patterns to track */
#define FIN_CONSOLIDATION_MAX_PATTERNS            512

/** Default replay batch size */
#define FIN_CONSOLIDATION_DEFAULT_BATCH_SIZE      64

/** Default strengthening factor */
#define FIN_CONSOLIDATION_DEFAULT_STRENGTHEN_RATE 0.1f

/** Default pruning threshold */
#define FIN_CONSOLIDATION_DEFAULT_PRUNE_THRESHOLD 0.2f

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_CONSOLIDATION_ERROR_BASE              33500
#define FIN_CONSOLIDATION_ERR_OK                  0
#define FIN_CONSOLIDATION_ERR_NULL                (FIN_CONSOLIDATION_ERROR_BASE + 1)
#define FIN_CONSOLIDATION_ERR_INVALID_PARAM       (FIN_CONSOLIDATION_ERROR_BASE + 2)
#define FIN_CONSOLIDATION_ERR_NO_MEMORY           (FIN_CONSOLIDATION_ERROR_BASE + 3)
#define FIN_CONSOLIDATION_ERR_STATE               (FIN_CONSOLIDATION_ERROR_BASE + 4)
#define FIN_CONSOLIDATION_ERR_IMMUNE              (FIN_CONSOLIDATION_ERROR_BASE + 5)
#define FIN_CONSOLIDATION_ERR_BBB                 (FIN_CONSOLIDATION_ERROR_BASE + 6)
#define FIN_CONSOLIDATION_ERR_HISTORY_FULL        (FIN_CONSOLIDATION_ERROR_BASE + 7)
#define FIN_CONSOLIDATION_ERR_PATTERN_FULL        (FIN_CONSOLIDATION_ERROR_BASE + 8)
#define FIN_CONSOLIDATION_ERR_NO_HISTORY          (FIN_CONSOLIDATION_ERROR_BASE + 9)
#define FIN_CONSOLIDATION_ERR_NO_PATTERNS         (FIN_CONSOLIDATION_ERROR_BASE + 10)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_CONSOLIDATION_OP_STATE_UNINITIALIZED = 0,
    FIN_CONSOLIDATION_OP_STATE_INITIALIZED,
    FIN_CONSOLIDATION_OP_STATE_ACTIVE,
    FIN_CONSOLIDATION_OP_STATE_CONSOLIDATING,
    FIN_CONSOLIDATION_OP_STATE_DEGRADED,
    FIN_CONSOLIDATION_OP_STATE_ERROR
} fin_consolidation_op_state_t;

/**
 * @brief Trade direction
 */
typedef enum {
    FIN_TRADE_DIRECTION_LONG = 1,    /**< Buy/long position */
    FIN_TRADE_DIRECTION_SHORT = -1,  /**< Sell/short position */
    FIN_TRADE_DIRECTION_FLAT = 0     /**< No position / exit */
} fin_trade_direction_t;

/**
 * @brief Consolidation mode
 */
typedef enum {
    FIN_CONSOLIDATION_MODE_FULL = 0,     /**< Full consolidation (replay + strengthen + prune) */
    FIN_CONSOLIDATION_MODE_REPLAY_ONLY,  /**< Replay patterns only */
    FIN_CONSOLIDATION_MODE_PRUNE_ONLY,   /**< Prune losing patterns only */
    FIN_CONSOLIDATION_MODE_STRENGTHEN_ONLY /**< Strengthen winning patterns only */
} fin_consolidation_mode_t;

/**
 * @brief Pattern type
 */
typedef enum {
    FIN_PATTERN_TYPE_MOMENTUM = 0,   /**< Momentum/trend pattern */
    FIN_PATTERN_TYPE_REVERSAL,       /**< Mean reversion pattern */
    FIN_PATTERN_TYPE_BREAKOUT,       /**< Breakout pattern */
    FIN_PATTERN_TYPE_SUPPORT,        /**< Support/resistance pattern */
    FIN_PATTERN_TYPE_VOLUME,         /**< Volume-based pattern */
    FIN_PATTERN_TYPE_SENTIMENT,      /**< Sentiment-based pattern */
    FIN_PATTERN_TYPE_FUNDAMENTAL,    /**< Fundamental-based pattern */
    FIN_PATTERN_TYPE_CUSTOM,         /**< Custom pattern */
    FIN_PATTERN_TYPE_COUNT
} fin_pattern_type_t;

/* ============================================================================
 * Core Data Structures (as specified)
 * ============================================================================ */

/**
 * @brief Individual trade record
 */
typedef struct {
    float price;              /**< Entry/exit price */
    float quantity;           /**< Position size */
    int direction;            /**< Trade direction (1=long, -1=short, 0=flat) */
    float outcome;            /**< Trade outcome (profit/loss) */
    uint64_t timestamp_ms;    /**< Trade timestamp in milliseconds */
} fin_trade_record_t;

/**
 * @brief Trade history buffer
 */
typedef struct {
    fin_trade_record_t* trades;   /**< Array of trade records */
    uint32_t num_trades;          /**< Number of trades in history */
} fin_trade_history_t;

/**
 * @brief Consolidation result
 */
typedef struct {
    uint32_t patterns_replayed;      /**< Number of patterns replayed */
    uint32_t patterns_pruned;        /**< Number of patterns pruned */
    float* pattern_strengths;        /**< Updated pattern strengths array */
    uint32_t num_patterns;           /**< Number of patterns in array */
} fin_consolidation_result_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t replays;            /**< Total pattern replays */
    uint64_t prunings;           /**< Total pattern prunings */
    uint64_t strengthenings;     /**< Total pattern strengthenings */
    uint64_t immune_checks;      /**< Immune system checks performed */
    uint64_t bbb_validations;    /**< BBB validations performed */
    uint64_t kg_messages_sent;   /**< KG messages published */
    uint64_t health_heartbeats;  /**< Health heartbeats sent */
} fin_consolidation_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Pattern entry in memory
 */
typedef struct {
    uint32_t pattern_id;             /**< Unique pattern ID */
    fin_pattern_type_t type;         /**< Pattern type */
    float strength;                  /**< Current pattern strength [0-1] */
    float cumulative_outcome;        /**< Cumulative P&L for pattern */
    uint32_t occurrence_count;       /**< Times pattern occurred */
    uint32_t win_count;              /**< Times pattern was profitable */
    uint32_t loss_count;             /**< Times pattern was unprofitable */
    uint64_t last_seen_ms;           /**< Last occurrence timestamp */
    uint32_t replay_count;           /**< Times replayed during consolidation */
    float win_rate;                  /**< Win rate [0-1] */
    float avg_profit;                /**< Average profit per occurrence */
    float avg_loss;                  /**< Average loss per occurrence */
    float profit_factor;             /**< Gross profit / gross loss */
    bool enabled;                    /**< Pattern active flag */
} fin_pattern_entry_t;

/**
 * @brief Trade with pattern annotation
 */
typedef struct {
    fin_trade_record_t trade;        /**< Base trade record */
    uint32_t* pattern_ids;           /**< Associated pattern IDs */
    uint32_t num_patterns;           /**< Number of associated patterns */
    float volatility;                /**< Market volatility at trade */
    float sentiment;                 /**< Market sentiment at trade */
    char symbol[32];                 /**< Trading symbol */
} fin_annotated_trade_t;

/**
 * @brief Consolidation session
 */
typedef struct {
    uint64_t session_id;             /**< Session identifier */
    uint64_t start_time_ms;          /**< Session start timestamp */
    uint64_t end_time_ms;            /**< Session end timestamp */
    fin_consolidation_mode_t mode;   /**< Consolidation mode */
    uint32_t trades_processed;       /**< Trades processed in session */
    uint32_t patterns_updated;       /**< Patterns updated in session */
    float total_strengthening;       /**< Total strength increase */
    float total_weakening;           /**< Total strength decrease */
} fin_consolidation_session_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Consolidation parameters */
    uint32_t replay_batch_size;      /**< Patterns to replay per batch */
    float strengthen_rate;           /**< Rate to strengthen winners [0-1] */
    float weaken_rate;               /**< Rate to weaken losers [0-1] */
    float prune_threshold;           /**< Strength below which to prune [0-1] */
    float min_win_rate;              /**< Minimum win rate to strengthen */
    uint32_t min_occurrences;        /**< Minimum occurrences for stats */

    /* History settings */
    uint32_t max_history_size;       /**< Maximum trade history size */
    uint32_t max_pattern_count;      /**< Maximum patterns to track */
    uint64_t history_retention_ms;   /**< How long to retain history */

    /* Consolidation scheduling */
    bool auto_consolidate;           /**< Auto-consolidate periodically */
    uint64_t consolidation_interval_ms; /**< Interval between auto-consolidations */

    /* Integration settings */
    bool enable_immune_integration;  /**< Enable immune system checks */
    bool enable_bbb_validation;      /**< Enable BBB validation */
    bool enable_kg_messaging;        /**< Enable KG messaging */
    bool enable_health_monitoring;   /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;            /**< Verbose debug output */
} fin_consolidation_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Pattern replayed callback
 */
typedef void (*fin_consolidation_replay_callback_t)(
    const fin_pattern_entry_t* pattern,
    void* user_data
);

/**
 * @brief Pattern pruned callback
 */
typedef void (*fin_consolidation_prune_callback_t)(
    const fin_pattern_entry_t* pattern,
    void* user_data
);

/**
 * @brief Pattern strengthened callback
 */
typedef void (*fin_consolidation_strengthen_callback_t)(
    const fin_pattern_entry_t* pattern,
    float old_strength,
    float new_strength,
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
 * @brief Opaque financial consolidation bridge handle
 */
typedef struct financial_consolidation_bridge financial_consolidation_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_default_config(fin_consolidation_config_t* config);

/**
 * @brief Create financial consolidation bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_consolidation_bridge_t* financial_consolidation_bridge_create(
    const fin_consolidation_config_t* config
);

/**
 * @brief Destroy financial consolidation bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_consolidation_bridge_destroy(financial_consolidation_bridge_t* bridge);

/**
 * @brief Reset bridge state (clear history and patterns)
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_reset(financial_consolidation_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_consolidation_bridge_set_immune(
    financial_consolidation_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_consolidation_bridge_set_bbb(
    financial_consolidation_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_consolidation_bridge_set_health_agent(
    financial_consolidation_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_consolidation_bridge_set_kg_wiring(
    financial_consolidation_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_consolidation_bridge_set_logger(
    financial_consolidation_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_consolidation_bridge_set_security(
    financial_consolidation_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_consolidation_bridge_set_ethics(
    financial_consolidation_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_consolidation_bridge_set_lgss(
    financial_consolidation_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_consolidation_bridge_set_coordinator(
    financial_consolidation_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_consolidation_bridge_set_bio_router(
    financial_consolidation_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Core API - Trade History Management
 * ============================================================================ */

/**
 * @brief Add a trade to history
 *
 * @param bridge Bridge handle
 * @param trade Trade record to add
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_add_trade(
    financial_consolidation_bridge_t* bridge,
    const fin_trade_record_t* trade
);

/**
 * @brief Add an annotated trade to history
 *
 * @param bridge Bridge handle
 * @param trade Annotated trade record
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_add_annotated_trade(
    financial_consolidation_bridge_t* bridge,
    const fin_annotated_trade_t* trade
);

/**
 * @brief Get trade history
 *
 * @param bridge Bridge handle
 * @param history Output history (caller allocates trades array)
 * @param max_trades Maximum trades to return
 * @return Number of trades returned, or negative error code
 */
int financial_consolidation_bridge_get_history(
    const financial_consolidation_bridge_t* bridge,
    fin_trade_history_t* history,
    uint32_t max_trades
);

/**
 * @brief Clear trade history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_clear_history(
    financial_consolidation_bridge_t* bridge
);

/**
 * @brief Get trade count
 *
 * @param bridge Bridge handle
 * @return Number of trades in history
 */
uint32_t financial_consolidation_bridge_get_trade_count(
    const financial_consolidation_bridge_t* bridge
);

/* ============================================================================
 * Core API - Pattern Management
 * ============================================================================ */

/**
 * @brief Register a pattern
 *
 * @param bridge Bridge handle
 * @param type Pattern type
 * @param initial_strength Initial pattern strength [0-1]
 * @return Pattern ID on success (>=0), negative error code on failure
 */
int financial_consolidation_bridge_register_pattern(
    financial_consolidation_bridge_t* bridge,
    fin_pattern_type_t type,
    float initial_strength
);

/**
 * @brief Associate a pattern with a trade
 *
 * @param bridge Bridge handle
 * @param trade_index Index of trade in history
 * @param pattern_id Pattern ID to associate
 * @param outcome Outcome attributed to this pattern
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_associate_pattern(
    financial_consolidation_bridge_t* bridge,
    uint32_t trade_index,
    uint32_t pattern_id,
    float outcome
);

/**
 * @brief Get pattern by ID
 *
 * @param bridge Bridge handle
 * @param pattern_id Pattern ID
 * @param pattern Output pattern entry
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_get_pattern(
    const financial_consolidation_bridge_t* bridge,
    uint32_t pattern_id,
    fin_pattern_entry_t* pattern
);

/**
 * @brief Get pattern count
 *
 * @param bridge Bridge handle
 * @return Number of registered patterns
 */
uint32_t financial_consolidation_bridge_get_pattern_count(
    const financial_consolidation_bridge_t* bridge
);

/**
 * @brief Clear all patterns
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_clear_patterns(
    financial_consolidation_bridge_t* bridge
);

/* ============================================================================
 * Core API - Consolidation Operations
 * ============================================================================ */

/**
 * @brief Replay profitable patterns
 *
 * Re-activates and reinforces winning patterns from trade history.
 * This simulates memory replay during sleep-like consolidation.
 *
 * @param bridge Bridge handle
 * @param result Output consolidation result (caller must free pattern_strengths)
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_replay(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_result_t* result
);

/**
 * @brief Prune losing patterns
 *
 * Removes or weakens patterns associated with losing trades.
 * Patterns below prune_threshold are removed entirely.
 *
 * @param bridge Bridge handle
 * @param result Output consolidation result
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_prune_losers(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_result_t* result
);

/**
 * @brief Strengthen winning patterns
 *
 * Increases strength of patterns associated with profitable trades.
 * Strength increases proportional to pattern win rate and profit factor.
 *
 * @param bridge Bridge handle
 * @param result Output consolidation result
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_strengthen_winners(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_result_t* result
);

/**
 * @brief Run full consolidation cycle
 *
 * Performs complete consolidation: replay, prune, strengthen.
 *
 * @param bridge Bridge handle
 * @param mode Consolidation mode
 * @param result Output consolidation result
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_consolidate(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_mode_t mode,
    fin_consolidation_result_t* result
);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Free a consolidation result
 *
 * @param result Result to free (NULL safe)
 */
void financial_consolidation_result_free(fin_consolidation_result_t* result);

/**
 * @brief Initialize a consolidation result (before use)
 *
 * @param result Result to initialize
 */
void financial_consolidation_result_init(fin_consolidation_result_t* result);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set pattern replayed callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_set_replay_callback(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_replay_callback_t callback,
    void* user_data
);

/**
 * @brief Set pattern pruned callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_set_prune_callback(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_prune_callback_t callback,
    void* user_data
);

/**
 * @brief Set pattern strengthened callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_set_strengthen_callback(
    financial_consolidation_bridge_t* bridge,
    fin_consolidation_strengthen_callback_t callback,
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
fin_consolidation_op_state_t financial_consolidation_bridge_get_op_state(
    const financial_consolidation_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_consolidation_bridge_get_stats(
    const financial_consolidation_bridge_t* bridge,
    fin_consolidation_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_consolidation_bridge_reset_stats(financial_consolidation_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_consolidation_bridge_get_last_error(void);

/**
 * @brief Get top N patterns by strength
 *
 * @param bridge Bridge handle
 * @param patterns Output array (caller allocates)
 * @param max_patterns Maximum patterns to return
 * @return Number of patterns returned, or negative error code
 */
int financial_consolidation_bridge_get_top_patterns(
    const financial_consolidation_bridge_t* bridge,
    fin_pattern_entry_t* patterns,
    uint32_t max_patterns
);

/**
 * @brief Get patterns by type
 *
 * @param bridge Bridge handle
 * @param type Pattern type filter
 * @param patterns Output array (caller allocates)
 * @param max_patterns Maximum patterns to return
 * @return Number of patterns returned, or negative error code
 */
int financial_consolidation_bridge_get_patterns_by_type(
    const financial_consolidation_bridge_t* bridge,
    fin_pattern_type_t type,
    fin_pattern_entry_t* patterns,
    uint32_t max_patterns
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
int financial_consolidation_bridge_heartbeat(
    financial_consolidation_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_consolidation_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get operational state name
 *
 * @param state Operational state
 * @return String name (static)
 */
const char* fin_consolidation_op_state_name(fin_consolidation_op_state_t state);

/**
 * @brief Get pattern type name
 *
 * @param type Pattern type
 * @return String name (static)
 */
const char* fin_consolidation_pattern_type_name(fin_pattern_type_t type);

/**
 * @brief Get consolidation mode name
 *
 * @param mode Consolidation mode
 * @return String name (static)
 */
const char* fin_consolidation_mode_name(fin_consolidation_mode_t mode);

/**
 * @brief Get trade direction name
 *
 * @param direction Trade direction
 * @return String name (static)
 */
const char* fin_consolidation_direction_name(fin_trade_direction_t direction);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_consolidation_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_CONSOLIDATION_BRIDGE_H */
