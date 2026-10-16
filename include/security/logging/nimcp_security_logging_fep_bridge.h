/**
 * @file nimcp_security_logging_fep_bridge.h
 * @brief Free Energy Principle Bridge for Security Logging Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for security logging - maps log integrity violations to free energy
 * WHY:  Log tampering, injection attacks, and audit trail corruption represent
 *       high-surprise deviations from expected logging states in FEP framework
 * HOW:  Map anomaly scores to free energy, log manipulation to prediction errors,
 *       and integrity violations to surprise levels
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LOG INTEGRITY AS FREE ENERGY MINIMIZATION:
 * - Healthy audit trails = low free energy (logs match expected patterns)
 * - Tampered logs = high free energy (entries deviate from predictions)
 * - Injection attacks = prediction error (malicious content violates expectations)
 * - Timestamp manipulation = surprise (unexpected temporal anomalies)
 *
 * FEP INTEGRATION:
 * ```
 * Log Entry (e) --> Integrity Verification
 *         |
 *         v
 * Expected Pattern mu (learned logging model)
 *         |
 *         v
 * Prediction Error: epsilon = e - g(mu)
 *         |
 *         v
 * Free Energy F = Complexity + Inaccuracy
 *         |
 *         v
 * Surprise = -ln p(e) <= F
 *         |
 *         v
 * Anomaly Score = F / F_threshold
 * ```
 *
 * DETECTION TYPES:
 * - Log injection (SQL-style patterns in log entries)
 * - Log deletion/truncation (missing entries in sequence)
 * - Timestamp manipulation (non-monotonic or implausible times)
 * - Audit trail corruption (hash chain violations)
 * - Pattern anomalies (unusual log patterns or frequencies)
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0)  --> Normal logging activity
 * - Medium FE (2.0-5.0) --> Suspicious (elevated monitoring)
 * - High FE (5.0-10.0) --> Probable tampering (alert and investigate)
 * - Very high FE (>10.0) --> Active attack (lockdown and preserve evidence)
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |          SECURITY LOGGING - FEP BRIDGE (Log Integrity)                  |
 * +=========================================================================+
 * |                                                                         |
 * |   +------------------+         +------------------+                     |
 * |   |  FEP System      |<------->|  Security        |                     |
 * |   |                  |         |  Logging Bridge  |                     |
 * |   | - Free Energy    |         |                  |                     |
 * |   | - Surprise       |         | - Log Analysis   |                     |
 * |   | - Precision      |         | - Pattern Detect |                     |
 * |   | - Active Inf.    |         | - Audit Trail    |                     |
 * |   +------------------+         +------------------+                     |
 * |           |                              |                              |
 * |           v                              v                              |
 * |   +-------------------------------------------------------------+      |
 * |   |              BIDIRECTIONAL EFFECTS                          |      |
 * |   |                                                             |      |
 * |   |  FEP --> Security:                                          |      |
 * |   |    - Free energy --> Anomaly score                          |      |
 * |   |    - Surprise --> Detection threshold                       |      |
 * |   |    - Precision --> Detection sensitivity                    |      |
 * |   |    - Active inference --> Protective action                 |      |
 * |   |                                                             |      |
 * |   |  Security --> FEP:                                          |      |
 * |   |    - Injection attacks --> High-surprise observations       |      |
 * |   |    - Log tampering --> Prediction error spikes              |      |
 * |   |    - Timestamp anomalies --> Temporal precision updates     |      |
 * |   |    - Valid logs --> Update generative model                 |      |
 * |   +-------------------------------------------------------------+      |
 * |                                                                         |
 * +=========================================================================+
 * ```
 *
 * PROTECTIVE ACTIONS VIA ACTIVE INFERENCE:
 * When high free energy is detected, the system uses active inference to select
 * protective actions that minimize expected free energy:
 * - Increase logging frequency
 * - Enable detailed audit trails
 * - Preserve log snapshots
 * - Alert security personnel
 * - Block suspicious sources
 *
 * @see nimcp_security_logging_bridge.h
 * @see nimcp_free_energy.h
 * @see nimcp_bio_async.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_LOGGING_FEP_BRIDGE_H
#define NIMCP_SECURITY_LOGGING_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/logging/nimcp_security_logging_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Free energy thresholds for log integrity levels */
#define SEC_LOG_FEP_NORMAL_THRESHOLD       2.0f   /**< Normal log activity (low FE) */
#define SEC_LOG_FEP_SUSPICIOUS_THRESHOLD   5.0f   /**< Suspicious (monitoring) */
#define SEC_LOG_FEP_TAMPERED_THRESHOLD     10.0f  /**< Probable tampering */
#define SEC_LOG_FEP_ATTACK_THRESHOLD       20.0f  /**< Active attack */

/** @brief Precision bounds for detection sensitivity */
#define SEC_LOG_FEP_MIN_PRECISION          0.05f  /**< Minimum precision */
#define SEC_LOG_FEP_MAX_PRECISION          20.0f  /**< Maximum precision */
#define SEC_LOG_FEP_DEFAULT_PRECISION      1.0f   /**< Default precision */

/** @brief Surprise thresholds */
#define SEC_LOG_FEP_SURPRISE_LOW           2.5f   /**< Low surprise */
#define SEC_LOG_FEP_SURPRISE_MEDIUM        6.0f   /**< Medium surprise */
#define SEC_LOG_FEP_SURPRISE_HIGH          12.0f  /**< High surprise */

/** @brief Bio-async module ID */
#define BIO_MODULE_SECURITY_LOGGING_FEP    0x0E30 /**< Security logging FEP bridge */

/** @brief Maximum protective actions to evaluate */
#define SEC_LOG_FEP_MAX_ACTIONS            16

/** @brief History window for running averages */
#define SEC_LOG_FEP_HISTORY_SIZE           128

/** @brief Maximum injection pattern length */
#define SEC_LOG_FEP_MAX_PATTERN_LEN        256

/** @brief Maximum entries to analyze per batch */
#define SEC_LOG_FEP_MAX_BATCH_SIZE         64

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Log integrity levels based on free energy
 *
 * WHAT: Categorization of logging system health
 * WHY:  Drive appropriate security responses based on FE level
 */
typedef enum {
    SEC_LOG_FEP_INTEGRITY_NORMAL = 0,       /**< Low FE, logs are consistent */
    SEC_LOG_FEP_INTEGRITY_SUSPICIOUS,        /**< Elevated FE, needs monitoring */
    SEC_LOG_FEP_INTEGRITY_TAMPERED,          /**< High FE, probable tampering */
    SEC_LOG_FEP_INTEGRITY_COMPROMISED,       /**< Very high FE, active attack */
    SEC_LOG_FEP_INTEGRITY_COUNT
} sec_log_fep_integrity_t;

/**
 * @brief Protective action types via active inference
 *
 * WHAT: Actions that can minimize expected free energy
 * WHY:  Active inference selects actions to protect log integrity
 */
typedef enum {
    SEC_LOG_FEP_ACTION_NONE = 0,             /**< No action needed */
    SEC_LOG_FEP_ACTION_MONITOR,              /**< Increase monitoring */
    SEC_LOG_FEP_ACTION_PRESERVE,             /**< Preserve log snapshots */
    SEC_LOG_FEP_ACTION_INCREASE_DETAIL,      /**< Increase log detail level */
    SEC_LOG_FEP_ACTION_BLOCK_SOURCE,         /**< Block suspicious source */
    SEC_LOG_FEP_ACTION_ALERT,                /**< Alert security personnel */
    SEC_LOG_FEP_ACTION_LOCKDOWN,             /**< Lock logging system */
    SEC_LOG_FEP_ACTION_ROTATE,               /**< Force log rotation (preserve) */
    SEC_LOG_FEP_ACTION_COUNT
} sec_log_fep_action_t;

/**
 * @brief Detection event types
 *
 * WHAT: Types of log integrity violations detected
 * WHY:  Categorize detections for appropriate response
 */
typedef enum {
    SEC_LOG_FEP_DETECT_NONE = 0,             /**< No detection */
    SEC_LOG_FEP_DETECT_INJECTION,            /**< Log injection attack */
    SEC_LOG_FEP_DETECT_DELETION,             /**< Log deletion/truncation */
    SEC_LOG_FEP_DETECT_TIMESTAMP_MANIP,      /**< Timestamp manipulation */
    SEC_LOG_FEP_DETECT_AUDIT_CORRUPT,        /**< Audit trail corruption */
    SEC_LOG_FEP_DETECT_PATTERN_ANOMALY,      /**< Unusual log patterns */
    SEC_LOG_FEP_DETECT_SEQUENCE_GAP,         /**< Missing sequence numbers */
    SEC_LOG_FEP_DETECT_FORMAT_VIOLATION,     /**< Log format violations */
    SEC_LOG_FEP_DETECT_COUNT
} sec_log_fep_detection_t;

/**
 * @brief Injection attack subtypes
 *
 * WHAT: Specific types of log injection attacks
 * WHY:  Different injections require different responses
 */
typedef enum {
    SEC_LOG_FEP_INJECT_NONE = 0,             /**< No injection detected */
    SEC_LOG_FEP_INJECT_SQL,                  /**< SQL injection patterns */
    SEC_LOG_FEP_INJECT_COMMAND,              /**< Command injection */
    SEC_LOG_FEP_INJECT_SCRIPT,               /**< Script injection (XSS-like) */
    SEC_LOG_FEP_INJECT_NEWLINE,              /**< Newline injection (log splitting) */
    SEC_LOG_FEP_INJECT_FORMAT_STRING,        /**< Format string attack */
    SEC_LOG_FEP_INJECT_NULL_BYTE,            /**< Null byte injection */
    SEC_LOG_FEP_INJECT_COUNT
} sec_log_fep_inject_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security Logging FEP bridge configuration
 *
 * WHAT: Configuration parameters for FEP-logging integration
 * WHY:  Control sensitivity, thresholds, and detection behavior
 * HOW:  Set thresholds, enable features, configure modulation
 */
typedef struct {
    /* FEP parameters */
    float anomaly_fe_threshold;              /**< FE threshold for anomaly detection */
    float surprise_threshold;                /**< Surprise threshold for alerts */
    float precision_learning_rate;           /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_scoring;                    /**< Use FEP for anomaly scoring */
    bool enable_precision_modulation;        /**< Adapt precision based on detections */
    float normal_fe_threshold;               /**< FE threshold for normal state */
    float attack_fe_threshold;               /**< FE threshold for attack detection */

    /* Injection detection */
    bool detect_sql_injection;               /**< Detect SQL injection patterns */
    bool detect_command_injection;           /**< Detect command injection */
    bool detect_script_injection;            /**< Detect script/XSS patterns */
    bool detect_format_string;               /**< Detect format string attacks */

    /* Temporal analysis */
    bool enable_timestamp_analysis;          /**< Analyze timestamp patterns */
    float timestamp_tolerance_ms;            /**< Allowed timestamp drift (ms) */
    bool enforce_monotonic_time;             /**< Require monotonic timestamps */

    /* Sequence analysis */
    bool enable_sequence_analysis;           /**< Analyze sequence numbers */
    uint32_t max_sequence_gap;               /**< Maximum allowed sequence gap */

    /* Active inference protection */
    bool enable_active_protection;           /**< Enable active inference for protection */
    float action_temperature;                /**< Softmax temperature for action selection */
    uint32_t max_protective_actions;         /**< Max actions per cycle */

    /* Detection weights */
    float injection_weight;                  /**< Weight for injection detection in FE */
    float deletion_weight;                   /**< Weight for deletion detection */
    float timestamp_weight;                  /**< Weight for timestamp anomalies */
    float audit_weight;                      /**< Weight for audit corruption */

    /* Learning parameters */
    bool enable_online_learning;             /**< Update FEP from detections */
    float learning_rate;                     /**< Pattern learning rate */
    bool learn_from_false_positives;         /**< Update on FP feedback */

    /* Bio-async integration */
    bool enable_bio_async;                   /**< Enable bio-async callbacks */
} sec_log_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on security logging (FEP --> Security)
 *
 * WHAT: How FEP state affects security detection/response
 * WHY:  Free energy provides anomaly and tampering indicators
 */
typedef struct {
    /* Anomaly indicators */
    float fep_anomaly_score;                 /**< Anomaly score from FE [0-1] */
    float log_integrity_score;               /**< Log integrity [0-1, 1=healthy] */
    float temporal_integrity_score;          /**< Timestamp integrity [0-1] */
    float sequence_integrity_score;          /**< Sequence integrity [0-1] */

    /* Detection thresholds (precision-adjusted) */
    float adjusted_anomaly_threshold;        /**< Precision-adjusted threshold */
    float detection_sensitivity;             /**< Current sensitivity from precision */

    /* Surprise indicators */
    float surprise_score;                    /**< Current surprise level */
    float temporal_surprise;                 /**< Surprise from timestamp changes */
    float content_surprise;                  /**< Surprise from content analysis */

    /* Integrity classification */
    sec_log_fep_integrity_t integrity_level; /**< Current integrity level */

    /* Active inference outputs */
    sec_log_fep_action_t recommended_action; /**< Recommended protective action */
    float action_confidence;                 /**< Confidence in recommended action */
} sec_log_fep_effects_t;

/**
 * @brief Security effects on FEP (Security --> FEP)
 *
 * WHAT: How security detections affect FEP state
 * WHY:  Security events inform pattern learning and precision
 */
typedef struct {
    /* Detection counts */
    uint64_t entries_verified;               /**< Successfully verified log entries */
    uint64_t injections_detected;            /**< Injection attacks detected */
    uint64_t deletions_detected;             /**< Deletions/truncations detected */
    uint64_t timestamp_anomalies;            /**< Timestamp anomalies detected */
    uint64_t audit_corruptions;              /**< Audit trail corruptions detected */
    uint64_t sequence_gaps;                  /**< Sequence gaps detected */

    /* Running averages */
    float avg_entry_anomaly;                 /**< Average anomaly score per entry */
    float avg_temporal_deviation;            /**< Average timestamp deviation */
    float avg_content_divergence;            /**< Average content divergence */

    /* Attack state */
    bool attack_in_progress;                 /**< Currently under attack */
    sec_log_fep_detection_t current_attack;  /**< Type of current attack */
    sec_log_fep_inject_type_t inject_type;   /**< Injection type (if applicable) */
    float attack_severity;                   /**< Current attack severity */
} fep_security_log_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge operational state
 *
 * WHAT: Current state of the FEP-logging bridge
 * WHY:  Track active status, precision, and running metrics
 */
typedef struct {
    bool active;                             /**< Whether bridge is active */
    uint64_t update_count;                   /**< Number of updates */
    uint64_t entries_analyzed;               /**< Log entries analyzed */
    uint64_t detection_count;                /**< Total detections */
    uint64_t protection_count;               /**< Protective actions taken */

    /* Precision state */
    float current_precision;                 /**< Current precision level */
    float precision_velocity;                /**< Rate of precision change */

    /* Running averages */
    float avg_free_energy;                   /**< Running average FE */
    float avg_surprise;                      /**< Running average surprise */
    float avg_prediction_error;              /**< Running average pred error */

    /* Integrity tracking */
    sec_log_fep_integrity_t last_integrity;  /**< Last integrity level */
    uint64_t integrity_transitions;          /**< State transitions */

    /* Temporal state */
    uint64_t last_timestamp_ns;              /**< Last seen timestamp */
    uint32_t last_sequence_num;              /**< Last seen sequence number */
} sec_log_fep_state_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Accumulated statistics for monitoring and debugging
 * WHY:  Track performance and detection accuracy
 */
typedef struct {
    /* Detection statistics */
    uint64_t total_entries_analyzed;         /**< Total entries analyzed */
    uint64_t fep_based_detections;           /**< Detections using FEP */
    uint64_t injections_found;               /**< Injection attacks found */
    uint64_t deletions_found;                /**< Deletions found */
    uint64_t timestamp_violations;           /**< Timestamp violations */
    uint64_t audit_violations;               /**< Audit violations */
    uint64_t false_positives;                /**< Known false positives */

    /* Injection breakdown */
    uint64_t sql_injections;                 /**< SQL injection attempts */
    uint64_t command_injections;             /**< Command injection attempts */
    uint64_t script_injections;              /**< Script injection attempts */
    uint64_t format_string_attacks;          /**< Format string attacks */
    uint64_t newline_injections;             /**< Newline injection attempts */

    /* FEP metrics */
    float avg_free_energy;                   /**< Average free energy */
    float max_free_energy;                   /**< Maximum FE observed */
    float avg_surprise;                      /**< Average surprise */
    float max_surprise;                      /**< Maximum surprise observed */

    /* Precision tracking */
    float current_precision;                 /**< Current precision */
    uint64_t precision_adaptations;          /**< Precision updates */

    /* Protection statistics */
    uint64_t protections_attempted;          /**< Protection attempts */
    uint64_t protections_successful;         /**< Successful protections */
    float avg_protection_fe_reduction;       /**< Avg FE reduction per protection */

    /* Integrity statistics */
    uint64_t normal_states;                  /**< Time in normal state */
    uint64_t suspicious_states;              /**< Time in suspicious state */
    uint64_t tampered_states;                /**< Time in tampered state */
    uint64_t compromised_states;             /**< Time in compromised state */
} sec_log_fep_stats_t;

/* ============================================================================
 * Detection Result Structure
 * ============================================================================ */

/**
 * @brief Detection result from FEP-enhanced analysis
 *
 * WHAT: Result of log integrity verification using FEP
 * WHY:  Provide detailed detection information for response
 */
typedef struct {
    /* Primary detection */
    sec_log_fep_detection_t detection_type;  /**< Type of detection */
    sec_log_fep_inject_type_t inject_type;   /**< Injection subtype (if applicable) */
    float anomaly_score;                     /**< Overall anomaly score [0-1] */
    float confidence;                        /**< Detection confidence [0-1] */

    /* FEP metrics */
    float free_energy;                       /**< Current free energy */
    float surprise;                          /**< Surprise level */
    float prediction_error;                  /**< Prediction error magnitude */

    /* Integrity assessment */
    sec_log_fep_integrity_t integrity;       /**< Integrity classification */
    bool requires_action;                    /**< Whether action is needed */
    sec_log_fep_action_t recommended_action; /**< Recommended action */

    /* Details */
    uint64_t affected_entry_id;              /**< Affected log entry ID */
    uint64_t affected_timestamp_ns;          /**< Timestamp of affected entry */
    char pattern_matched[SEC_LOG_FEP_MAX_PATTERN_LEN]; /**< Matched injection pattern */
    char explanation[256];                   /**< Human-readable explanation */
} sec_log_fep_result_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Logging FEP bridge
 *
 * WHAT: Main bridge structure for FEP-logging integration
 * WHY:  Centralized control for free energy-based log security
 * HOW:  Contains FEP system, logging bridge, effects, and state
 */
typedef struct {
    bridge_base_t base;                      /**< MUST be first: base infrastructure */

    /* Configuration */
    sec_log_fep_config_t config;             /**< Bridge configuration */

    /* Connected systems */
    fep_system_t* fep_system;                /**< FEP system handle */
    security_logging_bridge_t* log_bridge;   /**< Security logging bridge */

    /* Bidirectional effects */
    sec_log_fep_effects_t fep_effects;       /**< FEP --> Security effects */
    fep_security_log_effects_t sec_effects;  /**< Security --> FEP effects */

    /* State and statistics */
    sec_log_fep_state_t state;               /**< Current operational state */
    sec_log_fep_stats_t stats;               /**< Accumulated statistics */

    /* History buffers for running averages */
    float* fe_history;                       /**< Free energy history */
    float* surprise_history;                 /**< Surprise history */
    uint32_t history_head;                   /**< Circular buffer head (deprecated - use per-buffer heads) */
    uint32_t history_count;                  /**< Number of history entries (deprecated) */
    uint32_t fe_history_head;                /**< FE history circular buffer head */
    uint32_t fe_history_count;               /**< FE history entry count */
    uint32_t surprise_history_head;          /**< Surprise history circular buffer head */
    uint32_t surprise_history_count;         /**< Surprise history entry count */

    /* Protection state */
    sec_log_fep_action_t pending_action;     /**< Pending protective action */
    uint64_t last_protection_time;           /**< Last protection timestamp */
} sec_log_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for FEP-logging integration
 * WHY:  Simplify initialization with security-focused defaults
 * HOW:  Return standard thresholds and detection settings
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * Defaults:
 * - anomaly_fe_threshold: 5.0
 * - surprise_threshold: 6.0
 * - precision_learning_rate: 0.05
 * - enable_active_protection: true
 * - All injection detection types enabled
 */
int sec_log_fep_default_config(sec_log_fep_config_t* config);

/**
 * @brief Create security logging FEP bridge
 *
 * WHAT: Initialize FEP integration for security logging
 * WHY:  Enable free energy-based log tampering and injection detection
 * HOW:  Connect FEP system to logging bridge, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param log_bridge Security logging bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 *
 * Memory: ~48KB for default configuration
 * Thread safety: Returned handle is thread-safe
 */
sec_log_fep_bridge_t* sec_log_fep_create(
    const sec_log_fep_config_t* config,
    security_logging_bridge_t* log_bridge,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security logging FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async, cleanup base
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_log_fep_destroy(sec_log_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Fresh start without reconnection overhead
 * HOW:  Zero effects, reset statistics, clear history
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_log_fep_reset(sec_log_fep_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_log_fep_get_config(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_config_t* config
);

/**
 * @brief Update configuration
 *
 * WHAT: Update bridge configuration at runtime
 * WHY:  Allow dynamic adjustment of sensitivity and thresholds
 * HOW:  Validate and apply new configuration
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_log_fep_set_config(
    sec_log_fep_bridge_t* bridge,
    const sec_log_fep_config_t* config
);

/* ============================================================================
 * Compute and Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on security logging
 *
 * WHAT: Calculate FEP-derived anomaly and integrity scores
 * WHY:  Use free energy for log integrity assessment
 * HOW:  Process current FEP state, compute integrity metrics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_log_fep_compute_effects(sec_log_fep_bridge_t* bridge);

/**
 * @brief Analyze log entry for tampering
 *
 * WHAT: FEP-enhanced analysis of individual log entry
 * WHY:  Detect injection, manipulation, and anomalies
 * HOW:  Compare entry against learned patterns, compute FE
 *
 * @param bridge Bridge handle
 * @param entry Log entry to analyze
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_log_fep_analyze_entry(
    sec_log_fep_bridge_t* bridge,
    const security_log_entry_t* entry,
    sec_log_fep_result_t* result
);

/**
 * @brief Detect log injection attacks
 *
 * WHAT: Scan log message for injection patterns
 * WHY:  Prevent attackers from injecting malicious log entries
 * HOW:  Pattern matching + FEP surprise analysis
 *
 * @param bridge Bridge handle
 * @param message Log message to analyze
 * @param message_len Message length
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_log_fep_detect_injection(
    sec_log_fep_bridge_t* bridge,
    const char* message,
    size_t message_len,
    sec_log_fep_result_t* result
);

/**
 * @brief Check for log deletion/truncation
 *
 * WHAT: Detect missing log entries in sequence
 * WHY:  Attackers may delete incriminating logs
 * HOW:  Sequence analysis + temporal gap detection
 *
 * @param bridge Bridge handle
 * @param expected_seq Expected sequence number
 * @param actual_seq Actual sequence number received
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_log_fep_detect_deletion(
    sec_log_fep_bridge_t* bridge,
    uint32_t expected_seq,
    uint32_t actual_seq,
    sec_log_fep_result_t* result
);

/**
 * @brief Detect timestamp manipulation
 *
 * WHAT: Identify non-monotonic or implausible timestamps
 * WHY:  Attackers may backdate or modify timestamps
 * HOW:  Temporal pattern analysis + FEP temporal precision
 *
 * @param bridge Bridge handle
 * @param timestamp_ns Timestamp to verify (nanoseconds)
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_log_fep_detect_timestamp_manipulation(
    sec_log_fep_bridge_t* bridge,
    uint64_t timestamp_ns,
    sec_log_fep_result_t* result
);

/**
 * @brief Verify audit trail integrity
 *
 * WHAT: Check audit trail hash chain for corruption
 * WHY:  Detect tampering with historical logs
 * HOW:  Hash verification + FEP chain consistency model
 *
 * @param bridge Bridge handle
 * @param expected_hash Expected chain hash
 * @param actual_hash Actual computed hash
 * @param result Output detection result
 * @return 0 on success, -1 on error
 */
int sec_log_fep_verify_audit_trail(
    sec_log_fep_bridge_t* bridge,
    uint64_t expected_hash,
    uint64_t actual_hash,
    sec_log_fep_result_t* result
);

/**
 * @brief Update bridge from detection event
 *
 * WHAT: Feed detection back to FEP for learning
 * WHY:  Update generative model from security events
 * HOW:  Convert detection to FEP observation, update beliefs
 *
 * @param bridge Bridge handle
 * @param detection Detection type
 * @param severity Detection severity [0-1]
 * @param entry_id Affected entry ID (0 if none)
 * @return 0 on success, -1 on error
 */
int sec_log_fep_update_from_detection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_detection_t detection,
    float severity,
    uint64_t entry_id
);

/**
 * @brief Apply precision modulation
 *
 * WHAT: Adjust detection precision based on FEP state
 * WHY:  Adapt sensitivity to current integrity level
 * HOW:  Modulate precision based on detection performance
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_log_fep_apply_precision_modulation(sec_log_fep_bridge_t* bridge);

/* ============================================================================
 * Active Inference Protection API
 * ============================================================================ */

/**
 * @brief Select protective action via active inference
 *
 * WHAT: Choose action that minimizes expected free energy
 * WHY:  Active inference guides optimal protection
 * HOW:  Evaluate actions, select via softmax over -EFE
 *
 * @param bridge Bridge handle
 * @param action_out Output selected action
 * @param confidence_out Output action confidence
 * @return 0 on success, -1 on error
 */
int sec_log_fep_select_protection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t* action_out,
    float* confidence_out
);

/**
 * @brief Execute protective action
 *
 * WHAT: Perform the selected protective action
 * WHY:  Reduce free energy by protecting log integrity
 * HOW:  Execute action type on logging system
 *
 * @param bridge Bridge handle
 * @param action Action to execute
 * @param target_entry_id Target entry (0 if not applicable)
 * @return 0 on success, -1 on error
 */
int sec_log_fep_execute_protection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t action,
    uint64_t target_entry_id
);

/**
 * @brief Report protection outcome
 *
 * WHAT: Report whether protection was successful
 * WHY:  Update FEP from action outcomes for learning
 * HOW:  Measure FE reduction, update precision
 *
 * @param bridge Bridge handle
 * @param action Action that was executed
 * @param success Whether action succeeded
 * @param fe_reduction Free energy reduction achieved
 * @return 0 on success, -1 on error
 */
int sec_log_fep_report_protection(
    sec_log_fep_bridge_t* bridge,
    sec_log_fep_action_t action,
    bool success,
    float fe_reduction
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on security
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sec_log_fep_get_effects(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_effects_t* effects
);

/**
 * @brief Get security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int sec_log_fep_get_security_effects(
    const sec_log_fep_bridge_t* bridge,
    fep_security_log_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int sec_log_fep_get_stats(
    const sec_log_fep_bridge_t* bridge,
    sec_log_fep_stats_t* stats
);

/**
 * @brief Get current anomaly score
 *
 * @param bridge Bridge handle
 * @return Current anomaly score [0-1] or -1.0 on error
 */
float sec_log_fep_get_anomaly_score(const sec_log_fep_bridge_t* bridge);

/**
 * @brief Get current integrity level
 *
 * @param bridge Bridge handle
 * @return Current integrity level or -1 on error
 */
sec_log_fep_integrity_t sec_log_fep_get_integrity(
    const sec_log_fep_bridge_t* bridge
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0 on error
 */
float sec_log_fep_get_free_energy(const sec_log_fep_bridge_t* bridge);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_log_fep_reset_stats(sec_log_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module log security notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_log_fep_connect_bio_async(sec_log_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_log_fep_disconnect_bio_async(sec_log_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool sec_log_fep_is_bio_async_connected(const sec_log_fep_bridge_t* bridge);

/**
 * @brief Process bio-async inbox messages
 *
 * WHAT: Process pending messages from other modules
 * WHY:  Handle security alerts and log requests
 * HOW:  Use bio_router_process_inbox for message handling
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t sec_log_fep_process_inbox(
    sec_log_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get integrity level name
 *
 * @param level Integrity level
 * @return Human-readable name
 */
const char* sec_log_fep_integrity_name(sec_log_fep_integrity_t level);

/**
 * @brief Get action name
 *
 * @param action Action type
 * @return Human-readable name
 */
const char* sec_log_fep_action_name(sec_log_fep_action_t action);

/**
 * @brief Get detection type name
 *
 * @param detection Detection type
 * @return Human-readable name
 */
const char* sec_log_fep_detection_name(sec_log_fep_detection_t detection);

/**
 * @brief Get injection type name
 *
 * @param inject_type Injection type
 * @return Human-readable name
 */
const char* sec_log_fep_inject_type_name(sec_log_fep_inject_type_t inject_type);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Bridge handle
 */
void sec_log_fep_print_summary(const sec_log_fep_bridge_t* bridge);

/**
 * @brief Print statistics (debug)
 *
 * @param stats Statistics to print
 */
void sec_log_fep_print_stats(const sec_log_fep_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_LOGGING_FEP_BRIDGE_H */
