/**
 * @file nimcp_financial_emotion_bridge.h
 * @brief Financial Emotion Bridge - Plutchik-based emotion modeling for trading
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for modeling and tracking emotional states during trading using
 *       Plutchik's 8 primary emotions plus financial-specific compound emotions.
 *       Integrates with decision modulation and bias detection.
 *
 * WHY:  Emotions significantly impact trading decisions and outcomes. Research
 *       shows that fear, greed, and panic are major drivers of market volatility.
 *       This bridge enables:
 *       - Real-time emotional state tracking based on market events
 *       - Decision modulation to counteract emotional biases
 *       - Detection of destructive emotional patterns (FOMO, panic selling)
 *       - Integration with autobiographical memory for experience-based learning
 *
 * HOW:  Market events trigger emotional responses via appraisal theory. Primary
 *       emotions combine to form compound emotions (e.g., fear + anticipation = FOMO).
 *       Emotional states modulate decision parameters (risk tolerance, position size).
 *       Bias detection identifies when emotions exceed healthy thresholds.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    Financial Emotion Bridge                                |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |     Market Events         |       |    Plutchik Model         |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Price changes             |       | 8 Primary Emotions        |        |
 * |  | Volume spikes             |       | Compound Emotions         |        |
 * |  | Volatility shifts         |       | Appraisal Dimensions      |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |              Emotion Update Engine                        |            |
 * |  |  event -> appraisal -> primary_emotions -> compounds      |            |
 * |  +----------------------------------------------------------+            |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +---------------------------+       +---------------------------+        |
 * |  |   Decision Modulation     |       |     Bias Detection        |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Risk tolerance scaling    |       | FOMO detection            |        |
 * |  | Position size adjustment  |       | Panic selling detection   |        |
 * |  | Stop loss modification    |       | Greed-driven overtrading  |        |
 * |  +---------------------------+       +---------------------------+        |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_autobio_bridge.h
 * @see nimcp_financial_neural_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_EMOTION_BRIDGE_H
#define NIMCP_FINANCIAL_EMOTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_EMOTION_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_EMOTION_BRIDGE_MAGIC      0x46454D42  /* 'FEMB' */

/** Bio-async module ID for financial emotion bridge */
#define BIO_MODULE_FINANCIAL_EMOTION        0x0397

/** Maximum history entries for emotion tracking */
#define FIN_EMOTION_MAX_HISTORY             1024

/** Number of Plutchik primary emotions */
#define FIN_EMOTION_PRIMARY_COUNT           8

/** Number of compound emotions */
#define FIN_EMOTION_COMPOUND_COUNT          6

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_EMOTION_ERROR_BASE              33200
#define FIN_EMOTION_ERR_OK                  0
#define FIN_EMOTION_ERR_NULL                (FIN_EMOTION_ERROR_BASE + 1)
#define FIN_EMOTION_ERR_INVALID_PARAM       (FIN_EMOTION_ERROR_BASE + 2)
#define FIN_EMOTION_ERR_NO_MEMORY           (FIN_EMOTION_ERROR_BASE + 3)
#define FIN_EMOTION_ERR_STATE               (FIN_EMOTION_ERROR_BASE + 4)
#define FIN_EMOTION_ERR_IMMUNE              (FIN_EMOTION_ERROR_BASE + 5)
#define FIN_EMOTION_ERR_BBB                 (FIN_EMOTION_ERROR_BASE + 6)
#define FIN_EMOTION_ERR_VALIDATION          (FIN_EMOTION_ERROR_BASE + 7)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Market event types that trigger emotional responses (guarded)
 */
#ifndef FIN_MARKET_EVENT_TYPE_DEFINED
#define FIN_MARKET_EVENT_TYPE_DEFINED
typedef enum {
    FIN_MKT_EVENT_PRICE_INCREASE = 0,   /**< Price increased */
    FIN_MKT_EVENT_PRICE_DECREASE,       /**< Price decreased */
    FIN_MKT_EVENT_VOLUME_SPIKE,         /**< Unusual volume */
    FIN_MKT_EVENT_VOLATILITY_SPIKE,     /**< Volatility increase */
    FIN_MKT_EVENT_STOP_LOSS_HIT,        /**< Stop loss triggered */
    FIN_MKT_EVENT_PROFIT_TARGET_HIT,    /**< Take profit triggered */
    FIN_MKT_EVENT_NEWS_POSITIVE,        /**< Positive news release */
    FIN_MKT_EVENT_NEWS_NEGATIVE,        /**< Negative news release */
    FIN_MKT_EVENT_REGIME_CHANGE,        /**< Market regime shift */
    FIN_MKT_EVENT_MISSED_OPPORTUNITY,   /**< Missed trade opportunity */
    FIN_MKT_EVENT_COUNT
} fin_market_event_type_t;
#endif /* FIN_MARKET_EVENT_TYPE_DEFINED */

/**
 * @brief Plutchik's primary emotion indices
 */
typedef enum {
    FIN_PRIMARY_JOY = 0,                /**< Happiness, elation */
    FIN_PRIMARY_SADNESS,                /**< Disappointment, grief */
    FIN_PRIMARY_ANGER,                  /**< Frustration, rage */
    FIN_PRIMARY_FEAR,                   /**< Anxiety, terror */
    FIN_PRIMARY_SURPRISE,               /**< Astonishment */
    FIN_PRIMARY_DISGUST,                /**< Revulsion, contempt */
    FIN_PRIMARY_TRUST,                  /**< Confidence, admiration */
    FIN_PRIMARY_ANTICIPATION,           /**< Expectation, vigilance */
    FIN_PRIMARY_COUNT
} fin_primary_emotion_t;

/**
 * @brief Financial compound emotions
 */
typedef enum {
    FIN_COMPOUND_GREED = 0,             /**< Joy + Anticipation */
    FIN_COMPOUND_PANIC,                 /**< Fear + Surprise */
    FIN_COMPOUND_FOMO,                  /**< Fear + Anticipation (Fear of Missing Out) */
    FIN_COMPOUND_EUPHORIA,              /**< Joy + Surprise */
    FIN_COMPOUND_ANXIETY,               /**< Fear + Anticipation (worry) */
    FIN_COMPOUND_REGRET,                /**< Sadness + Anger */
    FIN_COMPOUND_COUNT
} fin_compound_emotion_t;

/**
 * @brief Dominant emotion type (for get_dominant)
 */
typedef enum {
    FIN_DOMINANT_NEUTRAL = 0,           /**< No strong emotion */
    FIN_DOMINANT_JOY,
    FIN_DOMINANT_SADNESS,
    FIN_DOMINANT_ANGER,
    FIN_DOMINANT_FEAR,
    FIN_DOMINANT_SURPRISE,
    FIN_DOMINANT_DISGUST,
    FIN_DOMINANT_TRUST,
    FIN_DOMINANT_ANTICIPATION,
    FIN_DOMINANT_GREED,
    FIN_DOMINANT_PANIC,
    FIN_DOMINANT_FOMO,
    FIN_DOMINANT_EUPHORIA,
    FIN_DOMINANT_ANXIETY,
    FIN_DOMINANT_REGRET,
    FIN_DOMINANT_COUNT
} fin_dominant_emotion_t;

/**
 * @brief Emotional bias types for detection
 */
typedef enum {
    FIN_BIAS_NONE = 0,                  /**< No significant bias */
    FIN_BIAS_FOMO,                      /**< Fear of missing out */
    FIN_BIAS_PANIC_SELLING,             /**< Panic-driven selling */
    FIN_BIAS_GREED_OVERTRADING,         /**< Greed-driven excessive trading */
    FIN_BIAS_LOSS_AVERSION,             /**< Excessive fear of losses */
    FIN_BIAS_OVERCONFIDENCE,            /**< Excessive trust/joy */
    FIN_BIAS_REVENGE_TRADING,           /**< Anger-driven trading after loss */
    FIN_BIAS_COUNT
} fin_emotional_bias_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_EMOTION_STATE_UNINITIALIZED = 0,
    FIN_EMOTION_STATE_INITIALIZED,
    FIN_EMOTION_STATE_ACTIVE,
    FIN_EMOTION_STATE_DEGRADED,
    FIN_EMOTION_STATE_ERROR
} fin_emotion_bridge_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Complete emotional state using Plutchik's model
 *
 * All values are in the range [0.0, 1.0] representing intensity.
 */
typedef struct {
    /* Primary emotions [0,1] - Plutchik's 8 */
    float joy;                          /**< Happiness, elation */
    float sadness;                      /**< Disappointment, grief */
    float anger;                        /**< Frustration, rage */
    float fear;                         /**< Anxiety, terror */
    float surprise;                     /**< Astonishment */
    float disgust;                      /**< Revulsion, contempt */
    float trust;                        /**< Confidence, admiration */
    float anticipation;                 /**< Expectation, vigilance */

    /* Compound emotions [0,1] */
    float greed;                        /**< joy + anticipation */
    float panic;                        /**< fear + surprise */
    float fomo;                         /**< fear + anticipation */
    float euphoria;                     /**< joy + surprise */
    float anxiety;                      /**< fear + anticipation (worried) */
    float regret;                       /**< sadness + anger */

    /* Appraisal dimensions [0,1] */
    float certainty;                    /**< How certain about outcomes */
    float control;                      /**< Perceived control over situation */
    float relevance;                    /**< Personal relevance of events */
} fin_emotion_state_t;

/**
 * @brief Market event that triggers emotional response (guarded)
 */
#ifndef FIN_MARKET_EVENT_DEFINED
#define FIN_MARKET_EVENT_DEFINED
typedef struct {
    fin_market_event_type_t event_type; /**< Type of market event */
    float magnitude;                    /**< Event magnitude [-1.0, 1.0] */
    float surprise_factor;              /**< How unexpected [0.0, 1.0] */
    uint64_t timestamp_ms;              /**< Event timestamp */
} fin_market_event_t;
#endif /* FIN_MARKET_EVENT_DEFINED */

/**
 * @brief Result of dominant emotion query
 */
typedef struct {
    fin_dominant_emotion_t dominant;    /**< The dominant emotion */
    float intensity;                    /**< Intensity [0.0, 1.0] */
    bool is_compound;                   /**< True if compound emotion */
} fin_dominant_result_t;

/**
 * @brief Decision modulation parameters
 *
 * These parameters can be used to adjust trading decisions based on
 * emotional state to counteract biases.
 */
typedef struct {
    float risk_tolerance_scale;         /**< Multiply base risk tolerance */
    float position_size_scale;          /**< Multiply base position size */
    float stop_loss_scale;              /**< Multiply stop loss distance */
    float take_profit_scale;            /**< Multiply take profit distance */
    float urgency_dampening;            /**< Slow down decisions [0,1] */
    bool suggest_pause;                 /**< Suggest taking a break */
    char reason[128];                   /**< Human-readable reason */
} fin_decision_modulation_t;

/**
 * @brief Bias detection result
 */
typedef struct {
    fin_emotional_bias_t bias;          /**< Detected bias type */
    float severity;                     /**< Bias severity [0.0, 1.0] */
    float confidence;                   /**< Detection confidence [0.0, 1.0] */
    char description[256];              /**< Human-readable description */
    bool action_recommended;            /**< Should take corrective action */
} fin_bias_detection_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                   /**< Total emotion updates */
    uint64_t modulations;               /**< Decision modulations performed */
    uint64_t bias_detections;           /**< Bias detection calls */
    uint64_t immune_checks;             /**< Immune system checks */
    uint64_t bbb_validations;           /**< BBB validations performed */
    uint64_t kg_messages_sent;          /**< KG messages published */
    uint64_t health_heartbeats;         /**< Health heartbeats sent */
} fin_emotion_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Emotion dynamics */
    float decay_rate;                   /**< Emotion decay per second [0,1] */
    float sensitivity;                  /**< Response sensitivity [0,2] */
    float baseline_mood;                /**< Baseline mood level [0,1] */

    /* Thresholds for bias detection */
    float fomo_threshold;               /**< FOMO detection threshold */
    float panic_threshold;              /**< Panic detection threshold */
    float greed_threshold;              /**< Greed detection threshold */
    float overconfidence_threshold;     /**< Overconfidence threshold */

    /* Decision modulation settings */
    float max_risk_scale;               /**< Max risk tolerance scale */
    float min_risk_scale;               /**< Min risk tolerance scale */
    float urgency_threshold;            /**< When to dampen urgency */
    bool enable_pause_suggestion;       /**< Enable pause suggestions */

    /* Integration settings */
    bool enable_immune_integration;     /**< Enable immune system */
    bool enable_bbb_validation;         /**< Enable BBB validation */
    bool enable_kg_messaging;           /**< Enable KG messaging */
    bool enable_health_monitoring;      /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;               /**< Verbose debug output */
} fin_emotion_config_t;

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
 * @brief Opaque financial emotion bridge handle
 */
typedef struct financial_emotion_bridge financial_emotion_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_emotion_bridge_default_config(fin_emotion_config_t* config);

/**
 * @brief Create financial emotion bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_emotion_bridge_t* financial_emotion_bridge_create(
    const fin_emotion_config_t* config
);

/**
 * @brief Destroy financial emotion bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_emotion_bridge_destroy(financial_emotion_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_reset(financial_emotion_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_emotion_bridge_set_immune(financial_emotion_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_emotion_bridge_set_bbb(financial_emotion_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_emotion_bridge_set_health_agent(financial_emotion_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_emotion_bridge_set_kg_wiring(financial_emotion_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_emotion_bridge_set_logger(financial_emotion_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_emotion_bridge_set_security(financial_emotion_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_emotion_bridge_set_ethics(financial_emotion_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_emotion_bridge_set_lgss(financial_emotion_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_emotion_bridge_set_coordinator(financial_emotion_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_emotion_bridge_set_bio_router(financial_emotion_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Core Emotion API
 * ============================================================================ */

/**
 * @brief Update emotional state based on market event
 *
 * Processes a market event through the appraisal system to update
 * primary and compound emotions.
 *
 * @param bridge Bridge handle
 * @param event Market event that occurred
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_update(
    financial_emotion_bridge_t* bridge,
    const fin_market_event_t* event
);

/**
 * @brief Get current emotional state
 *
 * @param bridge Bridge handle
 * @param state Output emotional state
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_get_state(
    const financial_emotion_bridge_t* bridge,
    fin_emotion_state_t* state
);

/**
 * @brief Get dominant emotion
 *
 * Returns the emotion with highest intensity, considering both
 * primary and compound emotions.
 *
 * @param bridge Bridge handle
 * @param result Output dominant emotion result
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_get_dominant(
    const financial_emotion_bridge_t* bridge,
    fin_dominant_result_t* result
);

/**
 * @brief Apply time-based decay to emotions
 *
 * Call periodically to naturally decay emotional intensity over time.
 * Decay rate is determined by configuration.
 *
 * @param bridge Bridge handle
 * @param elapsed_ms Milliseconds since last decay call
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_decay(
    financial_emotion_bridge_t* bridge,
    uint64_t elapsed_ms
);

/* ============================================================================
 * Decision Modulation API
 * ============================================================================ */

/**
 * @brief Get decision modulation parameters
 *
 * Returns scaling factors and recommendations based on current emotional
 * state to help counteract emotional biases in trading decisions.
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation parameters
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_modulate_decision(
    const financial_emotion_bridge_t* bridge,
    fin_decision_modulation_t* modulation
);

/* ============================================================================
 * Bias Detection API
 * ============================================================================ */

/**
 * @brief Detect emotional biases
 *
 * Analyzes current emotional state to detect potentially destructive
 * emotional patterns like FOMO, panic selling, or greed-driven behavior.
 *
 * @param bridge Bridge handle
 * @param detection Output bias detection result
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_detect_bias(
    const financial_emotion_bridge_t* bridge,
    fin_bias_detection_t* detection
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
fin_emotion_bridge_state_t financial_emotion_bridge_get_bridge_state(
    const financial_emotion_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_get_stats(
    const financial_emotion_bridge_t* bridge,
    fin_emotion_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_emotion_bridge_reset_stats(financial_emotion_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_emotion_bridge_get_last_error(void);

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
int financial_emotion_bridge_heartbeat(
    financial_emotion_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get primary emotion name
 *
 * @param emotion Primary emotion type
 * @return String name (static)
 */
const char* fin_emotion_primary_name(fin_primary_emotion_t emotion);

/**
 * @brief Get compound emotion name
 *
 * @param emotion Compound emotion type
 * @return String name (static)
 */
const char* fin_emotion_compound_name(fin_compound_emotion_t emotion);

/**
 * @brief Get dominant emotion name
 *
 * @param emotion Dominant emotion type
 * @return String name (static)
 */
const char* fin_emotion_dominant_name(fin_dominant_emotion_t emotion);

/**
 * @brief Get bias type name
 *
 * @param bias Bias type
 * @return String name (static)
 */
const char* fin_emotion_bias_name(fin_emotional_bias_t bias);

/**
 * @brief Get event type name
 *
 * @param event Event type
 * @return String name (static)
 */
const char* fin_emotion_event_name(fin_market_event_type_t event);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_emotion_state_name(fin_emotion_bridge_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_emotion_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_training_begin(financial_emotion_bridge_t* bridge);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_training_end(financial_emotion_bridge_t* bridge);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_emotion_bridge_training_step(financial_emotion_bridge_t* bridge, float progress);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_EMOTION_BRIDGE_H */
