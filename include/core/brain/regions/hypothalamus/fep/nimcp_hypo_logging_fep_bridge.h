/**
 * @file nimcp_hypo_logging_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Logging Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic system logging and monitoring
 * WHY:  Event frequency anomalies and log patterns represent deviations
 *       from expected homeostatic behavior in the predictive processing framework
 * HOW:  Map event frequency to free energy, anomaly detection to prediction error,
 *       and use precision modulation based on system load
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC MONITORING AS PREDICTIVE PROCESSING:
 * -------------------------------------------------
 * The hypothalamus monitors bodily functions and generates signals when anomalies
 * are detected. Normal operation has predictable event patterns - deviations generate:
 *
 * 1. Normal event frequency = low free energy (expected patterns)
 * 2. Elevated event rate = high free energy (potential stress)
 * 3. Anomalous events = prediction error (unexpected signals)
 * 4. Event pattern changes = belief violation (system state shift)
 *
 * LOGGING AS INTEROCEPTIVE AWARENESS:
 * Similar to how the hypothalamus monitors internal body signals (interoception),
 * this bridge monitors internal system events to detect homeostatic disruptions.
 *
 * FEP INTEGRATION:
 * ```
 * Log Event (e) -> Pattern Analysis
 *         |
 * Expected Event Pattern mu (learned event baseline)
 *         |
 * Prediction Error: epsilon = e - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(e) <= F
 *         |
 * System Stress Level = F / F_threshold
 * ```
 *
 * FEP MAPPINGS:
 * - Event frequency -> Free energy (deviation from expected rate)
 * - Anomaly detection -> Prediction error (unexpected events)
 * - Error rate -> Surprise level
 * - Pattern changes -> Belief divergence
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0)  -> Normal operation
 * - Medium FE (2-5) -> Elevated activity (monitoring)
 * - High FE (5-10)  -> Significant anomalies (alert)
 * - Very high FE (>10) -> Critical system stress
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_logging.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_LOGGING_FEP_BRIDGE_H
#define NIMCP_HYPO_LOGGING_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for hypothalamus logging FEP bridge */
#define BIO_MODULE_HYPO_LOGGING_FEP     0x0B03

/** Free energy thresholds for system stress levels */
#define HYPO_LOGGING_FEP_NORMAL_THRESHOLD      2.0f   /**< Normal operation */
#define HYPO_LOGGING_FEP_ELEVATED_THRESHOLD    5.0f   /**< Elevated activity */
#define HYPO_LOGGING_FEP_ANOMALY_THRESHOLD     10.0f  /**< Significant anomalies */
#define HYPO_LOGGING_FEP_CRITICAL_THRESHOLD    20.0f  /**< Critical stress */

/** Precision bounds */
#define HYPO_LOGGING_FEP_MIN_PRECISION         0.1f   /**< Minimum precision */
#define HYPO_LOGGING_FEP_MAX_PRECISION         10.0f  /**< Maximum precision */
#define HYPO_LOGGING_FEP_DEFAULT_PRECISION     1.0f   /**< Default precision */

/** Event rate thresholds */
#define HYPO_LOGGING_FEP_RATE_WINDOW_MS        1000   /**< Rate calculation window */
#define HYPO_LOGGING_FEP_MAX_EVENTS_PER_SEC    1000   /**< Max expected events/sec */

/** Anomaly detection thresholds */
#define HYPO_LOGGING_FEP_ERROR_RATE_THRESHOLD  0.05f  /**< Error rate threshold */
#define HYPO_LOGGING_FEP_WARNING_RATE_THRESHOLD 0.15f /**< Warning rate threshold */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief System stress levels based on FEP metrics
 *
 * WHAT: Categorization of system stress from logging patterns
 * WHY:  Enable graded response to system anomalies
 */
typedef enum {
    HYPO_LOGGING_FEP_LEVEL_NORMAL = 0,        /**< Normal operation */
    HYPO_LOGGING_FEP_LEVEL_ELEVATED,          /**< Elevated activity */
    HYPO_LOGGING_FEP_LEVEL_STRESSED,          /**< System stressed */
    HYPO_LOGGING_FEP_LEVEL_ANOMALOUS,         /**< Significant anomalies */
    HYPO_LOGGING_FEP_LEVEL_CRITICAL           /**< Critical - immediate action */
} hypo_logging_fep_level_t;

/**
 * @brief Active inference response types for logging
 *
 * WHAT: Types of responses via active inference
 * WHY:  Different stress levels require different interventions
 */
typedef enum {
    HYPO_LOGGING_FEP_RESPONSE_NONE = 0,       /**< No response needed */
    HYPO_LOGGING_FEP_RESPONSE_MONITOR,        /**< Increase monitoring */
    HYPO_LOGGING_FEP_RESPONSE_THROTTLE,       /**< Throttle event rate */
    HYPO_LOGGING_FEP_RESPONSE_ALERT,          /**< Generate alert */
    HYPO_LOGGING_FEP_RESPONSE_EMERGENCY       /**< Emergency response */
} hypo_logging_fep_response_t;

/**
 * @brief Anomaly types detected by FEP
 *
 * WHAT: Classification of logging anomaly types
 * WHY:  Different anomalies require different handling
 */
typedef enum {
    HYPO_LOGGING_ANOMALY_NONE = 0,            /**< No anomaly */
    HYPO_LOGGING_ANOMALY_RATE,                /**< Event rate anomaly */
    HYPO_LOGGING_ANOMALY_ERROR_SPIKE,         /**< Error rate spike */
    HYPO_LOGGING_ANOMALY_WARNING_SPIKE,       /**< Warning rate spike */
    HYPO_LOGGING_ANOMALY_PATTERN              /**< Pattern anomaly */
} hypo_logging_anomaly_type_t;

/**
 * @brief Log severity levels
 *
 * WHAT: Severity levels for log events
 * WHY:  Weight events by severity in FE computation
 */
typedef enum {
    HYPO_LOG_SEVERITY_DEBUG = 0,              /**< Debug level */
    HYPO_LOG_SEVERITY_INFO,                   /**< Info level */
    HYPO_LOG_SEVERITY_WARNING,                /**< Warning level */
    HYPO_LOG_SEVERITY_ERROR,                  /**< Error level */
    HYPO_LOG_SEVERITY_CRITICAL,               /**< Critical level */
    HYPO_LOG_SEVERITY_COUNT
} hypo_log_severity_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus logging FEP configuration
 *
 * WHAT: Configuration for FEP-logging integration
 * WHY:  Control detection sensitivity and response parameters
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from event deviation */
    float precision_modulation;               /**< Precision based on fatigue */
    bool enable_active_inference;             /**< Allow logging-based actions */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* Event rate parameters */
    uint32_t rate_window_ms;                  /**< Window for rate calculation */
    float expected_event_rate;                /**< Expected events per second */
    float rate_deviation_threshold;           /**< Rate deviation for alarm */

    /* Severity weights */
    float debug_weight;                       /**< Weight for debug events */
    float info_weight;                        /**< Weight for info events */
    float warning_weight;                     /**< Weight for warning events */
    float error_weight;                       /**< Weight for error events */
    float critical_weight;                    /**< Weight for critical events */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */
    float error_rate_threshold;               /**< Error rate threshold */
    float warning_rate_threshold;             /**< Warning rate threshold */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_logging_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output
 *
 * WHAT: How FEP analysis affects logging monitoring
 * WHY:  Free energy provides system stress signal
 */
typedef struct {
    float free_energy;                        /**< Current FE from events */
    float prediction_error;                   /**< PE from event patterns */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_logging_fep_level_t stress_level;    /**< Stress classification */
    float stress_confidence;                  /**< Detection confidence [0-1] */

    hypo_logging_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    hypo_logging_anomaly_type_t detected_anomaly; /**< Most significant anomaly */
    float system_health;                      /**< System health estimate [0-1] */

    /* Event-derived metrics */
    float event_rate;                         /**< Current event rate (events/sec) */
    float error_rate;                         /**< Error proportion [0-1] */
    float warning_rate;                       /**< Warning proportion [0-1] */
    float anomaly_score;                      /**< Anomaly score [0-1] */
} hypo_logging_fep_effects_t;

/**
 * @brief Logging effects on FEP
 *
 * WHAT: How logging events affect FEP beliefs
 * WHY:  Event patterns update the generative model
 */
typedef struct {
    /* Event counts by severity */
    uint64_t debug_count;                     /**< Debug events */
    uint64_t info_count;                      /**< Info events */
    uint64_t warning_count;                   /**< Warning events */
    uint64_t error_count;                     /**< Error events */
    uint64_t critical_count;                  /**< Critical events */
    uint64_t total_events;                    /**< Total events */

    /* Rate metrics */
    float current_event_rate;                 /**< Current event rate */
    float current_error_rate;                 /**< Current error rate */
    float current_warning_rate;               /**< Current warning rate */

    /* Anomaly tracking */
    uint64_t rate_anomalies;                  /**< Rate anomalies detected */
    uint64_t error_spikes;                    /**< Error spikes detected */
    uint64_t warning_spikes;                  /**< Warning spikes detected */
    uint64_t pattern_anomalies;               /**< Pattern anomalies detected */

    /* State */
    bool system_stressed;                     /**< System stressed */
    bool anomaly_active;                      /**< Anomaly currently active */
} logging_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Event rate tracking
 *
 * WHAT: Tracks event rates for prediction
 * WHY:  Enables FEP-based rate anomaly detection
 */
typedef struct {
    uint64_t window_start_ms;                 /**< Current window start */
    uint32_t events_in_window;                /**< Events in current window */
    uint32_t by_severity[HYPO_LOG_SEVERITY_COUNT]; /**< Events by severity */
    float rate_history[16];                   /**< Recent rate history */
    uint32_t rate_history_idx;                /**< Current history index */

    float predicted_rate;                     /**< FEP predicted rate */
    float mean_rate;                          /**< Mean event rate */
    float variance_rate;                      /**< Rate variance */
} hypo_logging_rate_tracking_t;

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring
 */
typedef struct {
    bool active;                              /**< Whether bridge is active */
    uint64_t update_count;                    /**< Number of updates */
    uint64_t detection_count;                 /**< Detections processed */

    float current_precision;                  /**< Current precision level */
    float avg_surprise;                       /**< Running average surprise */
    float avg_prediction_error;               /**< Running average PE */

    hypo_logging_fep_level_t last_level;      /**< Last stress level */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    hypo_logging_rate_tracking_t rate_tracking; /**< Rate tracking state */
} hypo_logging_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;                   /**< Total updates performed */
    uint64_t fep_detections;                  /**< FEP-based detections */
    uint64_t anomalies_detected;              /**< Anomalies found */
    uint64_t responses_triggered;             /**< Responses triggered */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By anomaly type */
    uint64_t rate_anomalies;                  /**< Rate anomalies */
    uint64_t error_spikes;                    /**< Error spikes */
    uint64_t warning_spikes;                  /**< Warning spikes */
    uint64_t pattern_anomalies;               /**< Pattern anomalies */

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float max_event_rate;                     /**< Maximum event rate observed */
    float max_error_rate;                     /**< Maximum error rate observed */
} hypo_logging_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Logging FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus logging to FEP
 * WHY:  Centralized integration of logging with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_logging_fep_config_t config;         /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_logging_fep_effects_t fep_effects;   /**< FEP -> Logging effects */
    logging_to_fep_effects_t log_effects;     /**< Logging -> FEP effects */

    /* State and statistics */
    hypo_logging_fep_state_t state;           /**< Current state */
    hypo_logging_fep_stats_t stats;           /**< Statistics */
} hypo_logging_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for logging FEP integration
 * WHY:  Simplify initialization with balanced settings
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_default_config(hypo_logging_fep_config_t* config);

/**
 * @brief Create hypothalamus logging FEP bridge
 *
 * WHAT: Initialize FEP integration for logging
 * WHY:  Enable surprise-based event monitoring
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_logging_fep_bridge_t* hypo_logging_fep_create(
    const hypo_logging_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus logging FEP bridge
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_logging_fep_destroy(hypo_logging_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_reset(hypo_logging_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_update(hypo_logging_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from drive state
 *
 * WHAT: Calculate FE from current drive and event state
 * WHY:  Core FEP computation for logging monitoring
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_compute_fe(
    hypo_logging_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on fatigue
 *
 * WHAT: Adjust detection precision based on fatigue
 * WHY:  Precision represents confidence; fatigue reduces confidence
 *
 * @param bridge Bridge handle
 * @param fatigue Fatigue level [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_modulate_precision(
    hypo_logging_fep_bridge_t* bridge,
    float fatigue
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_get_effects(
    const hypo_logging_fep_bridge_t* bridge,
    hypo_logging_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_get_stats(
    const hypo_logging_fep_bridge_t* bridge,
    hypo_logging_fep_stats_t* stats
);

/* ============================================================================
 * Event Processing API
 * ============================================================================ */

/**
 * @brief Log an event for FEP processing
 *
 * WHAT: Feed a log event to the bridge
 * WHY:  Events update FEP beliefs about system state
 *
 * @param bridge Bridge handle
 * @param severity Event severity level
 * @param timestamp_ms Event timestamp (ms)
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_log_event(
    hypo_logging_fep_bridge_t* bridge,
    hypo_log_severity_t severity,
    uint64_t timestamp_ms
);

/**
 * @brief Detect anomaly in current event pattern
 *
 * WHAT: Analyze current pattern for anomalies
 * WHY:  Anomaly detection is core FEP function
 *
 * @param bridge Bridge handle
 * @param anomaly_out Output anomaly type
 * @param severity_out Output severity [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_detect_anomaly(
    hypo_logging_fep_bridge_t* bridge,
    hypo_logging_anomaly_type_t* anomaly_out,
    float* severity_out
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_connect_bio_async(
    hypo_logging_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_logging_fep_disconnect_bio_async(hypo_logging_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_logging_fep_process_messages(
    hypo_logging_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get stress level name
 *
 * @param level Stress level
 * @return Human-readable name
 */
const char* hypo_logging_fep_level_name(hypo_logging_fep_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_logging_fep_response_name(hypo_logging_fep_response_t response);

/**
 * @brief Get anomaly type name
 *
 * @param anomaly Anomaly type
 * @return Human-readable name
 */
const char* hypo_logging_fep_anomaly_name(hypo_logging_anomaly_type_t anomaly);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return Human-readable name
 */
const char* hypo_logging_fep_severity_name(hypo_log_severity_t severity);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_logging_fep_print_summary(const hypo_logging_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_LOGGING_FEP_BRIDGE_H */
