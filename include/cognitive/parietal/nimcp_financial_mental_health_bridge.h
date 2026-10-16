/**
 * @file nimcp_financial_mental_health_bridge.h
 * @brief Financial Mental Health Bridge - Trader wellbeing assessment and protection
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for assessing mental health state during trading and determining
 *       when trading should be paused for trader wellbeing. Integrates stress,
 *       anxiety, depression, cognitive load, and decision fatigue monitoring.
 *
 * WHY:  Trading can severely impact mental health. Chronic stress, anxiety,
 *       and burnout lead to poor decisions and destructive behavior patterns.
 *       This bridge provides:
 *       - Real-time mental health state assessment
 *       - Trading advisability determination based on wellbeing
 *       - Break recommendations with appropriate durations
 *       - Integration with emotion bridge for comprehensive monitoring
 *
 * HOW:  Monitors multiple mental health indicators and computes an overall
 *       judgment impairment score. When impairment exceeds thresholds, trading
 *       is blocked and breaks are recommended. Supports configurable thresholds
 *       and recovery tracking.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                 Financial Mental Health Bridge                            |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |   Mental Health Inputs    |       |    Health Thresholds      |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Stress level              |       | Max stress for trading    |        |
 * |  | Anxiety level             |       | Max anxiety for trading   |        |
 * |  | Depression risk           |       | Depression warning level  |        |
 * |  | Cognitive load            |       | Max cognitive load        |        |
 * |  | Decision fatigue          |       | Decision count limits     |        |
 * |  +------------+--------------+       +-------------+-------------+        |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +----------------------------------------------------------+            |
 * |  |           Mental Health Assessment Engine                 |            |
 * |  |  inputs -> weighted_score -> judgment_impairment -> action|            |
 * |  +----------------------------------------------------------+            |
 * |               |                                    |                      |
 * |               v                                    v                      |
 * |  +---------------------------+       +---------------------------+        |
 * |  |   Trading Advisability    |       |    Break Recommendations  |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Should trade? (bool)      |       | Break duration (minutes)  |        |
 * |  | Risk level assessment     |       | Break type (short/medium) |        |
 * |  | Warning messages          |       | Recovery activities       |        |
 * |  +---------------------------+       +---------------------------+        |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @see nimcp_financial_emotion_bridge.h
 * @see nimcp_financial_neural_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_MENTAL_HEALTH_BRIDGE_H
#define NIMCP_FINANCIAL_MENTAL_HEALTH_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_MENTAL_HEALTH_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_MENTAL_HEALTH_BRIDGE_MAGIC      0x464D4842  /* 'FMHB' */

/** Bio-async module ID for financial mental health bridge */
#define BIO_MODULE_FINANCIAL_MENTAL_HEALTH        0x0398

/** Maximum history entries for health tracking */
#define FIN_MENTAL_HEALTH_MAX_HISTORY             512

/** Maximum break recommendation message length */
#define FIN_MENTAL_HEALTH_MAX_MESSAGE             256

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_MENTAL_HEALTH_ERROR_BASE              33300
#define FIN_MENTAL_HEALTH_ERR_OK                  0
#define FIN_MENTAL_HEALTH_ERR_NULL                (FIN_MENTAL_HEALTH_ERROR_BASE + 1)
#define FIN_MENTAL_HEALTH_ERR_INVALID_PARAM       (FIN_MENTAL_HEALTH_ERROR_BASE + 2)
#define FIN_MENTAL_HEALTH_ERR_NO_MEMORY           (FIN_MENTAL_HEALTH_ERROR_BASE + 3)
#define FIN_MENTAL_HEALTH_ERR_STATE               (FIN_MENTAL_HEALTH_ERROR_BASE + 4)
#define FIN_MENTAL_HEALTH_ERR_IMMUNE              (FIN_MENTAL_HEALTH_ERROR_BASE + 5)
#define FIN_MENTAL_HEALTH_ERR_BBB                 (FIN_MENTAL_HEALTH_ERROR_BASE + 6)
#define FIN_MENTAL_HEALTH_ERR_VALIDATION          (FIN_MENTAL_HEALTH_ERROR_BASE + 7)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Mental health risk levels
 */
typedef enum {
    FIN_MH_RISK_LOW = 0,           /**< Healthy state, trading OK */
    FIN_MH_RISK_MODERATE,          /**< Some stress, caution advised */
    FIN_MH_RISK_ELEVATED,          /**< Significant impairment, reduce trading */
    FIN_MH_RISK_HIGH,              /**< Trading not advisable */
    FIN_MH_RISK_CRITICAL,          /**< Stop trading immediately */
    FIN_MH_RISK_COUNT
} fin_mental_health_risk_t;

/**
 * @brief Break types for recommendations
 */
typedef enum {
    FIN_BREAK_NONE = 0,            /**< No break needed */
    FIN_BREAK_MICRO,               /**< 5-minute break */
    FIN_BREAK_SHORT,               /**< 15-minute break */
    FIN_BREAK_MEDIUM,              /**< 30-minute break */
    FIN_BREAK_LONG,                /**< 1-hour break */
    FIN_BREAK_EXTENDED,            /**< 2+ hour break */
    FIN_BREAK_SESSION_END,         /**< End trading session */
    FIN_BREAK_COUNT
} fin_break_type_t;

/**
 * @brief Trading advisability result
 */
typedef enum {
    FIN_TRADE_ADVISED = 0,         /**< Trading is advisable */
    FIN_TRADE_CAUTION,             /**< Trade with caution */
    FIN_TRADE_REDUCED,             /**< Reduce position sizes */
    FIN_TRADE_NOT_ADVISED,         /**< Trading not recommended */
    FIN_TRADE_BLOCKED,             /**< Trading should be blocked */
    FIN_TRADE_ADVICE_COUNT
} fin_trading_advice_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_MH_STATE_UNINITIALIZED = 0,
    FIN_MH_STATE_INITIALIZED,
    FIN_MH_STATE_ACTIVE,
    FIN_MH_STATE_MONITORING,
    FIN_MH_STATE_ALERT,
    FIN_MH_STATE_DEGRADED,
    FIN_MH_STATE_ERROR
} fin_mental_health_bridge_state_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Complete mental health state
 *
 * All values are in the range [0.0, 1.0] representing intensity/severity.
 */
typedef struct {
    float stress_level;            /**< Current stress level [0,1] */
    float anxiety_level;           /**< Current anxiety level [0,1] */
    float depression_risk;         /**< Depression risk indicator [0,1] */
    float cognitive_load;          /**< Current cognitive load [0,1] */
    float decision_fatigue;        /**< Decision fatigue level [0,1] */
    bool judgment_impaired;        /**< Overall judgment impairment flag */
} fin_mental_health_state_t;

/**
 * @brief Mental health assessment result
 */
typedef struct {
    fin_mental_health_state_t state;     /**< Current health state */
    fin_mental_health_risk_t risk_level; /**< Overall risk assessment */
    float impairment_score;              /**< Combined impairment [0,1] */
    float wellbeing_score;               /**< Inverse: wellbeing [0,1] */
    uint64_t timestamp_ms;               /**< Assessment timestamp */
    char summary[FIN_MENTAL_HEALTH_MAX_MESSAGE]; /**< Human-readable summary */
} fin_mental_health_assessment_t;

/**
 * @brief Trading advisability result
 */
typedef struct {
    fin_trading_advice_t advice;         /**< Trading advice */
    bool should_trade;                   /**< Simple yes/no */
    float confidence;                    /**< Confidence in advice [0,1] */
    float max_position_scale;            /**< Recommended position scale [0,1] */
    float max_risk_scale;                /**< Recommended risk scale [0,1] */
    char reason[FIN_MENTAL_HEALTH_MAX_MESSAGE]; /**< Explanation */
} fin_trading_advisability_t;

/**
 * @brief Break recommendation
 */
typedef struct {
    fin_break_type_t break_type;         /**< Type of break recommended */
    uint32_t duration_minutes;           /**< Recommended duration */
    float urgency;                       /**< How urgent [0,1] */
    bool mandatory;                      /**< Is break mandatory? */
    char message[FIN_MENTAL_HEALTH_MAX_MESSAGE]; /**< Guidance message */
    char activities[FIN_MENTAL_HEALTH_MAX_MESSAGE]; /**< Suggested activities */
} fin_break_recommendation_t;

/**
 * @brief Input factors for health update
 */
typedef struct {
    /* Trading activity metrics */
    uint32_t trades_today;               /**< Number of trades today */
    uint32_t decisions_today;            /**< Total decisions made */
    uint32_t losses_today;               /**< Number of losing trades */
    uint32_t wins_today;                 /**< Number of winning trades */
    float pnl_today;                     /**< P&L for today (normalized) */

    /* Session metrics */
    uint32_t session_duration_mins;      /**< Minutes in current session */
    uint32_t time_since_break_mins;      /**< Minutes since last break */
    uint32_t consecutive_losses;         /**< Current loss streak */
    uint32_t consecutive_wins;           /**< Current win streak */

    /* External factors */
    float sleep_quality;                 /**< Last night's sleep [0,1] */
    float physical_wellness;             /**< Physical state [0,1] */
    float external_stress;               /**< Non-trading stress [0,1] */

    /* Biometric data (if available) */
    bool biometrics_available;           /**< Whether biometrics are provided */
    float heart_rate_variability;        /**< HRV (normalized) */
    float cortisol_estimate;             /**< Stress hormone estimate [0,1] */
} fin_health_input_factors_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t assessments;                /**< Total assessments performed */
    uint64_t trading_blocked;            /**< Times trading was blocked */
    uint64_t breaks_recommended;         /**< Break recommendations made */
    uint64_t immune_checks;              /**< Immune system checks */
    uint64_t bbb_validations;            /**< BBB validations performed */
    uint64_t kg_messages_sent;           /**< KG messages published */
    uint64_t health_heartbeats;          /**< Health heartbeats sent */
} fin_mental_health_bridge_stats_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Stress thresholds */
    float stress_warning_threshold;      /**< Stress warning level [0,1] */
    float stress_block_threshold;        /**< Stress blocking level [0,1] */

    /* Anxiety thresholds */
    float anxiety_warning_threshold;     /**< Anxiety warning level [0,1] */
    float anxiety_block_threshold;       /**< Anxiety blocking level [0,1] */

    /* Depression thresholds */
    float depression_warning_threshold;  /**< Depression warning level [0,1] */

    /* Cognitive load thresholds */
    float cognitive_load_warning;        /**< Cognitive load warning [0,1] */
    float cognitive_load_block;          /**< Cognitive load blocking [0,1] */

    /* Decision fatigue thresholds */
    float decision_fatigue_warning;      /**< Fatigue warning level [0,1] */
    float decision_fatigue_block;        /**< Fatigue blocking level [0,1] */

    /* Combined impairment thresholds */
    float impairment_warning;            /**< Combined impairment warning */
    float impairment_block;              /**< Combined impairment blocking */

    /* Trading limits */
    uint32_t max_trades_per_session;     /**< Max trades before forced break */
    uint32_t max_decisions_per_session;  /**< Max decisions before break */
    uint32_t max_session_duration_mins;  /**< Max session length (minutes) */
    uint32_t max_time_without_break;     /**< Max minutes without break */
    uint32_t max_consecutive_losses;     /**< Max losses before mandatory break */

    /* Weight factors for impairment calculation */
    float stress_weight;                 /**< Stress contribution [0,1] */
    float anxiety_weight;                /**< Anxiety contribution [0,1] */
    float depression_weight;             /**< Depression contribution [0,1] */
    float cognitive_weight;              /**< Cognitive load contribution [0,1] */
    float fatigue_weight;                /**< Fatigue contribution [0,1] */

    /* Recovery settings */
    float recovery_rate;                 /**< Recovery rate per minute [0,1] */
    bool enable_mandatory_breaks;        /**< Enforce mandatory breaks */
    bool enable_biometric_integration;   /**< Use biometric data if available */

    /* Integration settings */
    bool enable_immune_integration;      /**< Enable immune system */
    bool enable_bbb_validation;          /**< Enable BBB validation */
    bool enable_kg_messaging;            /**< Enable KG messaging */
    bool enable_health_monitoring;       /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;                /**< Verbose debug output */
} fin_mental_health_config_t;

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
 * @brief Opaque financial mental health bridge handle
 */
typedef struct financial_mental_health_bridge financial_mental_health_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int financial_mental_health_bridge_default_config(fin_mental_health_config_t* config);

/**
 * @brief Create financial mental health bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_mental_health_bridge_t* financial_mental_health_bridge_create(
    const fin_mental_health_config_t* config
);

/**
 * @brief Destroy financial mental health bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_mental_health_bridge_destroy(financial_mental_health_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_reset(financial_mental_health_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_mental_health_bridge_set_immune(
    financial_mental_health_bridge_t* bridge, void* immune);

/**
 * @brief Set BBB system handle
 */
int financial_mental_health_bridge_set_bbb(
    financial_mental_health_bridge_t* bridge, bbb_system_t bbb);

/**
 * @brief Set health agent handle
 */
int financial_mental_health_bridge_set_health_agent(
    financial_mental_health_bridge_t* bridge, void* health_agent);

/**
 * @brief Set KG wiring handle
 */
int financial_mental_health_bridge_set_kg_wiring(
    financial_mental_health_bridge_t* bridge, void* kg_wiring);

/**
 * @brief Set logger handle
 */
int financial_mental_health_bridge_set_logger(
    financial_mental_health_bridge_t* bridge, void* logger);

/**
 * @brief Set security handle
 */
int financial_mental_health_bridge_set_security(
    financial_mental_health_bridge_t* bridge, void* security);

/**
 * @brief Set ethics engine handle
 */
int financial_mental_health_bridge_set_ethics(
    financial_mental_health_bridge_t* bridge, ethics_engine_t ethics);

/**
 * @brief Set LGSS handle
 */
int financial_mental_health_bridge_set_lgss(
    financial_mental_health_bridge_t* bridge, const void* lgss);

/**
 * @brief Set cycle coordinator handle
 */
int financial_mental_health_bridge_set_coordinator(
    financial_mental_health_bridge_t* bridge, brain_cycle_coordinator_t* coordinator);

/**
 * @brief Set bio router handle
 */
int financial_mental_health_bridge_set_bio_router(
    financial_mental_health_bridge_t* bridge, void* bio_router);

/* ============================================================================
 * Core Mental Health API
 * ============================================================================ */

/**
 * @brief Assess current mental health state
 *
 * Performs a comprehensive assessment of the trader's mental health state
 * based on current indicators and input factors.
 *
 * @param bridge Bridge handle
 * @param factors Input factors for assessment (NULL uses internal state)
 * @param assessment Output assessment result
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_assess(
    financial_mental_health_bridge_t* bridge,
    const fin_health_input_factors_t* factors,
    fin_mental_health_assessment_t* assessment
);

/**
 * @brief Determine if trading is advisable
 *
 * Based on current mental health state, determines whether the trader
 * should continue trading, trade with reduced size, or stop.
 *
 * @param bridge Bridge handle
 * @param advisability Output trading advisability
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_should_trade(
    financial_mental_health_bridge_t* bridge,
    fin_trading_advisability_t* advisability
);

/**
 * @brief Get break recommendation
 *
 * Recommends appropriate break duration and activities based on
 * current mental health state and session metrics.
 *
 * @param bridge Bridge handle
 * @param recommendation Output break recommendation
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_recommend_break(
    financial_mental_health_bridge_t* bridge,
    fin_break_recommendation_t* recommendation
);

/* ============================================================================
 * State Update API
 * ============================================================================ */

/**
 * @brief Update mental health state directly
 *
 * Allows direct update of mental health indicators from external sources.
 *
 * @param bridge Bridge handle
 * @param state New mental health state
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_update_state(
    financial_mental_health_bridge_t* bridge,
    const fin_mental_health_state_t* state
);

/**
 * @brief Update based on input factors
 *
 * Updates internal mental health state based on trading activity
 * and other input factors.
 *
 * @param bridge Bridge handle
 * @param factors Input factors
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_update_from_factors(
    financial_mental_health_bridge_t* bridge,
    const fin_health_input_factors_t* factors
);

/**
 * @brief Record that a break was taken
 *
 * Updates internal state to reflect recovery from a break.
 *
 * @param bridge Bridge handle
 * @param duration_minutes Duration of break taken
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_record_break(
    financial_mental_health_bridge_t* bridge,
    uint32_t duration_minutes
);

/**
 * @brief Apply time-based recovery
 *
 * Call periodically to apply gradual recovery to mental health indicators.
 *
 * @param bridge Bridge handle
 * @param elapsed_ms Milliseconds since last recovery call
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_apply_recovery(
    financial_mental_health_bridge_t* bridge,
    uint64_t elapsed_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current mental health state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_get_state(
    const financial_mental_health_bridge_t* bridge,
    fin_mental_health_state_t* state
);

/**
 * @brief Get current risk level
 *
 * @param bridge Bridge handle
 * @return Current risk level
 */
fin_mental_health_risk_t financial_mental_health_bridge_get_risk_level(
    const financial_mental_health_bridge_t* bridge
);

/**
 * @brief Get bridge operational state
 *
 * @param bridge Bridge handle
 * @return Current operational state
 */
fin_mental_health_bridge_state_t financial_mental_health_bridge_get_bridge_state(
    const financial_mental_health_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_get_stats(
    const financial_mental_health_bridge_t* bridge,
    fin_mental_health_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_mental_health_bridge_reset_stats(financial_mental_health_bridge_t* bridge);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_mental_health_bridge_get_last_error(void);

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
int financial_mental_health_bridge_heartbeat(
    financial_mental_health_bridge_t* bridge,
    const char* operation,
    float progress
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get risk level name
 *
 * @param risk Risk level
 * @return String name (static)
 */
const char* fin_mental_health_risk_name(fin_mental_health_risk_t risk);

/**
 * @brief Get break type name
 *
 * @param break_type Break type
 * @return String name (static)
 */
const char* fin_mental_health_break_name(fin_break_type_t break_type);

/**
 * @brief Get trading advice name
 *
 * @param advice Trading advice
 * @return String name (static)
 */
const char* fin_mental_health_advice_name(fin_trading_advice_t advice);

/**
 * @brief Get bridge state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_mental_health_state_name(fin_mental_health_bridge_state_t state);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_mental_health_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_training_begin(financial_mental_health_bridge_t* bridge);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_training_end(financial_mental_health_bridge_t* bridge);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_mental_health_bridge_training_step(
    financial_mental_health_bridge_t* bridge, float progress);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_MENTAL_HEALTH_BRIDGE_H */
