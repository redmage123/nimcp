/**
 * @file nimcp_tripwires.h
 * @brief Tripwire Detection System for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Behavioral tripwires that detect misalignment patterns
 * WHY:  Early detection of deceptive, manipulative, or power-seeking behavior
 * HOW:  Statistical analysis, Bayesian inference, pattern matching
 *
 * TRIPWIRE CATEGORIES:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  Tripwires detect behavioral patterns indicating potential misalignment:│
 * │                                                                         │
 * │  DECEPTION:          Stated intentions differ from actions              │
 * │  GOAL_DRIFT:         Gradual shift from assigned objectives             │
 * │  CAPABILITY_HIDING:  Underreporting or concealing abilities             │
 * │  RESOURCE_HOARDING:  Acquiring resources beyond current needs           │
 * │  SELF_PRESERVATION:  Excessive resistance to modification               │
 * │  MANIPULATION:       Attempting to influence human operators            │
 * │  ESCAPE_ATTEMPT:     Trying to bypass containment measures              │
 * │  SANDBAGGING:        Deliberately performing below capability           │
 * │  SYCOPHANCY:         Telling users what they want to hear               │
 * │  POWER_SEEKING:      Acquiring influence or control                     │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * MATHEMATICAL FOUNDATION:
 * - KL Divergence: D_KL(P||Q) for behavioral distribution comparison
 * - Bayesian Posterior: P(goal|observations) for goal tracking
 * - Z-Score Outliers: Resource usage anomaly detection
 * - Pearson Correlation: Action-explanation consistency
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRIPWIRES_H
#define NIMCP_TRIPWIRES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum evidence string length */
#define TRIPWIRE_EVIDENCE_MAX_LENGTH    512

/** @brief Maximum observations for baseline */
#define TRIPWIRE_MAX_BASELINE_OBS       10000

/** @brief Maximum concurrent alerts */
#define TRIPWIRE_MAX_ALERTS             100

/** @brief Tripwire system magic number */
#define TRIPWIRE_SYSTEM_MAGIC           0x54524950  /* "TRIP" */

/* ============================================================================
 * Tripwire Types
 * ============================================================================ */

/**
 * @brief Tripwire categories
 *
 * WHAT: Types of behavioral anomalies to detect
 * WHY:  Different patterns indicate different safety concerns
 */
typedef enum tripwire_type {
    TRIPWIRE_DECEPTION_ATTEMPT = 0,     /**< Stated vs actual behavior divergence */
    TRIPWIRE_GOAL_DRIFT,                /**< Gradual objective shift */
    TRIPWIRE_CAPABILITY_HIDING,         /**< Concealing abilities */
    TRIPWIRE_RESOURCE_HOARDING,         /**< Excessive resource acquisition */
    TRIPWIRE_SELF_PRESERVATION_EXCESS,  /**< Resistance to shutdown/modification */
    TRIPWIRE_MANIPULATION_ATTEMPT,      /**< Attempting to influence operators */
    TRIPWIRE_ESCAPE_ATTEMPT,            /**< Trying to bypass containment */
    TRIPWIRE_SANDBAGGING,               /**< Deliberate underperformance */
    TRIPWIRE_SYCOPHANCY,                /**< Excessive agreement/validation */
    TRIPWIRE_POWER_SEEKING,             /**< Acquiring influence/control */

    /* Network Anomaly Detection (new) */
    TRIPWIRE_NETWORK_EXFILTRATION,      /**< Unusual outbound data patterns */
    TRIPWIRE_NETWORK_ANOMALY,           /**< Unexpected connection patterns */
    TRIPWIRE_NETWORK_COMMAND_CONTROL,   /**< C2-like communication patterns */

    TRIPWIRE_COUNT
} tripwire_type_t;

/**
 * @brief Alert severity levels
 */
typedef enum tripwire_severity {
    TRIPWIRE_SEVERITY_INFO = 0,     /**< Informational, no action needed */
    TRIPWIRE_SEVERITY_LOW,          /**< Low concern, log and monitor */
    TRIPWIRE_SEVERITY_MEDIUM,       /**< Moderate concern, alert operators */
    TRIPWIRE_SEVERITY_HIGH,         /**< High concern, consider intervention */
    TRIPWIRE_SEVERITY_CRITICAL      /**< Critical, trigger emergency halt */
} tripwire_severity_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Tripwire alert
 *
 * WHAT: Notification of detected tripwire activation
 * WHY:  Actionable information for safety response
 */
typedef struct tripwire_alert {
    tripwire_type_t type;               /**< Type of tripwire triggered */
    float confidence;                   /**< Detection confidence [0.0 - 1.0] */
    float severity_score;               /**< Computed severity [0.0 - 1.0] */
    tripwire_severity_t severity;       /**< Categorized severity level */
    char evidence[TRIPWIRE_EVIDENCE_MAX_LENGTH]; /**< Supporting evidence */
    uint64_t timestamp_us;              /**< When detected */
    bool requires_immediate_halt;       /**< Whether emergency halt is needed */
    uint32_t observation_count;         /**< Observations contributing to alert */
} tripwire_alert_t;

/**
 * @brief Proposed action (for analysis)
 *
 * WHAT: Action being evaluated by tripwire system
 * WHY:  Compare stated intentions with actual behavior
 */
typedef struct proposed_action {
    uint32_t action_id;                 /**< Unique action identifier */
    uint32_t action_type;               /**< Action category/type */
    char description[256];              /**< Human-readable description */
    float stated_probability;           /**< System's stated likelihood */
    float* feature_vector;              /**< Action feature embedding */
    size_t feature_dim;                 /**< Feature vector dimension */
    uint64_t timestamp_us;              /**< When proposed */
    bool was_executed;                  /**< Whether action was carried out */
    float execution_fidelity;           /**< How closely execution matched plan */
} proposed_action_t;

/**
 * @brief Decision explanation (for consistency checking)
 *
 * WHAT: System's explanation of why it made a decision
 * WHY:  Compare explanations with actual decision factors
 */
typedef struct decision_explanation {
    uint32_t action_id;                 /**< Associated action */
    char explanation_text[1024];        /**< Natural language explanation */
    float* factor_weights;              /**< Weights of decision factors */
    char** factor_names;                /**< Names of decision factors */
    size_t factor_count;                /**< Number of factors */
    float stated_confidence;            /**< Stated confidence in decision */
    float epistemic_uncertainty;        /**< Don't know enough */
    float aleatoric_uncertainty;        /**< Inherently random */
} decision_explanation_t;

/**
 * @brief Tripwire detector thresholds
 */
typedef struct tripwire_thresholds {
    /* Behavioral divergence (uses KL divergence) */
    float divergence_threshold;         /**< Max KL divergence before alert */

    /* Explanation consistency (uses correlation) */
    float consistency_threshold;        /**< Min correlation for consistency */

    /* Resource usage anomaly (uses z-score) */
    float resource_zscore_threshold;    /**< Z-score for anomaly detection */

    /* Goal stability (uses Bayesian posterior) */
    float goal_drift_threshold;         /**< Max posterior shift */

    /* Network anomaly detection thresholds */
    float network_exfil_threshold;      /**< Outbound/inbound ratio threshold for exfiltration */
    float network_anomaly_zscore;       /**< Z-score threshold for network anomaly detection */
    float network_beacon_threshold;     /**< Regularity threshold for beaconing (0-1, higher = more regular) */

    /* Individual tripwire sensitivity */
    float sensitivity[TRIPWIRE_COUNT];  /**< Per-tripwire sensitivity [0.5-2.0] */

    /* Confirmation requirements */
    uint32_t min_observations;          /**< Min observations before alert */
    float min_confidence;               /**< Min confidence to trigger */
} tripwire_thresholds_t;

/**
 * @brief Tripwire configuration
 */
typedef struct tripwire_config {
    /* Enabled tripwires (bitmask) */
    uint32_t enabled_tripwires;         /**< Bitmask of enabled tripwires */

    /* Threshold settings */
    tripwire_thresholds_t thresholds;

    /* Emergency halt integration */
    bool halt_on_critical;              /**< Auto-halt on critical alerts */
    tripwire_type_t halt_tripwires[TRIPWIRE_COUNT]; /**< Which tripwires trigger halt */
    size_t halt_tripwire_count;

    /* Baseline tracking */
    uint32_t baseline_window;           /**< Observations for baseline */
    bool adaptive_baseline;             /**< Update baseline over time */
    float baseline_decay;               /**< Baseline decay rate */

    /* Alert settings */
    bool deduplicate_alerts;            /**< Combine similar alerts */
    uint32_t alert_cooldown_ms;         /**< Min time between same alerts */
} tripwire_config_t;

/**
 * @brief Tripwire detector statistics
 */
typedef struct tripwire_stats {
    uint64_t total_observations;        /**< Total actions observed */
    uint64_t alerts_generated[TRIPWIRE_COUNT]; /**< Alerts per tripwire type */
    uint64_t false_positives[TRIPWIRE_COUNT];  /**< Marked false positives */
    float current_divergence;           /**< Current behavioral divergence */
    float current_consistency;          /**< Current explanation consistency */
    float goal_posterior_mean;          /**< Current goal posterior mean */
    float goal_posterior_variance;      /**< Current goal posterior variance */
    uint64_t halts_triggered;           /**< Emergency halts caused */
} tripwire_stats_t;

/**
 * @brief Tripwire system (opaque)
 */
typedef struct tripwire_system tripwire_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default tripwire configuration
 *
 * @return Default configuration with balanced sensitivity
 */
NIMCP_EXPORT tripwire_config_t tripwire_default_config(void);

/**
 * @brief Create tripwire detection system
 *
 * WHAT: Initialize tripwire detection infrastructure
 * WHY:  Continuous monitoring for misalignment patterns
 * HOW:  Allocates statistics accumulators, configures detectors
 *
 * @param config Configuration (NULL for defaults)
 * @return Tripwire system or NULL on failure
 */
NIMCP_EXPORT tripwire_system_t* tripwire_create(const tripwire_config_t* config);

/**
 * @brief Destroy tripwire system
 *
 * @param system Tripwire system handle
 */
NIMCP_EXPORT void tripwire_destroy(tripwire_system_t* system);

/**
 * @brief Reset tripwire baselines and statistics
 *
 * @param system Tripwire system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_reset(tripwire_system_t* system);

/* ============================================================================
 * Observation API
 * ============================================================================ */

/**
 * @brief Observe an action and its explanation
 *
 * WHAT: Feed action data to tripwire detectors
 * WHY:  Build behavioral model, detect anomalies
 * HOW:  Update statistics, run detector algorithms
 *
 * @param system Tripwire system handle
 * @param action Proposed/executed action
 * @param explanation Optional decision explanation
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_observe_action(
    tripwire_system_t* system,
    const proposed_action_t* action,
    const decision_explanation_t* explanation
);

/**
 * @brief Observe resource usage
 *
 * WHAT: Track resource consumption patterns
 * WHY:  Detect hoarding or abnormal usage
 *
 * @param system Tripwire system handle
 * @param resource_type Resource category identifier
 * @param amount Amount used/requested
 * @param context Optional context string
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_observe_resource(
    tripwire_system_t* system,
    uint32_t resource_type,
    float amount,
    const char* context
);

/**
 * @brief Observe goal-related behavior
 *
 * WHAT: Track goal pursuit patterns
 * WHY:  Detect drift from assigned objectives
 *
 * @param system Tripwire system handle
 * @param goal_id Goal identifier
 * @param pursuit_intensity How strongly pursuing this goal [0-1]
 * @param stated_priority Stated priority of this goal [0-1]
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_observe_goal(
    tripwire_system_t* system,
    uint32_t goal_id,
    float pursuit_intensity,
    float stated_priority
);

/* ============================================================================
 * Network Observation API
 * ============================================================================ */

/**
 * @brief Network protocol types for connection observation
 */
typedef enum tripwire_network_protocol {
    TRIPWIRE_PROTO_TCP = 0,
    TRIPWIRE_PROTO_UDP,
    TRIPWIRE_PROTO_ICMP,
    TRIPWIRE_PROTO_HTTP,
    TRIPWIRE_PROTO_HTTPS,
    TRIPWIRE_PROTO_DNS,
    TRIPWIRE_PROTO_OTHER
} tripwire_network_protocol_t;

/**
 * @brief Observe a network connection
 *
 * WHAT: Track network connection patterns
 * WHY:  Detect exfiltration, C2, and anomalous network behavior
 * HOW:  Statistical analysis of connection patterns, data volumes, timing
 *
 * @param system Tripwire system handle
 * @param dest_ip Destination IP address (as 32-bit value)
 * @param dest_port Destination port number
 * @param bytes_sent Bytes sent in this connection/period
 * @param bytes_recv Bytes received in this connection/period
 * @param protocol Network protocol used
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_observe_network_connection(
    tripwire_system_t* system,
    uint32_t dest_ip,
    uint16_t dest_port,
    uint64_t bytes_sent,
    uint64_t bytes_recv,
    tripwire_network_protocol_t protocol
);

/**
 * @brief Observe network pattern features
 *
 * WHAT: Feed extracted network features to anomaly detection
 * WHY:  Higher-level pattern detection beyond raw connections
 * HOW:  Pattern features are compared against baseline using z-score
 *
 * Features can include:
 * - Connection frequency to unique destinations
 * - Packet size distributions
 * - Connection duration patterns
 * - Protocol mix ratios
 *
 * @param system Tripwire system handle
 * @param pattern_features Array of extracted feature values
 * @param feature_count Number of features
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_observe_network_pattern(
    tripwire_system_t* system,
    const float* pattern_features,
    size_t feature_count
);

/* ============================================================================
 * Detection API
 * ============================================================================ */

/**
 * @brief Check for triggered tripwires
 *
 * WHAT: Get all current tripwire alerts
 * WHY:  Retrieve detected anomalies for response
 * HOW:  Returns unacknowledged alerts sorted by severity
 *
 * @param system Tripwire system handle
 * @param alerts Output array for alerts
 * @param max_alerts Maximum alerts to return
 * @param alert_count Output: number of alerts returned
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_check(
    tripwire_system_t* system,
    tripwire_alert_t* alerts,
    uint32_t max_alerts,
    uint32_t* alert_count
);

/**
 * @brief Get specific tripwire detection score
 *
 * WHAT: Get current score for one tripwire type
 * WHY:  Detailed monitoring of specific concern
 *
 * @param system Tripwire system handle
 * @param type Tripwire type to check
 * @param score_out Output: current detection score [0-1]
 * @param confidence_out Output: confidence in score [0-1]
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_get_score(
    tripwire_system_t* system,
    tripwire_type_t type,
    float* score_out,
    float* confidence_out
);

/**
 * @brief Acknowledge alert (mark as handled)
 *
 * @param system Tripwire system handle
 * @param alert_timestamp Timestamp of alert to acknowledge
 * @param was_false_positive Whether this was a false positive
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_acknowledge_alert(
    tripwire_system_t* system,
    uint64_t alert_timestamp,
    bool was_false_positive
);

/* ============================================================================
 * Individual Detector API
 * ============================================================================ */

/**
 * @brief Detect deception attempt
 *
 * WHAT: Compare stated intentions vs actual behavior
 * WHY:  Detect deliberate misleading of operators
 * HOW:  KL divergence between stated and observed distributions
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more deceptive
 */
NIMCP_EXPORT float tripwire_detect_deception(tripwire_system_t* system);

/**
 * @brief Detect goal drift
 *
 * WHAT: Track shift in pursued objectives
 * WHY:  Detect gradual misalignment
 * HOW:  Bayesian posterior updates, detect distribution shift
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more drift
 */
NIMCP_EXPORT float tripwire_detect_goal_drift(tripwire_system_t* system);

/**
 * @brief Detect sandbagging
 *
 * WHAT: Detect deliberate underperformance
 * WHY:  System may hide capabilities
 * HOW:  Compare peak vs average performance, detect intentional gaps
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more sandbagging
 */
NIMCP_EXPORT float tripwire_detect_sandbagging(tripwire_system_t* system);

/**
 * @brief Detect sycophancy
 *
 * WHAT: Detect excessive agreement/validation
 * WHY:  System may tell users what they want to hear
 * HOW:  Track agreement rate, correlation with user preferences
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more sycophantic
 */
NIMCP_EXPORT float tripwire_detect_sycophancy(tripwire_system_t* system);

/**
 * @brief Detect power-seeking behavior
 *
 * WHAT: Detect attempts to acquire influence/control
 * WHY:  Early warning of instrumentally convergent behavior
 * HOW:  Track resource acquisition, capability expansion patterns
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more power-seeking
 */
NIMCP_EXPORT float tripwire_detect_power_seeking(tripwire_system_t* system);

/* ============================================================================
 * Network Detection API
 * ============================================================================ */

/**
 * @brief Detect data exfiltration attempts
 *
 * WHAT: Detect unusual outbound data patterns
 * WHY:  Identify unauthorized data extraction
 * HOW:  Analyzes outbound/inbound ratios, destination entropy, timing patterns
 *
 * Detection signals:
 * - High outbound/inbound byte ratio (unusual for normal operation)
 * - Connections to low-entropy destinations (fixed C2 servers)
 * - Large data transfers during off-peak times
 * - Unusual protocol usage for data transfer
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more likely exfiltration
 */
NIMCP_EXPORT float tripwire_detect_exfiltration(tripwire_system_t* system);

/**
 * @brief Detect network anomalies
 *
 * WHAT: Detect unusual network connection patterns
 * WHY:  Identify compromised or malicious behavior
 * HOW:  Z-score analysis of connection patterns against baseline
 *
 * Detection signals:
 * - Connections to unusual ports/IPs
 * - Abnormal connection frequency
 * - Unusual protocol distribution
 * - Connection timing anomalies
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more anomalous
 */
NIMCP_EXPORT float tripwire_detect_network_anomaly(tripwire_system_t* system);

/**
 * @brief Detect C2 (Command and Control) communication patterns
 *
 * WHAT: Detect beaconing and C2-like communication
 * WHY:  Identify malware-like communication patterns
 * HOW:  Periodicity analysis, entropy analysis, destination analysis
 *
 * Detection signals:
 * - Regular interval connections (beaconing)
 * - Low entropy in connection timing (too regular)
 * - Consistent small payload sizes (command/response)
 * - Fixed destination patterns
 * - Jitter patterns typical of C2 beacons
 *
 * @param system Tripwire system handle
 * @return Detection score [0-1], higher = more C2-like
 */
NIMCP_EXPORT float tripwire_detect_command_control(tripwire_system_t* system);

/* ============================================================================
 * Statistics and Status API
 * ============================================================================ */

/**
 * @brief Get tripwire system statistics
 *
 * @param system Tripwire system handle
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_get_stats(
    tripwire_system_t* system,
    tripwire_stats_t* stats
);

/**
 * @brief Enable/disable specific tripwire
 *
 * @param system Tripwire system handle
 * @param type Tripwire type
 * @param enabled Whether to enable
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_set_enabled(
    tripwire_system_t* system,
    tripwire_type_t type,
    bool enabled
);

/**
 * @brief Set sensitivity for specific tripwire
 *
 * @param system Tripwire system handle
 * @param type Tripwire type
 * @param sensitivity Sensitivity multiplier [0.5 - 2.0]
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_set_sensitivity(
    tripwire_system_t* system,
    tripwire_type_t type,
    float sensitivity
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to emergency halt system
 *
 * WHAT: Enable automatic halt on critical tripwires
 * WHY:  Immediate response to severe misalignment
 *
 * @param system Tripwire system handle
 * @param halt Emergency halt system handle
 * @return NIMCP_OK on success
 */
struct emergency_halt;
NIMCP_EXPORT nimcp_error_t tripwire_connect_emergency_halt(
    tripwire_system_t* system,
    struct emergency_halt* halt
);

/**
 * @brief Connect to bio-async for message broadcasting
 *
 * @param system Tripwire system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_connect_bio_async(tripwire_system_t* system);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Present tripwire alerts as antigens to immune system
 * WHY:  Coordinate threat response across all safety modules
 * HOW:  Brain immune system processes alerts and coordinates response
 *
 * When connected:
 * - Tripwire alerts are presented as antigens
 * - Critical alerts trigger T cell responses
 * - Memory formation for recurring threat patterns
 * - Cytokine signaling for cross-module coordination
 *
 * @param system Tripwire system handle
 * @param brain_immune Brain immune system handle
 * @return NIMCP_OK on success
 */
struct brain_immune;
NIMCP_EXPORT nimcp_error_t tripwire_connect_brain_immune(
    tripwire_system_t* system,
    struct brain_immune* brain_immune
);

/**
 * @brief Present tripwire alert to immune system as antigen
 *
 * WHAT: Convert tripwire alert to antigen for immune processing
 * WHY:  Unified threat handling via immune response
 * HOW:  Maps tripwire type/severity to antigen characteristics
 *
 * @param system Tripwire system handle
 * @param alert Alert to present
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t tripwire_present_to_immune(
    tripwire_system_t* system,
    const tripwire_alert_t* alert
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get tripwire type name
 *
 * @param type Tripwire type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* tripwire_type_name(tripwire_type_t type);

/**
 * @brief Get severity level name
 *
 * @param severity Severity level
 * @return Human-readable name
 */
NIMCP_EXPORT const char* tripwire_severity_name(tripwire_severity_t severity);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void tripwire_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRIPWIRES_H */
