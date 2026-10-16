/**
 * @file nimcp_financial_tom_bridge.h
 * @brief Financial Theory of Mind Bridge
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling investor mental states, predicting actions,
 *       detecting false beliefs, and aggregating market sentiment
 *
 * WHY:  Financial markets are driven by human cognition. Understanding
 *       what investors believe, desire, and intend enables:
 *       - Prediction of market participant behavior
 *       - Detection of cognitive biases and misconceptions
 *       - Aggregation of collective market sentiment
 *       - Anticipation of market regime shifts
 *
 * HOW:  Each investor is modeled using BDI (Belief-Desire-Intention)
 *       framework combined with archetype-specific heuristics. Models
 *       are updated based on observed behavior and market conditions.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                 Financial Theory of Mind Bridge                           |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |  Investor Archetypes  |       |  Mental State Model   |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Graham (Value)        |       | Beliefs[16]           |                |
 * |  | Buffett (Moat)        |       | Desires[8]            |                |
 * |  | Soros (Reflexivity)   |       | Intentions[8]         |                |
 * |  | Lynch (Growth/GARP)   |       | Emotional State       |                |
 * |  | Templeton (Contrarian)|       | Confidence Level      |                |
 * |  | Dalio (All-Weather)   |       +-----------+-----------+                |
 * |  | Simons (Quant)        |                   |                            |
 * |  | Fisher (Scuttlebutt)  |                   v                            |
 * |  | Munger (Mental Models)|       +-----------------------+                |
 * |  | Livermore (Momentum)  |       |  Action Prediction    |                |
 * |  +----------+------------+       +-----------------------+                |
 * |             |                    | BUY/SELL/HOLD         |                |
 * |             v                    | Position Sizing       |                |
 * |  +----------------------------------------------------------+            |
 * |  |          False Belief Detection & Sentiment Aggregation  |            |
 * |  |  models[] → belief_analysis → misconception_detection    |            |
 * |  |  models[] → sentiment_weights → aggregate_sentiment      |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_investor_archetype.h
 * @see nimcp_financial_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_TOM_BRIDGE_H
#define NIMCP_FINANCIAL_TOM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_TOM_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_TOM_BRIDGE_MAGIC      0x46544F4D  /* 'FTOM' */

/** Bio-async module ID for financial ToM bridge */
#define BIO_MODULE_FINANCIAL_TOM        0x0398

/** Maximum investor models per bridge */
#define FIN_TOM_MAX_MODELS              256

/** Maximum beliefs per model */
#define FIN_TOM_MAX_BELIEFS             16

/** Maximum desires per model */
#define FIN_TOM_MAX_DESIRES             8

/** Maximum intentions per model */
#define FIN_TOM_MAX_INTENTIONS          8

/** Maximum false belief detections per query */
#define FIN_TOM_MAX_FALSE_BELIEFS       32

/** Maximum models in sentiment aggregation */
#define FIN_TOM_MAX_SENTIMENT_MODELS    64

/** Investor ID length */
#define FIN_TOM_INVESTOR_ID_LEN         64

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_TOM_ERROR_BASE              33200
#define FIN_TOM_ERR_OK                  0
#define FIN_TOM_ERR_NULL                (FIN_TOM_ERROR_BASE + 1)
#define FIN_TOM_ERR_INVALID_PARAM       (FIN_TOM_ERROR_BASE + 2)
#define FIN_TOM_ERR_NO_MEMORY           (FIN_TOM_ERROR_BASE + 3)
#define FIN_TOM_ERR_NOT_FOUND           (FIN_TOM_ERROR_BASE + 4)
#define FIN_TOM_ERR_CAPACITY            (FIN_TOM_ERROR_BASE + 5)
#define FIN_TOM_ERR_STATE               (FIN_TOM_ERROR_BASE + 6)
#define FIN_TOM_ERR_IMMUNE              (FIN_TOM_ERROR_BASE + 7)
#define FIN_TOM_ERR_BBB                 (FIN_TOM_ERROR_BASE + 8)
#define FIN_TOM_ERR_ARCHETYPE           (FIN_TOM_ERROR_BASE + 9)
#define FIN_TOM_ERR_MODEL               (FIN_TOM_ERROR_BASE + 10)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Legendary investor archetypes
 *
 * Each archetype embodies distinct investment philosophies and cognitive styles
 * that influence belief formation, desire priorities, and action intentions.
 */
typedef enum {
    FIN_TOM_ARCHETYPE_GRAHAM = 0,   /**< Benjamin Graham: Deep value, margin of safety */
    FIN_TOM_ARCHETYPE_BUFFETT,      /**< Warren Buffett: Moat, quality at fair price */
    FIN_TOM_ARCHETYPE_SOROS,        /**< George Soros: Reflexivity, macro trends */
    FIN_TOM_ARCHETYPE_LYNCH,        /**< Peter Lynch: Growth at reasonable price */
    FIN_TOM_ARCHETYPE_TEMPLETON,    /**< John Templeton: Global contrarian */
    FIN_TOM_ARCHETYPE_DALIO,        /**< Ray Dalio: Risk parity, all-weather */
    FIN_TOM_ARCHETYPE_SIMONS,       /**< Jim Simons: Quantitative, statistical */
    FIN_TOM_ARCHETYPE_FISHER,       /**< Philip Fisher: Scuttlebutt, long-term growth */
    FIN_TOM_ARCHETYPE_MUNGER,       /**< Charlie Munger: Mental models, inversion */
    FIN_TOM_ARCHETYPE_LIVERMORE,    /**< Jesse Livermore: Momentum, pivotal points */
    FIN_TOM_ARCHETYPE_COUNT
} fin_tom_archetype_t;

/**
 * @brief Emotional states affecting investor cognition
 */
typedef enum {
    FIN_TOM_EMOTION_NEUTRAL = 0,    /**< Calm, rational state */
    FIN_TOM_EMOTION_FEAR,           /**< Risk aversion, pessimism */
    FIN_TOM_EMOTION_GREED,          /**< Risk seeking, over-optimism */
    FIN_TOM_EMOTION_PANIC,          /**< Irrational fear, flight response */
    FIN_TOM_EMOTION_EUPHORIA,       /**< Irrational exuberance */
    FIN_TOM_EMOTION_FRUSTRATION,    /**< Cognitive dissonance */
    FIN_TOM_EMOTION_CONFIDENCE,     /**< Positive certainty */
    FIN_TOM_EMOTION_DOUBT,          /**< Negative uncertainty */
    FIN_TOM_EMOTION_COUNT
} fin_tom_emotion_t;

/**
 * @brief Predicted investor actions
 */
typedef enum {
    FIN_TOM_ACTION_STRONG_BUY = 0,  /**< High conviction long entry */
    FIN_TOM_ACTION_BUY,             /**< Standard long entry */
    FIN_TOM_ACTION_HOLD,            /**< No position change */
    FIN_TOM_ACTION_REDUCE,          /**< Partial position exit */
    FIN_TOM_ACTION_SELL,            /**< Full position exit */
    FIN_TOM_ACTION_SHORT,           /**< Short position entry */
    FIN_TOM_ACTION_COVER,           /**< Short position exit */
    FIN_TOM_ACTION_COUNT
} fin_tom_action_t;

/**
 * @brief False belief categories
 */
typedef enum {
    FIN_TOM_FALSE_BELIEF_NONE = 0,           /**< No false belief detected */
    FIN_TOM_FALSE_BELIEF_OVERVALUATION,      /**< Asset overvalued */
    FIN_TOM_FALSE_BELIEF_UNDERVALUATION,     /**< Asset undervalued */
    FIN_TOM_FALSE_BELIEF_TREND_CONTINUATION, /**< Trend will continue */
    FIN_TOM_FALSE_BELIEF_TREND_REVERSAL,     /**< Trend will reverse */
    FIN_TOM_FALSE_BELIEF_INFORMATION_EDGE,   /**< Believes has edge */
    FIN_TOM_FALSE_BELIEF_SKILL_ATTRIBUTION,  /**< Attributes luck to skill */
    FIN_TOM_FALSE_BELIEF_CONSENSUS_VALIDITY, /**< Believes consensus is right */
    FIN_TOM_FALSE_BELIEF_COUNT
} fin_tom_false_belief_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_TOM_STATE_UNINITIALIZED = 0,
    FIN_TOM_STATE_INITIALIZED,
    FIN_TOM_STATE_ACTIVE,
    FIN_TOM_STATE_DEGRADED,
    FIN_TOM_STATE_ERROR
} fin_tom_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Investor mental model using BDI framework
 *
 * Models the cognitive state of an investor including beliefs about
 * market conditions, desires for outcomes, and intentions for action.
 */
typedef struct {
    char investor_id[FIN_TOM_INVESTOR_ID_LEN];  /**< Unique investor identifier */
    fin_tom_archetype_t archetype;              /**< Primary cognitive archetype */
    float beliefs[FIN_TOM_MAX_BELIEFS];         /**< Belief strengths [-1.0, 1.0] */
    float desires[FIN_TOM_MAX_DESIRES];         /**< Desire intensities [0.0, 1.0] */
    float intentions[FIN_TOM_MAX_INTENTIONS];   /**< Intention strengths [0.0, 1.0] */
    int emotional_state;                        /**< fin_tom_emotion_t value */
    float confidence;                           /**< Overall confidence [0.0, 1.0] */
} fin_investor_model_t;

/**
 * @brief Predicted investor action with reasoning
 */
typedef struct {
    fin_tom_action_t action;                    /**< Predicted action */
    float probability;                          /**< Action probability [0.0, 1.0] */
    float conviction;                           /**< Conviction strength [0.0, 1.0] */
    float position_size_pct;                    /**< Suggested position size */
    char rationale[256];                        /**< Human-readable rationale */
    uint64_t prediction_timestamp_ms;           /**< When prediction was made */
} fin_tom_action_prediction_t;

/**
 * @brief Detected false belief with evidence
 */
typedef struct {
    fin_tom_false_belief_type_t type;           /**< Type of false belief */
    float severity;                             /**< Severity [0.0, 1.0] */
    float confidence;                           /**< Detection confidence [0.0, 1.0] */
    char description[256];                      /**< Description of misconception */
    char evidence[256];                         /**< Supporting evidence */
    char investor_id[FIN_TOM_INVESTOR_ID_LEN];  /**< Affected investor */
} fin_tom_false_belief_t;

/**
 * @brief False belief detection result
 */
typedef struct {
    fin_tom_false_belief_t* beliefs;            /**< Array of detected beliefs */
    uint32_t count;                             /**< Number of detections */
    uint32_t total_analyzed;                    /**< Models analyzed */
} fin_tom_false_belief_result_t;

/**
 * @brief Aggregated market sentiment
 */
typedef struct {
    float bullish_pct;                          /**< Percentage bullish [0.0, 1.0] */
    float bearish_pct;                          /**< Percentage bearish [0.0, 1.0] */
    float neutral_pct;                          /**< Percentage neutral [0.0, 1.0] */
    float consensus_strength;                   /**< How aligned are views [0.0, 1.0] */
    float conviction_avg;                       /**< Average conviction */
    float fear_level;                           /**< Aggregate fear [0.0, 1.0] */
    float greed_level;                          /**< Aggregate greed [0.0, 1.0] */
    uint32_t models_included;                   /**< Models in aggregation */
    uint64_t timestamp_ms;                      /**< Aggregation timestamp */
} fin_tom_sentiment_t;

/**
 * @brief Market context for model updates and predictions
 */
typedef struct {
    float current_price;                        /**< Current market price */
    float price_change_pct;                     /**< Recent price change */
    float volatility;                           /**< Current volatility */
    float volume_ratio;                         /**< Volume vs average */
    float market_fear_greed;                    /**< Market-wide fear/greed */
    float sector_momentum;                      /**< Sector momentum */
    bool is_trending;                           /**< In trend state */
    bool is_ranging;                            /**< In range state */
} fin_tom_market_context_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t models_created;                    /**< Total models created */
    uint64_t action_predictions;                /**< Action predictions made */
    uint64_t false_belief_detections;           /**< False beliefs detected */
    uint64_t sentiment_aggregations;            /**< Sentiment aggregations */
    uint64_t immune_checks;                     /**< Immune system checks */
    uint64_t bbb_validations;                   /**< BBB validations performed */
    uint64_t kg_messages_sent;                  /**< KG messages published */
    uint64_t health_heartbeats;                 /**< Health heartbeats sent */
} fin_tom_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Model settings */
    uint32_t max_models;                        /**< Maximum investor models */
    bool enable_archetype_weighting;            /**< Weight by archetype */
    float default_confidence;                   /**< Default confidence level */

    /* Prediction settings */
    float action_threshold;                     /**< Min probability for action */
    bool enable_rationale_generation;           /**< Generate explanations */

    /* False belief detection */
    float false_belief_threshold;               /**< Detection threshold */
    bool enable_cross_model_detection;          /**< Compare across models */

    /* Sentiment aggregation */
    bool weight_by_confidence;                  /**< Weight by model confidence */
    bool weight_by_archetype_track_record;      /**< Weight by past accuracy */

    /* Integration settings */
    bool enable_immune_integration;             /**< Enable immune system */
    bool enable_bbb_validation;                 /**< Enable BBB validation */
    bool enable_kg_messaging;                   /**< Enable KG messaging */
    bool enable_health_monitoring;              /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;                       /**< Verbose debug output */
} fin_tom_config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Model created/updated callback
 */
typedef void (*fin_tom_model_callback_t)(
    const fin_investor_model_t* model,
    void* user_data
);

/**
 * @brief False belief detected callback
 */
typedef void (*fin_tom_false_belief_callback_t)(
    const fin_tom_false_belief_t* belief,
    void* user_data
);

/* ============================================================================
 * Bridge Handle
 * ============================================================================ */

/**
 * @brief Opaque financial ToM bridge handle
 */
typedef struct financial_tom_bridge financial_tom_bridge_t;

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
int financial_tom_bridge_default_config(fin_tom_config_t* config);

/**
 * @brief Create financial theory of mind bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_tom_bridge_t* financial_tom_bridge_create(
    const fin_tom_config_t* config
);

/**
 * @brief Destroy financial theory of mind bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_tom_bridge_destroy(financial_tom_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_reset(financial_tom_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_tom_bridge_set_immune(financial_tom_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_tom_bridge_set_bbb(financial_tom_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_tom_bridge_set_health_agent(financial_tom_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_tom_bridge_set_kg_wiring(financial_tom_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_tom_bridge_set_logger(financial_tom_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_tom_bridge_set_security(financial_tom_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_tom_bridge_set_ethics(financial_tom_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_tom_bridge_set_lgss(financial_tom_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_tom_bridge_set_coordinator(financial_tom_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_tom_bridge_set_bio_router(financial_tom_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Investor Model API
 * ============================================================================ */

/**
 * @brief Create investor mental model
 *
 * @param bridge Bridge handle
 * @param investor_id Unique investor identifier
 * @param archetype Primary cognitive archetype
 * @param initial_confidence Initial confidence level [0.0, 1.0]
 * @param out_model Output model (optional)
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_model_investor(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    fin_tom_archetype_t archetype,
    float initial_confidence,
    fin_investor_model_t* out_model
);

/**
 * @brief Update investor model beliefs
 *
 * @param bridge Bridge handle
 * @param investor_id Investor to update
 * @param beliefs New belief values (NULL = no change)
 * @param num_beliefs Number of beliefs to update
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_update_beliefs(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const float* beliefs,
    uint32_t num_beliefs
);

/**
 * @brief Update investor model desires
 *
 * @param bridge Bridge handle
 * @param investor_id Investor to update
 * @param desires New desire values (NULL = no change)
 * @param num_desires Number of desires to update
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_update_desires(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const float* desires,
    uint32_t num_desires
);

/**
 * @brief Update investor emotional state
 *
 * @param bridge Bridge handle
 * @param investor_id Investor to update
 * @param emotion New emotional state
 * @param confidence New confidence level
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_update_emotion(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    fin_tom_emotion_t emotion,
    float confidence
);

/**
 * @brief Get investor model by ID
 *
 * @param bridge Bridge handle
 * @param investor_id Investor identifier
 * @param out_model Output model
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_get_model(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    fin_investor_model_t* out_model
);

/**
 * @brief Remove investor model
 *
 * @param bridge Bridge handle
 * @param investor_id Investor to remove
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_remove_model(
    financial_tom_bridge_t* bridge,
    const char* investor_id
);

/* ============================================================================
 * Action Prediction API
 * ============================================================================ */

/**
 * @brief Predict investor action given market context
 *
 * @param bridge Bridge handle
 * @param investor_id Investor to predict
 * @param context Market context
 * @param out_prediction Output prediction
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_predict_action(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const fin_tom_market_context_t* context,
    fin_tom_action_prediction_t* out_prediction
);

/**
 * @brief Predict actions for multiple investors
 *
 * @param bridge Bridge handle
 * @param investor_ids Array of investor IDs (NULL = all)
 * @param num_investors Number of investors (0 = all)
 * @param context Market context
 * @param out_predictions Output predictions array
 * @param max_predictions Maximum predictions to return
 * @param out_count Actual count returned
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_predict_actions_batch(
    financial_tom_bridge_t* bridge,
    const char** investor_ids,
    uint32_t num_investors,
    const fin_tom_market_context_t* context,
    fin_tom_action_prediction_t* out_predictions,
    uint32_t max_predictions,
    uint32_t* out_count
);

/* ============================================================================
 * False Belief Detection API
 * ============================================================================ */

/**
 * @brief Detect false beliefs in investor model
 *
 * @param bridge Bridge handle
 * @param investor_id Investor to analyze
 * @param context Market context (ground truth)
 * @param out_result Output result
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_detect_false_belief(
    financial_tom_bridge_t* bridge,
    const char* investor_id,
    const fin_tom_market_context_t* context,
    fin_tom_false_belief_result_t* out_result
);

/**
 * @brief Detect false beliefs across all models
 *
 * @param bridge Bridge handle
 * @param context Market context (ground truth)
 * @param out_result Output result
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_detect_false_beliefs_all(
    financial_tom_bridge_t* bridge,
    const fin_tom_market_context_t* context,
    fin_tom_false_belief_result_t* out_result
);

/**
 * @brief Free false belief result resources
 *
 * @param result Result to free
 */
void financial_tom_bridge_free_false_belief_result(fin_tom_false_belief_result_t* result);

/* ============================================================================
 * Sentiment Aggregation API
 * ============================================================================ */

/**
 * @brief Aggregate sentiment across investor models
 *
 * @param bridge Bridge handle
 * @param investor_ids Array of investor IDs (NULL = all)
 * @param num_investors Number of investors (0 = all)
 * @param out_sentiment Output aggregated sentiment
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_aggregate_sentiment(
    financial_tom_bridge_t* bridge,
    const char** investor_ids,
    uint32_t num_investors,
    fin_tom_sentiment_t* out_sentiment
);

/**
 * @brief Get sentiment by archetype
 *
 * @param bridge Bridge handle
 * @param archetype Archetype to filter
 * @param out_sentiment Output sentiment
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_sentiment_by_archetype(
    financial_tom_bridge_t* bridge,
    fin_tom_archetype_t archetype,
    fin_tom_sentiment_t* out_sentiment
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set model created/updated callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_set_model_callback(
    financial_tom_bridge_t* bridge,
    fin_tom_model_callback_t callback,
    void* user_data
);

/**
 * @brief Set false belief detected callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_set_false_belief_callback(
    financial_tom_bridge_t* bridge,
    fin_tom_false_belief_callback_t callback,
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
fin_tom_state_t financial_tom_bridge_get_state(
    const financial_tom_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_tom_bridge_get_stats(
    const financial_tom_bridge_t* bridge,
    fin_tom_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_tom_bridge_reset_stats(financial_tom_bridge_t* bridge);

/**
 * @brief Get model count
 *
 * @param bridge Bridge handle
 * @return Number of investor models
 */
uint32_t financial_tom_bridge_get_model_count(
    const financial_tom_bridge_t* bridge
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_tom_bridge_get_last_error(void);

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
int financial_tom_bridge_heartbeat(
    financial_tom_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Training Hooks (B23 Upgrade Compatibility)
 * ============================================================================ */

/**
 * @brief Begin training session
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int financial_tom_bridge_training_begin(financial_tom_bridge_t* bridge);

/**
 * @brief End training session
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int financial_tom_bridge_training_end(financial_tom_bridge_t* bridge);

/**
 * @brief Training step with progress
 * @param bridge Bridge handle
 * @param progress Progress [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int financial_tom_bridge_training_step(financial_tom_bridge_t* bridge, float progress);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get archetype name
 *
 * @param archetype Archetype type
 * @return String name (static)
 */
const char* fin_tom_archetype_name(fin_tom_archetype_t archetype);

/**
 * @brief Get emotion name
 *
 * @param emotion Emotion type
 * @return String name (static)
 */
const char* fin_tom_emotion_name(fin_tom_emotion_t emotion);

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return String name (static)
 */
const char* fin_tom_action_name(fin_tom_action_t action);

/**
 * @brief Get false belief type name
 *
 * @param type False belief type
 * @return String name (static)
 */
const char* fin_tom_false_belief_name(fin_tom_false_belief_type_t type);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_tom_state_name(fin_tom_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_tom_bridge_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_TOM_BRIDGE_H */
