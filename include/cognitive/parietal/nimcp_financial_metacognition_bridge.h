/**
 * @file nimcp_financial_metacognition_bridge.h
 * @brief Financial Metacognition Bridge - Cognitive bias detection and confidence calibration
 * @version 1.0.0
 * @date 2026-01-29
 *
 * WHAT: Bridge for detecting cognitive biases in financial decision-making,
 *       calibrating confidence levels, and determining when decisions need
 *       reconsideration. Part of the Phase 9 metacognitive systems integration.
 *
 * WHY:  Financial decisions are highly susceptible to cognitive biases that
 *       systematically distort judgment. This bridge enables:
 *       - Real-time detection of 10 common cognitive biases
 *       - Confidence calibration based on decision history
 *       - Automatic triggering of decision reconsideration
 *       - Mitigation strategies for detected biases
 *
 * HOW:  Analyzes decision patterns across a sliding window to detect biases.
 *       Uses Bayesian confidence calibration comparing predictions to outcomes.
 *       Triggers reconsideration when bias strength exceeds thresholds or
 *       when confidence is poorly calibrated.
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                 Financial Metacognition Bridge                            |
 * +===========================================================================+
 * |                                                                           |
 * |  +---------------------------+       +---------------------------+        |
 * |  |    Decision History       |       |    Bias Detection Engine  |        |
 * |  +---------------------------+       +---------------------------+        |
 * |  | Past N decisions          |       | Confirmation bias         |        |
 * |  | Predicted outcomes        |       | Anchoring bias            |        |
 * |  | Actual outcomes           |       | Recency bias              |        |
 * |  | Context & reasoning       |       | Overconfidence            |        |
 * |  +------------+--------------+       | Loss aversion             |        |
 * |               |                      | Herding behavior          |        |
 * |               v                      | Gambler's fallacy         |        |
 * |  +---------------------------+       | Sunk cost fallacy         |        |
 * |  |  Confidence Calibration   |       | Availability heuristic    |        |
 * |  +---------------------------+       | Hindsight bias            |        |
 * |  | Predicted vs actual       |       +-------------+-------------+        |
 * |  | Over/under confidence     |                     |                      |
 * |  | Calibration score         |                     v                      |
 * |  +------------+--------------+       +---------------------------+        |
 * |               |                      |   Reconsideration Engine  |        |
 * |               v                      +---------------------------+        |
 * |  +----------------------------------------------------------+            |
 * |  |              Metacognitive Assessment                     |            |
 * |  |  history + biases + calibration -> reconsider_decision?  |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * COGNITIVE BIASES DETECTED:
 * - CONFIRMATION: Seeking info that confirms existing beliefs
 * - ANCHORING: Over-relying on first piece of information
 * - RECENCY: Overweighting recent events vs historical data
 * - OVERCONFIDENCE: Excessive certainty in predictions
 * - LOSS_AVERSION: Asymmetric weighting of gains vs losses
 * - HERDING: Following crowd behavior irrationally
 * - GAMBLER_FALLACY: Believing independent events are correlated
 * - SUNK_COST: Continuing due to past investment
 * - AVAILABILITY: Overweighting easily recalled examples
 * - HINDSIGHT: Believing outcomes were predictable after the fact
 *
 * @see nimcp_financial_emotion_bridge.h
 * @see nimcp_financial_salience_bridge.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FINANCIAL_METACOGNITION_BRIDGE_H
#define NIMCP_FINANCIAL_METACOGNITION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FINANCIAL_METACOGNITION_BRIDGE_VERSION    "1.0.0"
#define FINANCIAL_METACOGNITION_BRIDGE_MAGIC      0x464D4354  /* 'FMCT' */

/** Bio-async module ID for financial metacognition bridge */
#define BIO_MODULE_FINANCIAL_METACOGNITION        0x039A

/** Maximum history entries for decision tracking */
#define FIN_METACOG_MAX_HISTORY                   256

/** Maximum decisions for bias analysis window */
#define FIN_METACOG_WINDOW_SIZE                   64

/** Maximum evidence/mitigation message length */
#define FIN_METACOG_DESC_LEN                      256

/** Maximum biases to detect in a single analysis */
#define FIN_METACOG_MAX_BIASES                    10

/* ============================================================================
 * Error Codes
 * ============================================================================ */

#define FIN_METACOG_ERROR_BASE                    33400
#define FIN_METACOG_ERR_OK                        0
#define FIN_METACOG_ERR_NULL                      (FIN_METACOG_ERROR_BASE + 1)
#define FIN_METACOG_ERR_INVALID_PARAM             (FIN_METACOG_ERROR_BASE + 2)
#define FIN_METACOG_ERR_NO_MEMORY                 (FIN_METACOG_ERROR_BASE + 3)
#define FIN_METACOG_ERR_STATE                     (FIN_METACOG_ERROR_BASE + 4)
#define FIN_METACOG_ERR_IMMUNE                    (FIN_METACOG_ERROR_BASE + 5)
#define FIN_METACOG_ERR_BBB                       (FIN_METACOG_ERROR_BASE + 6)
#define FIN_METACOG_ERR_VALIDATION                (FIN_METACOG_ERROR_BASE + 7)
#define FIN_METACOG_ERR_INSUFFICIENT_DATA         (FIN_METACOG_ERROR_BASE + 8)
#define FIN_METACOG_ERR_CAPACITY                  (FIN_METACOG_ERROR_BASE + 9)

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Cognitive bias types as specified by user
 */
typedef enum {
    FIN_BIAS_CONFIRMATION = 0,    /**< Seeking confirming info */
    FIN_BIAS_ANCHORING,           /**< Over-relying on first info */
    FIN_BIAS_RECENCY,             /**< Overweighting recent events */
    FIN_BIAS_OVERCONFIDENCE,      /**< Excessive certainty */
    FIN_BIAS_LOSS_AVERSION,       /**< Asymmetric loss weighting */
    FIN_BIAS_HERDING,             /**< Following the crowd */
    FIN_BIAS_GAMBLER_FALLACY,     /**< Independent events correlation */
    FIN_BIAS_SUNK_COST,           /**< Past investment continuation */
    FIN_BIAS_AVAILABILITY,        /**< Overweighting recent examples */
    FIN_BIAS_HINDSIGHT,           /**< Post-hoc predictability belief */
    FIN_BIAS_COUNT
} fin_cognitive_bias_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FIN_METACOG_STATE_UNINITIALIZED = 0,
    FIN_METACOG_STATE_INITIALIZED,
    FIN_METACOG_STATE_ACTIVE,
    FIN_METACOG_STATE_ANALYZING,
    FIN_METACOG_STATE_DEGRADED,
    FIN_METACOG_STATE_ERROR
} fin_metacognition_bridge_state_t;

/**
 * @brief Confidence calibration level
 */
typedef enum {
    FIN_CONFIDENCE_WELL_CALIBRATED = 0,   /**< Predictions match outcomes */
    FIN_CONFIDENCE_OVERCONFIDENT,          /**< Predicts better than reality */
    FIN_CONFIDENCE_UNDERCONFIDENT,         /**< Underestimates abilities */
    FIN_CONFIDENCE_INCONSISTENT,           /**< Erratic calibration */
    FIN_CONFIDENCE_UNKNOWN                 /**< Insufficient data */
} fin_confidence_level_t;

/**
 * @brief Reconsideration urgency levels
 */
typedef enum {
    FIN_RECONSIDER_NONE = 0,       /**< No reconsideration needed */
    FIN_RECONSIDER_OPTIONAL,       /**< May benefit from reconsideration */
    FIN_RECONSIDER_RECOMMENDED,    /**< Should reconsider */
    FIN_RECONSIDER_REQUIRED,       /**< Must reconsider before acting */
    FIN_RECONSIDER_BLOCK           /**< Block action until reconsidered */
} fin_reconsider_urgency_t;

/* ============================================================================
 * Data Structures (as specified by user)
 * ============================================================================ */

/**
 * @brief Bias detection result
 *
 * Contains details about a detected cognitive bias including strength,
 * evidence, and suggested mitigation strategy.
 */
typedef struct {
    fin_cognitive_bias_t bias;    /**< Bias type */
    float strength;               /**< Bias strength [0,1] */
    char evidence[256];           /**< What triggered detection */
    char mitigation[256];         /**< How to counter */
} fin_bias_detection_t;

/**
 * @brief Bridge statistics (as specified by user)
 */
typedef struct {
    uint64_t bias_checks;              /**< Total bias check operations */
    uint64_t biases_detected;          /**< Total biases detected */
    uint64_t confidence_calibrations;  /**< Confidence calibration calls */
    uint64_t reconsiderations;         /**< Reconsideration recommendations */
    uint64_t immune_checks;            /**< Immune system checks performed */
    uint64_t bbb_validations;          /**< BBB validations performed */
    uint64_t kg_messages_sent;         /**< KG messages published */
    uint64_t health_heartbeats;        /**< Health heartbeats sent */
} fin_metacognition_bridge_stats_t;

/* ============================================================================
 * Extended Data Structures
 * ============================================================================ */

/**
 * @brief Decision record for history tracking
 */
typedef struct {
    uint64_t timestamp_ms;             /**< Decision timestamp */
    float predicted_outcome;           /**< Predicted outcome [-1, 1] */
    float confidence;                  /**< Confidence in prediction [0, 1] */
    float actual_outcome;              /**< Actual outcome (if resolved) [-1, 1] */
    bool resolved;                     /**< Whether outcome is known */
    char symbol[16];                   /**< Instrument symbol */
    char context[128];                 /**< Decision context/reasoning */
    uint32_t decision_flags;           /**< Additional decision metadata */
} fin_decision_record_t;

/**
 * @brief Confidence calibration result
 */
typedef struct {
    fin_confidence_level_t level;      /**< Calibration level */
    float calibration_score;           /**< Overall calibration [0, 1] */
    float overconfidence_ratio;        /**< Over/under confidence ratio */
    float brier_score;                 /**< Brier score (lower is better) */
    uint32_t decisions_analyzed;       /**< Number of decisions in analysis */
    char recommendation[FIN_METACOG_DESC_LEN]; /**< Calibration advice */
} fin_confidence_result_t;

/**
 * @brief Reconsideration recommendation
 */
typedef struct {
    fin_reconsider_urgency_t urgency;  /**< Urgency level */
    bool should_reconsider;            /**< Simple yes/no */
    float confidence_in_original;      /**< Confidence in original decision */
    uint32_t detected_bias_count;      /**< Number of biases detected */
    fin_bias_detection_t biases[FIN_METACOG_MAX_BIASES]; /**< Detected biases */
    char reason[FIN_METACOG_DESC_LEN]; /**< Primary reconsideration reason */
    char suggested_action[FIN_METACOG_DESC_LEN]; /**< What to do next */
} fin_reconsider_result_t;

/**
 * @brief Comprehensive metacognitive assessment
 */
typedef struct {
    fin_confidence_result_t confidence; /**< Confidence calibration */
    uint32_t bias_count;               /**< Number of biases detected */
    fin_bias_detection_t biases[FIN_METACOG_MAX_BIASES]; /**< All detected biases */
    float metacognitive_accuracy;      /**< Overall metacognitive accuracy [0, 1] */
    float self_awareness_score;        /**< Self-awareness quality [0, 1] */
    char summary[FIN_METACOG_DESC_LEN]; /**< Assessment summary */
} fin_metacognitive_assessment_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bias detection thresholds
 */
typedef struct {
    float confirmation_threshold;      /**< Threshold for confirmation bias */
    float anchoring_threshold;         /**< Threshold for anchoring bias */
    float recency_threshold;           /**< Threshold for recency bias */
    float overconfidence_threshold;    /**< Threshold for overconfidence */
    float loss_aversion_threshold;     /**< Threshold for loss aversion */
    float herding_threshold;           /**< Threshold for herding */
    float gambler_fallacy_threshold;   /**< Threshold for gambler's fallacy */
    float sunk_cost_threshold;         /**< Threshold for sunk cost */
    float availability_threshold;      /**< Threshold for availability bias */
    float hindsight_threshold;         /**< Threshold for hindsight bias */
} fin_bias_thresholds_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Analysis parameters */
    uint32_t min_decisions_for_analysis; /**< Minimum decisions before analysis */
    uint32_t analysis_window_size;     /**< Sliding window size */
    float reconsider_bias_threshold;   /**< Bias strength to trigger reconsider */
    float reconsider_calibration_threshold; /**< Calibration score threshold */

    /* Bias detection thresholds */
    fin_bias_thresholds_t bias_thresholds;

    /* Confidence calibration parameters */
    float confidence_bin_width;        /**< Bin width for calibration [0.1] */
    float acceptable_calibration_error; /**< Max acceptable calibration error */

    /* Reconsideration parameters */
    uint32_t max_reconsider_triggers;  /**< Max reconsider triggers per session */
    float reconsider_cooldown_sec;     /**< Cooldown between reconsiderations */

    /* Integration settings */
    bool enable_immune_integration;    /**< Enable immune system checks */
    bool enable_bbb_validation;        /**< Enable BBB validation */
    bool enable_kg_messaging;          /**< Enable KG messaging */
    bool enable_health_monitoring;     /**< Enable health heartbeats */

    /* Logging */
    bool verbose_logging;              /**< Verbose debug output */
} fin_metacognition_config_t;

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
 * @brief Opaque financial metacognition bridge handle
 */
typedef struct financial_metacognition_bridge financial_metacognition_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_default_config(fin_metacognition_config_t* config);

/**
 * @brief Create financial metacognition bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
financial_metacognition_bridge_t* financial_metacognition_bridge_create(
    const fin_metacognition_config_t* config
);

/**
 * @brief Destroy financial metacognition bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void financial_metacognition_bridge_destroy(financial_metacognition_bridge_t* bridge);

/**
 * @brief Reset bridge state and history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_reset(financial_metacognition_bridge_t* bridge);

/* ============================================================================
 * Subsystem Setters
 * ============================================================================ */

/**
 * @brief Set immune system handle
 */
int financial_metacognition_bridge_set_immune(
    financial_metacognition_bridge_t* bridge,
    void* immune
);

/**
 * @brief Set BBB system handle
 */
int financial_metacognition_bridge_set_bbb(
    financial_metacognition_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Set health agent handle
 */
int financial_metacognition_bridge_set_health_agent(
    financial_metacognition_bridge_t* bridge,
    void* health_agent
);

/**
 * @brief Set KG wiring handle
 */
int financial_metacognition_bridge_set_kg_wiring(
    financial_metacognition_bridge_t* bridge,
    void* kg_wiring
);

/**
 * @brief Set logger handle
 */
int financial_metacognition_bridge_set_logger(
    financial_metacognition_bridge_t* bridge,
    void* logger
);

/**
 * @brief Set security handle
 */
int financial_metacognition_bridge_set_security(
    financial_metacognition_bridge_t* bridge,
    void* security
);

/**
 * @brief Set ethics engine handle
 */
int financial_metacognition_bridge_set_ethics(
    financial_metacognition_bridge_t* bridge,
    ethics_engine_t ethics
);

/**
 * @brief Set LGSS handle
 */
int financial_metacognition_bridge_set_lgss(
    financial_metacognition_bridge_t* bridge,
    const void* lgss
);

/**
 * @brief Set cycle coordinator handle
 */
int financial_metacognition_bridge_set_coordinator(
    financial_metacognition_bridge_t* bridge,
    brain_cycle_coordinator_t* coordinator
);

/**
 * @brief Set bio router handle
 */
int financial_metacognition_bridge_set_bio_router(
    financial_metacognition_bridge_t* bridge,
    void* bio_router
);

/* ============================================================================
 * Decision History API
 * ============================================================================ */

/**
 * @brief Record a new decision
 *
 * Adds a decision to the history for bias analysis. The outcome can be
 * updated later via record_outcome().
 *
 * @param bridge Bridge handle
 * @param record Decision record
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_record_decision(
    financial_metacognition_bridge_t* bridge,
    const fin_decision_record_t* record
);

/**
 * @brief Record outcome for a previous decision
 *
 * Updates a decision record with the actual outcome.
 *
 * @param bridge Bridge handle
 * @param timestamp_ms Original decision timestamp
 * @param actual_outcome Actual outcome [-1, 1]
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_record_outcome(
    financial_metacognition_bridge_t* bridge,
    uint64_t timestamp_ms,
    float actual_outcome
);

/**
 * @brief Get decision count in history
 *
 * @param bridge Bridge handle
 * @return Number of decisions in history, 0 if bridge is NULL
 */
uint32_t financial_metacognition_bridge_get_decision_count(
    const financial_metacognition_bridge_t* bridge
);

/**
 * @brief Clear decision history
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_clear_history(
    financial_metacognition_bridge_t* bridge
);

/* ============================================================================
 * Core API - Bias Detection
 * ============================================================================ */

/**
 * @brief Detect cognitive biases in decision history
 *
 * Analyzes the recent decision history to detect patterns indicative of
 * cognitive biases. Returns all detected biases with strength and evidence.
 *
 * @param bridge Bridge handle
 * @param biases Output array for detected biases (must have FIN_METACOG_MAX_BIASES capacity)
 * @param bias_count Output: number of biases detected
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_detect_biases(
    financial_metacognition_bridge_t* bridge,
    fin_bias_detection_t* biases,
    uint32_t* bias_count
);

/**
 * @brief Check for a specific bias
 *
 * @param bridge Bridge handle
 * @param bias_type Bias type to check
 * @param detection Output detection result
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_check_bias(
    financial_metacognition_bridge_t* bridge,
    fin_cognitive_bias_t bias_type,
    fin_bias_detection_t* detection
);

/* ============================================================================
 * Core API - Confidence Calibration
 * ============================================================================ */

/**
 * @brief Assess confidence calibration
 *
 * Compares predicted outcomes and confidence levels with actual outcomes
 * to determine if the trader is well-calibrated, overconfident, or
 * underconfident.
 *
 * @param bridge Bridge handle
 * @param result Output calibration result
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_assess_confidence(
    financial_metacognition_bridge_t* bridge,
    fin_confidence_result_t* result
);

/* ============================================================================
 * Core API - Reconsideration
 * ============================================================================ */

/**
 * @brief Determine if decision needs reconsideration
 *
 * Evaluates whether a pending or recent decision should be reconsidered
 * based on detected biases, calibration issues, or other metacognitive
 * factors.
 *
 * @param bridge Bridge handle
 * @param decision Current decision being evaluated
 * @param result Output reconsideration recommendation
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_should_reconsider(
    financial_metacognition_bridge_t* bridge,
    const fin_decision_record_t* decision,
    fin_reconsider_result_t* result
);

/* ============================================================================
 * Comprehensive Assessment API
 * ============================================================================ */

/**
 * @brief Perform full metacognitive assessment
 *
 * Combines bias detection, confidence calibration, and other factors
 * into a comprehensive metacognitive assessment.
 *
 * @param bridge Bridge handle
 * @param assessment Output comprehensive assessment
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_assess(
    financial_metacognition_bridge_t* bridge,
    fin_metacognitive_assessment_t* assessment
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
fin_metacognition_bridge_state_t financial_metacognition_bridge_get_state(
    const financial_metacognition_bridge_t* bridge
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_get_stats(
    const financial_metacognition_bridge_t* bridge,
    fin_metacognition_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void financial_metacognition_bridge_reset_stats(
    financial_metacognition_bridge_t* bridge
);

/**
 * @brief Get last error message
 *
 * @return Error message string (thread-local)
 */
const char* financial_metacognition_bridge_get_last_error(void);

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
int financial_metacognition_bridge_heartbeat(
    financial_metacognition_bridge_t* bridge,
    const char* operation,
    float progress
);

/**
 * @brief Set global health agent for module-level heartbeats
 *
 * @param agent Health agent (can be NULL to disable)
 */
void financial_metacognition_bridge_set_health_agent_global(void* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get bias type name
 *
 * @param bias Bias type
 * @return String name (static)
 */
const char* fin_metacog_bias_name(fin_cognitive_bias_t bias);

/**
 * @brief Get state name
 *
 * @param state Bridge state
 * @return String name (static)
 */
const char* fin_metacog_state_name(fin_metacognition_bridge_state_t state);

/**
 * @brief Get confidence level name
 *
 * @param level Confidence level
 * @return String name (static)
 */
const char* fin_metacog_confidence_name(fin_confidence_level_t level);

/**
 * @brief Get reconsideration urgency name
 *
 * @param urgency Urgency level
 * @return String name (static)
 */
const char* fin_metacog_urgency_name(fin_reconsider_urgency_t urgency);

/**
 * @brief Get bridge version
 *
 * @return Version string
 */
const char* financial_metacognition_bridge_version(void);

/* ============================================================================
 * Training Integration
 * ============================================================================ */

/**
 * @brief Begin training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_training_begin(
    financial_metacognition_bridge_t* bridge
);

/**
 * @brief End training session
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_training_end(
    financial_metacognition_bridge_t* bridge
);

/**
 * @brief Training step
 *
 * @param bridge Bridge handle
 * @param progress Training progress [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int financial_metacognition_bridge_training_step(
    financial_metacognition_bridge_t* bridge,
    float progress
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FINANCIAL_METACOGNITION_BRIDGE_H */
