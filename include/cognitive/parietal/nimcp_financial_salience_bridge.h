/**
 * @file nimcp_financial_salience_bridge.h
 * @brief Financial Salience Bridge - Attention prioritization for market events
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for evaluating, filtering, and ranking financial market events
 *       based on salience (novelty, surprise, urgency, relevance). Part of the
 *       Phase 5 Attention Systems integration.
 *
 * WHY:  In high-frequency trading environments, thousands of events occur per
 *       second. Not all events deserve attention. This bridge enables:
 *       - Real-time salience scoring based on multiple dimensions
 *       - Filtering events below attention thresholds
 *       - Ranking events for priority processing
 *       - Integration with attention allocation systems
 *
 * HOW:  Events are scored across four salience dimensions:
 *       - Novelty: How different from recent events (deviation from EMA)
 *       - Surprise: Prediction error magnitude (unexpected vs predicted)
 *       - Urgency: Time-sensitive requiring immediate action
 *       - Relevance: Relevant to current positions/watchlist
 *       Combined score uses configurable weights for filtering/ranking.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Financial Salience Bridge                              |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |     Market Events         |       |    Salience Dimensions    |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Price changes             |       | Novelty (deviation)       |        |
 * |  | Volume spikes             |       | Surprise (prediction err) |        |
 * |  | News events               |       | Urgency (time pressure)   |        |
 * |  | Order flow changes        |       | Relevance (to portfolio)  |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |              Salience Evaluation Engine                   |            |
 * |  |  event -> score_dimensions -> combine -> threshold_check  |            |
 * |  +----------------------------------------------------------+            |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +---------------------------+       +---------------------------+        |
 * |  |     Event Filtering       |       |     Event Ranking         |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Above-threshold events    |       | Priority-sorted events    |        |
 * |  | Attention-worthy only     |       | Top-K extraction          |        |
 * |  +---------------------------+       +---------------------------+        |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_working_memory_bridge.h
 * @see nimcp_financial_emotion_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_SALIENCE_BRIDGE_H
#define NIMCP_FINANCIAL_SALIENCE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_SALIENCE_BRIDGE_VERSION   "1.0.0"
#define FINANCIAL_SALIENCE_BRIDGE_MAGIC     0x4653414C  /* 'FSAL' */

/** Bio-async module ID for financial salience bridge */
#define BIO_MODULE_FINANCIAL_SALIENCE       0x0398

/** Maximum symbol length */
#define FIN_SALIENCE_MAX_SYMBOL             16

/** Maximum events for batch operations */
#define FIN_SALIENCE_MAX_BATCH              1024

/** Default history size for novelty calculation */
#define FIN_SALIENCE_HISTORY_SIZE           128

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_SALIENCE_ERROR_BASE             33300
#define FIN_SALIENCE_ERR_OK                 0
#define FIN_SALIENCE_ERR_NULL               (FIN_SALIENCE_ERROR_BASE + 1)
#define FIN_SALIENCE_ERR_INVALID_PARAM      (FIN_SALIENCE_ERROR_BASE + 2)
#define FIN_SALIENCE_ERR_NO_MEMORY          (FIN_SALIENCE_ERROR_BASE + 3)
#define FIN_SALIENCE_ERR_STATE              (FIN_SALIENCE_ERROR_BASE + 4)
#define FIN_SALIENCE_ERR_IMMUNE             (FIN_SALIENCE_ERROR_BASE + 5)
#define FIN_SALIENCE_ERR_BBB                (FIN_SALIENCE_ERROR_BASE + 6)
#define FIN_SALIENCE_ERR_VALIDATION         (FIN_SALIENCE_ERROR_BASE + 7)
#define FIN_SALIENCE_ERR_CAPACITY           (FIN_SALIENCE_ERROR_BASE + 8)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Market event types for salience evaluation
 */
typedef enum {
    FIN_SAL_EVENT_PRICE_CHANGE = 0,     /**< Price movement */
    FIN_SAL_EVENT_VOLUME_SPIKE,          /**< Abnormal volume */
    FIN_SAL_EVENT_VOLATILITY_CHANGE,     /**< Volatility shift */
    FIN_SAL_EVENT_ORDER_IMBALANCE,       /**< Order flow imbalance */
    FIN_SAL_EVENT_NEWS,                  /**< News/announcement */
    FIN_SAL_EVENT_EARNINGS,              /**< Earnings release */
    FIN_SAL_EVENT_DIVIDEND,              /**< Dividend announcement */
    FIN_SAL_EVENT_SPLIT,                 /**< Stock split */
    FIN_SAL_EVENT_HALT,                  /**< Trading halt */
    FIN_SAL_EVENT_CIRCUIT_BREAKER,       /**< Circuit breaker triggered */
    FIN_SAL_EVENT_REGIME_CHANGE,         /**< Market regime shift */
    FIN_SAL_EVENT_CUSTOM,                /**< Custom event type */
    FIN_SAL_EVENT_COUNT
} fin_salience_event_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_SALIENCE_STATE_UNINITIALIZED = 0,
    FIN_SALIENCE_STATE_INITIALIZED,
    FIN_SALIENCE_STATE_ACTIVE,
    FIN_SALIENCE_STATE_DEGRADED,
    FIN_SALIENCE_STATE_ERROR
} fin_salience_bridge_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Salience score with component breakdown
 *
 * All values in range [0.0, 1.0] representing intensity.
 */
typedef struct {
    float novelty;      /**< How different from recent events */
    float surprise;     /**< Prediction error magnitude */
    float urgency;      /**< Requires immediate action */
    float relevance;    /**< Relevant to current positions */
    float combined;     /**< Weighted combination */
} fin_salience_score_t;

/**
 * @brief Financial market event for salience evaluation
 */
typedef struct {
    int event_type;             /**< Event type (fin_salience_event_type_t) */
    float magnitude;            /**< Event magnitude [-1.0, 1.0] or absolute */
    float price_change_pct;     /**< Price change percentage */
    float volume_ratio;         /**< Volume vs average (1.0 = normal) */
    uint64_t timestamp_ms;      /**< Event timestamp */
    char symbol[FIN_SALIENCE_MAX_SYMBOL];  /**< Instrument symbol */
} fin_market_event_t;

/**
 * @brief Event with salience score attached
 */
typedef struct {
    fin_market_event_t event;   /**< Original event */
    fin_salience_score_t score; /**< Computed salience score */
} fin_scored_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t evaluations;       /**< Total salience evaluations */
    uint64_t filters_applied;   /**< Filter operations performed */
    uint64_t rankings;          /**< Ranking operations performed */
    uint64_t immune_checks;     /**< Immune system checks */
    uint64_t bbb_validations;   /**< BBB validations performed */
    uint64_t kg_messages_sent;  /**< KG messages published */
    uint64_t health_heartbeats; /**< Health heartbeats sent */
} fin_salience_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Salience dimension weights
 */
typedef struct {
    float novelty_weight;       /**< Weight for novelty [0,1] */
    float surprise_weight;      /**< Weight for surprise [0,1] */
    float urgency_weight;       /**< Weight for urgency [0,1] */
    float relevance_weight;     /**< Weight for relevance [0,1] */
} fin_salience_weights_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Salience weights (normalized internally) */
    fin_salience_weights_t weights;

    /* Thresholds */
    float attention_threshold;          /**< Minimum combined score for attention */
    float high_priority_threshold;      /**< Threshold for high priority events */

    /* Novelty calculation */
    float ema_alpha;                    /**< EMA smoothing factor [0,1] */
    uint32_t history_size;              /**< History size for novelty */

    /* Urgency parameters */
    float base_urgency_decay;           /**< Urgency decay per second */

    /* Integration settings */
    bool enable_immune_integration;     /**< Enable immune system */
    bool enable_bbb_validation;         /**< Enable BBB validation */
    bool enable_kg_messaging;           /**< Enable KG messaging */
    bool enable_health_monitoring;      /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;               /**< Verbose debug output */
} fin_salience_config_t;

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
 * @brief Opaque financial salience bridge handle
 */
typedef struct financial_salience_bridge financial_salience_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_salience_bridge_default_config(fin_salience_config_t* config);

/**
 * @brief Create financial salience bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_salience_bridge_t* financial_salience_bridge_create(
    const fin_salience_config_t* config
);

/**
 * @brief Destroy financial salience bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_salience_bridge_destroy(financial_salience_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_reset(financial_salience_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_salience_bridge_set_immune(financial_salience_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_salience_bridge_set_bbb(financial_salience_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_salience_bridge_set_health_agent(financial_salience_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_salience_bridge_set_kg_wiring(financial_salience_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_salience_bridge_set_logger(financial_salience_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_salience_bridge_set_security(financial_salience_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_salience_bridge_set_ethics(financial_salience_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_salience_bridge_set_lgss(financial_salience_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_salience_bridge_set_coordinator(financial_salience_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_salience_bridge_set_bio_router(financial_salience_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Core Salience API
 * ============================================================================ */

/**
 * @brief Evaluate salience of a single market event
 *
 * Computes salience score across all dimensions (novelty, surprise,
 * urgency, relevance) and combines them using configured weights.
 *
 * @param bridge Bridge handle
 * @param event Market event to evaluate
 * @param score Output salience score
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_evaluate(
    financial_salience_bridge_t* bridge,
    const fin_market_event_t* event,
    fin_salience_score_t* score
);

/**
 * @brief Filter events by salience threshold
 *
 * Takes an array of events, evaluates each for salience, and returns
 * only those meeting the attention threshold.
 *
 * @param bridge Bridge handle
 * @param events Input events array
 * @param event_count Number of input events
 * @param output Output array for filtered events (must have event_count capacity)
 * @param output_count Output: number of events passing filter
 * @param threshold Salience threshold (0 = use config default)
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_filter(
    financial_salience_bridge_t* bridge,
    const fin_market_event_t* events,
    size_t event_count,
    fin_scored_event_t* output,
    size_t* output_count,
    float threshold
);

/**
 * @brief Rank events by salience score
 *
 * Takes an array of events, evaluates each for salience, and returns
 * them sorted by combined salience score (highest first).
 *
 * @param bridge Bridge handle
 * @param events Input events array
 * @param event_count Number of input events
 * @param output Output array for ranked events (must have event_count capacity)
 * @param top_k Maximum events to return (0 = return all)
 * @param output_count Output: number of events returned
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_rank(
    financial_salience_bridge_t* bridge,
    const fin_market_event_t* events,
    size_t event_count,
    fin_scored_event_t* output,
    size_t top_k,
    size_t* output_count
);

/**
 * @brief Update relevance for a symbol
 *
 * Marks a symbol as relevant (e.g., in current portfolio or watchlist),
 * which affects relevance scoring for future events.
 *
 * @param bridge Bridge handle
 * @param symbol Symbol to update
 * @param relevance Relevance level [0,1]
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_set_symbol_relevance(
    financial_salience_bridge_t* bridge,
    const char* symbol,
    float relevance
);

/**
 * @brief Clear all symbol relevance data
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_clear_relevance(financial_salience_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current state
 */
fin_salience_bridge_state_t financial_salience_bridge_get_state(
    const financial_salience_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_get_stats(
    const financial_salience_bridge_t* bridge,
    fin_salience_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_salience_bridge_reset_stats(financial_salience_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_salience_bridge_get_last_error(void);

/**
 * @brief Get current salience weights
 *
 * @param bridge Bridge handle
 * @param weights Output weights
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_get_weights(
    const financial_salience_bridge_t* bridge,
    fin_salience_weights_t* weights
);

/**
 * @brief Update salience weights dynamically
 *
 * @param bridge Bridge handle
 * @param weights New weights (will be normalized)
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_set_weights(
    financial_salience_bridge_t* bridge,
    const fin_salience_weights_t* weights
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
int financial_salience_bridge_heartbeat(
    financial_salience_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get event type name
 *
 * @param event_type Event type
 * @return String name (static)
 */
const char* fin_salience_event_name(fin_salience_event_type_t event_type);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_salience_state_name(fin_salience_bridge_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_salience_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_training_begin(financial_salience_bridge_t* bridge);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_training_end(financial_salience_bridge_t* bridge);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_salience_bridge_training_step(financial_salience_bridge_t* bridge, float progress);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_SALIENCE_BRIDGE_H */
