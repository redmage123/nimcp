/**
 * @file nimcp_financial_autobio_bridge.h
 * @brief Financial Autobiographical Memory Bridge
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for recording and recalling autobiographical trading episodes
 *       with emotional context and lesson extraction
 *
 * WHY:  Trading decisions benefit from experience-based learning. This bridge
 *       integrates episodic memory (specific trading events) with emotional
 *       tagging and outcome tracking to enable:
 *       - Recall of emotionally-similar past situations
 *       - Pattern recognition across past trading outcomes
 *       - Extraction of actionable lessons from experience
 *
 * HOW:  Episodes are recorded with full context (price, quantity, direction,
 *       volatility, emotional state, outcome). Recall queries filter by
 *       emotion type or outcome range. Lesson extraction summarizes patterns
 *       across multiple episodes.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |              Financial Autobiographical Memory Bridge                      |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Trading System       |       |  Episodic Memory      |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Trade Execution       |       | Episode Storage       |                |
 * |  | Portfolio Events      |       | Emotional Tags        |                |
 * |  | Market Conditions     |       | Outcome Tracking      |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Episode Recording & Recall Engine               |            |
 * |  |  event → emotional_tag → storage → query → retrieval     |            |
 * |  +----------------------------------------------------------+            |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |          Lesson Extraction & Pattern Analysis            |            |
 * |  |  episodes → cluster_analysis → lesson_distillation       |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_bridge.h
 * @see nimcp_parietal_training_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_AUTOBIO_BRIDGE_H
#define NIMCP_FINANCIAL_AUTOBIO_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_AUTOBIO_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_AUTOBIO_BRIDGE_MAGIC      0x46414242  /* 'FABB' */

/** Bio-async module ID for financial autobio bridge */
#define BIO_MODULE_FINANCIAL_AUTOBIO        0x0396

/** Maximum episodes stored in memory */
#define FIN_AUTOBIO_MAX_EPISODES            4096

/** Maximum lessons extractable per query */
#define FIN_AUTOBIO_MAX_LESSONS             64

/** Maximum description length */
#define FIN_AUTOBIO_DESC_LEN                256

/** Maximum lesson text length */
#define FIN_AUTOBIO_LESSON_LEN              512

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_AUTOBIO_ERROR_BASE              33100
#define FIN_AUTOBIO_ERR_OK                  0
#define FIN_AUTOBIO_ERR_NULL                (FIN_AUTOBIO_ERROR_BASE + 1)
#define FIN_AUTOBIO_ERR_INVALID_PARAM       (FIN_AUTOBIO_ERROR_BASE + 2)
#define FIN_AUTOBIO_ERR_NO_MEMORY           (FIN_AUTOBIO_ERROR_BASE + 3)
#define FIN_AUTOBIO_ERR_NOT_FOUND           (FIN_AUTOBIO_ERROR_BASE + 4)
#define FIN_AUTOBIO_ERR_CAPACITY            (FIN_AUTOBIO_ERROR_BASE + 5)
#define FIN_AUTOBIO_ERR_STATE               (FIN_AUTOBIO_ERROR_BASE + 6)
#define FIN_AUTOBIO_ERR_IMMUNE              (FIN_AUTOBIO_ERROR_BASE + 7)
#define FIN_AUTOBIO_ERR_BBB                 (FIN_AUTOBIO_ERROR_BASE + 8)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Financial emotional states during trading
 *
 * Emotions significantly impact trading decisions and outcomes.
 * Tracking emotional state enables experience-based pattern recognition.
 */
typedef enum {
    FIN_EMOTION_NEUTRAL = 0,    /**< Calm, balanced state */
    FIN_EMOTION_JOY,            /**< Positive from gains/success */
    FIN_EMOTION_FEAR,           /**< Anxiety about losses */
    FIN_EMOTION_ANGER,          /**< Frustration from adverse events */
    FIN_EMOTION_SURPRISE,       /**< Unexpected market moves */
    FIN_EMOTION_SADNESS,        /**< Disappointment from losses */
    FIN_EMOTION_GREED,          /**< Over-optimistic pursuit of gains */
    FIN_EMOTION_PANIC,          /**< Extreme fear, irrational behavior */
    FIN_EMOTION_COUNT
} fin_emotion_type_t;

/**
 * @brief Trade direction
 */
typedef enum {
    FIN_TRADE_BUY = 0,          /**< Long position entry */
    FIN_TRADE_SELL,             /**< Position exit or short cover */
    FIN_TRADE_SHORT,            /**< Short position entry */
    FIN_TRADE_HOLD              /**< No action taken */
} fin_trade_direction_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_AUTOBIO_STATE_UNINITIALIZED = 0,
    FIN_AUTOBIO_STATE_INITIALIZED,
    FIN_AUTOBIO_STATE_ACTIVE,
    FIN_AUTOBIO_STATE_DEGRADED,
    FIN_AUTOBIO_STATE_ERROR
} fin_autobio_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Trading episode record
 *
 * Captures a complete trading event with emotional context and outcome.
 */
typedef struct {
    uint64_t episode_id;                        /**< Unique episode identifier */
    char description[FIN_AUTOBIO_DESC_LEN];     /**< Human-readable description */
    float trade_price;                          /**< Execution price */
    float trade_quantity;                       /**< Quantity traded */
    int trade_direction;                        /**< fin_trade_direction_t value */
    float market_volatility;                    /**< Market volatility at time */
    fin_emotion_type_t emotional_state;         /**< Emotional state during trade */
    float outcome;                              /**< Realized P&L or unrealized mark */
    char lesson_learned[FIN_AUTOBIO_LESSON_LEN]; /**< Post-hoc lesson extracted */
    uint64_t timestamp_ms;                      /**< Episode timestamp */
} fin_trading_episode_t;

/**
 * @brief Lesson extracted from episode analysis
 */
typedef struct {
    char lesson_text[FIN_AUTOBIO_LESSON_LEN];   /**< Lesson description */
    fin_emotion_type_t associated_emotion;      /**< Most common emotion */
    float avg_outcome;                          /**< Average outcome in cluster */
    uint32_t episode_count;                     /**< Number of supporting episodes */
    float confidence;                           /**< Lesson confidence [0.0-1.0] */
} fin_extracted_lesson_t;

/**
 * @brief Recall query result
 */
typedef struct {
    fin_trading_episode_t* episodes;            /**< Array of matched episodes */
    uint32_t count;                             /**< Number of matches */
    uint32_t total_available;                   /**< Total matches (may exceed returned) */
} fin_recall_result_t;

/**
 * @brief Lesson extraction result
 */
typedef struct {
    fin_extracted_lesson_t* lessons;            /**< Array of extracted lessons */
    uint32_t count;                             /**< Number of lessons */
} fin_lesson_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t episodes_recorded;                 /**< Total episodes recorded */
    uint64_t emotion_queries;                   /**< Queries by emotion */
    uint64_t outcome_queries;                   /**< Queries by outcome */
    uint64_t lessons_extracted;                 /**< Lessons extracted */
    uint64_t immune_checks;                     /**< Immune system checks */
    uint64_t bbb_validations;                   /**< BBB validations performed */
    uint64_t kg_messages_sent;                  /**< KG messages published */
    uint64_t health_heartbeats;                 /**< Health heartbeats sent */
} fin_autobio_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Episode storage */
    uint32_t max_episodes;                      /**< Maximum episodes to store */
    bool enable_auto_lesson_extraction;         /**< Auto-extract on record */
    uint32_t min_episodes_for_lesson;           /**< Min episodes for lesson */

    /* Recall settings */
    uint32_t default_recall_limit;              /**< Default max results */
    float outcome_similarity_threshold;         /**< Outcome matching tolerance */

    /* Integration settings */
    bool enable_immune_integration;             /**< Enable immune system */
    bool enable_bbb_validation;                 /**< Enable BBB validation */
    bool enable_kg_messaging;                   /**< Enable KG messaging */
    bool enable_health_monitoring;              /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;                       /**< Verbose debug output */
} fin_autobio_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Episode recorded callback
 */
typedef void (*fin_autobio_episode_callback_t)(
    const fin_trading_episode_t* episode,
    void* user_data
);

/**
 * @brief Lesson extracted callback
 */
typedef void (*fin_autobio_lesson_callback_t)(
    const fin_extracted_lesson_t* lesson,
    void* user_data
);

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial autobio bridge handle
 */
typedef struct financial_autobio_bridge financial_autobio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_autobio_bridge_default_config(fin_autobio_config_t* config);

/**
 * @brief Create financial autobiographical memory bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_autobio_bridge_t* financial_autobio_bridge_create(
    const fin_autobio_config_t* config
);

/**
 * @brief Destroy financial autobiographical memory bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_autobio_bridge_destroy(financial_autobio_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_reset(financial_autobio_bridge_t* bridge);

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
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_autobio_bridge_set_immune(financial_autobio_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_autobio_bridge_set_bbb(financial_autobio_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_autobio_bridge_set_health_agent(financial_autobio_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_autobio_bridge_set_kg_wiring(financial_autobio_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_autobio_bridge_set_logger(financial_autobio_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_autobio_bridge_set_security(financial_autobio_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_autobio_bridge_set_ethics(financial_autobio_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_autobio_bridge_set_lgss(financial_autobio_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_autobio_bridge_set_coordinator(financial_autobio_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_autobio_bridge_set_bio_router(financial_autobio_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Episode Recording API
 * ============================================================================ */

/**
 * @brief Record a trading episode
 *
 * @param bridge Bridge handle
 * @param episode Episode to record
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_record_episode(
    financial_autobio_bridge_t* bridge,
    const fin_trading_episode_t* episode
);

/**
 * @brief Create and record episode from components
 *
 * @param bridge Bridge handle
 * @param description Event description
 * @param price Trade price
 * @param quantity Trade quantity
 * @param direction Trade direction
 * @param volatility Market volatility
 * @param emotion Emotional state
 * @param outcome Trade outcome
 * @param lesson Lesson learned (can be NULL)
 * @return Episode ID on success, 0 on failure
 */
uint64_t financial_autobio_bridge_record(
    financial_autobio_bridge_t* bridge,
    const char* description,
    float price,
    float quantity,
    fin_trade_direction_t direction,
    float volatility,
    fin_emotion_type_t emotion,
    float outcome,
    const char* lesson
);

/* ============================================================================
 * Recall API
 * ============================================================================ */

/**
 * @brief Recall episodes by emotional state
 *
 * @param bridge Bridge handle
 * @param emotion Emotion to filter by
 * @param max_results Maximum results to return
 * @param result Output result structure
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_recall_by_emotion(
    financial_autobio_bridge_t* bridge,
    fin_emotion_type_t emotion,
    uint32_t max_results,
    fin_recall_result_t* result
);

/**
 * @brief Recall episodes by outcome range
 *
 * @param bridge Bridge handle
 * @param min_outcome Minimum outcome value
 * @param max_outcome Maximum outcome value
 * @param max_results Maximum results to return
 * @param result Output result structure
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_recall_by_outcome(
    financial_autobio_bridge_t* bridge,
    float min_outcome,
    float max_outcome,
    uint32_t max_results,
    fin_recall_result_t* result
);

/**
 * @brief Free recall result resources
 *
 * @param result Result to free
 */
void financial_autobio_bridge_free_recall_result(fin_recall_result_t* result);

/* ============================================================================
 * Lesson Extraction API
 * ============================================================================ */

/**
 * @brief Extract lessons from recorded episodes
 *
 * @param bridge Bridge handle
 * @param emotion Optional emotion filter (FIN_EMOTION_COUNT for all)
 * @param max_lessons Maximum lessons to extract
 * @param result Output result structure
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_get_lessons(
    financial_autobio_bridge_t* bridge,
    fin_emotion_type_t emotion,
    uint32_t max_lessons,
    fin_lesson_result_t* result
);

/**
 * @brief Free lesson result resources
 *
 * @param result Result to free
 */
void financial_autobio_bridge_free_lesson_result(fin_lesson_result_t* result);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set episode recorded callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_set_episode_callback(
    financial_autobio_bridge_t* bridge,
    fin_autobio_episode_callback_t callback,
    void* user_data
);

/**
 * @brief Set lesson extracted callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_set_lesson_callback(
    financial_autobio_bridge_t* bridge,
    fin_autobio_lesson_callback_t callback,
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
fin_autobio_state_t financial_autobio_bridge_get_state(
    const financial_autobio_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_autobio_bridge_get_stats(
    const financial_autobio_bridge_t* bridge,
    fin_autobio_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_autobio_bridge_reset_stats(financial_autobio_bridge_t* bridge);

/**
 * @brief Get episode count
 *
 * @param bridge Bridge handle
 * @return Episode count
 */
uint32_t financial_autobio_bridge_get_episode_count(
    const financial_autobio_bridge_t* bridge
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_autobio_bridge_get_last_error(void);

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
int financial_autobio_bridge_heartbeat(
    financial_autobio_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get emotion name
 *
 * @param emotion Emotion type
 * @return String name (static)
 */
const char* fin_autobio_emotion_name(fin_emotion_type_t emotion);

/**
 * @brief Get direction name
 *
 * @param direction Trade direction
 * @return String name (static)
 */
const char* fin_autobio_direction_name(fin_trade_direction_t direction);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_autobio_state_name(fin_autobio_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_autobio_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_AUTOBIO_BRIDGE_H */
