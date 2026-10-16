/**
 * @file nimcp_security_async_fep_bridge.h
 * @brief Free Energy Principle bridge for Security Async Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for asynchronous security operations
 * WHY:  Async anomalies (timing attacks, message floods) are high-surprise events
 * HOW:  Map async security metrics to free energy, use prediction errors for detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ASYNC SECURITY AS TEMPORAL SURPRISE DETECTION:
 * ---------------------------------------------
 * The brain constantly predicts when signals should arrive. Unexpected timing
 * (too fast, too slow, irregular patterns) generates surprise signals that
 * trigger protective responses. This bridge models that temporal prediction:
 *
 * 1. Normal message flow = low free energy (expected timing patterns)
 * 2. Message floods = high free energy (surprising rate increase)
 * 3. Timing anomalies = prediction error (unexpected inter-message intervals)
 * 4. Routing attacks = high surprise (unexpected message destinations)
 * 5. Queue manipulation = belief violation (queue state doesn't match prediction)
 *
 * FEP INTEGRATION:
 * ```
 * Async Event Observation (o) -> Timing/Pattern Assessment
 *         |
 * Expected Async Pattern mu (learned generative model of normal async behavior)
 *         |
 * Prediction Error: epsilon = o - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(o) <= F
 *         |
 * Async Threat Level = F / F_threshold
 * ```
 *
 * SECURITY-FEP MAPPING:
 * - Message rate -> Free energy (deviation from expected rate)
 * - Timing patterns -> Prediction error (unexpected intervals)
 * - Queue depth anomalies -> Surprise level
 * - Routing pattern changes -> Belief divergence
 * - Protective throttling -> Active inference response
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0)  -> Normal async operations
 * - Medium FE (2-5) -> Suspicious (elevated monitoring)
 * - High FE (5-10)  -> Probable attack detected
 * - Very high FE (>10) -> Critical async attack
 *
 * ARCHITECTURE:
 * ```
 * +============================================================================+
 * |        SECURITY ASYNC - FEP BRIDGE (Temporal Anomaly Detection)           |
 * +============================================================================+
 * |                                                                            |
 * |   +------------------+         +------------------+                        |
 * |   |  FEP System      |-------->|  Security        |                        |
 * |   |                  |         |  Async Bridge    |                        |
 * |   | * Free Energy    |         |                  |                        |
 * |   | * Surprise       |         | * Message Rates  |                        |
 * |   | * Precision      |         | * Timing Patterns|                        |
 * |   +------------------+         | * Queue States   |                        |
 * |           |                    +------------------+                        |
 * |   +--------------------------------------------------------------+         |
 * |   |              BIDIRECTIONAL EFFECTS                           |         |
 * |   |                                                              |         |
 * |   |  FEP -> Security:                                            |         |
 * |   |    * Free energy -> Async threat level                       |         |
 * |   |    * Surprise -> Pattern anomaly sensitivity                 |         |
 * |   |    * Precision -> Detection confidence                       |         |
 * |   |                                                              |         |
 * |   |  Security -> FEP:                                            |         |
 * |   |    * Detected floods -> High-surprise observations           |         |
 * |   |    * Normal traffic -> Update generative model               |         |
 * |   |    * False positives -> Reduce precision                     |         |
 * |   +--------------------------------------------------------------+         |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * ACTIVE INFERENCE FOR PROTECTIVE RESPONSES:
 * - High free energy triggers message throttling
 * - Precision modulation adjusts anomaly detection sensitivity
 * - Belief updates adapt to evolving traffic patterns
 * - Rate limiting as action to minimize expected free energy
 *
 * @see nimcp_security_async_bridge.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_ASYNC_FEP_BRIDGE_H
#define NIMCP_SECURITY_ASYNC_FEP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "security/async/nimcp_security_async_bridge.h"
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

/** Free energy thresholds for async security threat levels */
#define SEC_ASYNC_FEP_NORMAL_THRESHOLD      2.0f   /**< Below = normal operations */
#define SEC_ASYNC_FEP_SUSPICIOUS_THRESHOLD  5.0f   /**< Above = suspicious activity */
#define SEC_ASYNC_FEP_ATTACK_THRESHOLD      10.0f  /**< Above = attack detected */
#define SEC_ASYNC_FEP_CRITICAL_THRESHOLD    20.0f  /**< Above = critical threat */

/** Precision bounds for detection sensitivity */
#define SEC_ASYNC_FEP_MIN_PRECISION         0.1f   /**< Minimum precision */
#define SEC_ASYNC_FEP_MAX_PRECISION         10.0f  /**< Maximum precision */
#define SEC_ASYNC_FEP_DEFAULT_PRECISION     1.0f   /**< Default precision */

/** Surprise thresholds */
#define SEC_ASYNC_FEP_SURPRISE_NORMAL       2.0f   /**< Normal surprise level */
#define SEC_ASYNC_FEP_SURPRISE_ANOMALY      8.0f   /**< Anomaly threshold */

/** Prediction error thresholds */
#define SEC_ASYNC_FEP_PE_TOLERANCE          0.2f   /**< Normal prediction error */
#define SEC_ASYNC_FEP_PE_ATTACK             0.7f   /**< Attack threshold */

/** Timing thresholds (milliseconds) */
#define SEC_ASYNC_FEP_MIN_INTERVAL_MS       1      /**< Min expected message interval */
#define SEC_ASYNC_FEP_MAX_INTERVAL_MS       10000  /**< Max expected message interval */
#define SEC_ASYNC_FEP_RATE_WINDOW_MS        1000   /**< Rate calculation window */

/** Bio-async module ID for security-async FEP bridge */
#define BIO_MODULE_SECURITY_ASYNC_FEP       0x0E22

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Async security threat levels based on FEP metrics
 *
 * WHAT: Threat classification from free energy analysis
 * WHY:  Enable graded response to async security events
 */
typedef enum {
    SEC_ASYNC_FEP_THREAT_NONE = 0,       /**< No threat detected */
    SEC_ASYNC_FEP_THREAT_SUSPICIOUS,     /**< Suspicious pattern */
    SEC_ASYNC_FEP_THREAT_MODERATE,       /**< Moderate threat */
    SEC_ASYNC_FEP_THREAT_HIGH,           /**< High threat level */
    SEC_ASYNC_FEP_THREAT_CRITICAL        /**< Critical - immediate response */
} sec_async_fep_threat_level_t;

/**
 * @brief Active inference response types for async security
 *
 * WHAT: Types of protective responses via active inference
 * WHY:  Different threats require different countermeasures
 */
typedef enum {
    SEC_ASYNC_FEP_RESPONSE_NONE = 0,     /**< No response needed */
    SEC_ASYNC_FEP_RESPONSE_MONITOR,      /**< Increase monitoring */
    SEC_ASYNC_FEP_RESPONSE_THROTTLE,     /**< Apply rate throttling */
    SEC_ASYNC_FEP_RESPONSE_ISOLATE,      /**< Isolate suspicious source */
    SEC_ASYNC_FEP_RESPONSE_BLOCK         /**< Block source entirely */
} sec_async_fep_response_t;

/**
 * @brief Async anomaly types detected by FEP
 *
 * WHAT: Classification of async anomaly types
 * WHY:  Different anomaly types require different handling
 */
typedef enum {
    SEC_ASYNC_ANOMALY_NONE = 0,          /**< No anomaly */
    SEC_ASYNC_ANOMALY_TIMING,            /**< Timing pattern anomaly */
    SEC_ASYNC_ANOMALY_FLOOD,             /**< Message flood detected */
    SEC_ASYNC_ANOMALY_QUEUE,             /**< Queue manipulation detected */
    SEC_ASYNC_ANOMALY_ROUTING,           /**< Bio-async routing attack */
    SEC_ASYNC_ANOMALY_PATTERN            /**< General pattern anomaly */
} sec_async_anomaly_type_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Security async FEP configuration
 *
 * WHAT: Configuration for FEP-security async integration
 * WHY:  Control detection sensitivity and response parameters
 * HOW:  Adjustable thresholds, learning rates, and feature flags
 */
typedef struct {
    /* FEP parameters */
    float free_energy_threshold;          /**< FE threshold for threat detection */
    float surprise_threshold;             /**< Surprise threshold */
    float precision_learning_rate;        /**< Precision adaptation rate */

    /* Detection parameters */
    bool use_fep_detection;               /**< Use FEP for threat detection */
    bool enable_precision_modulation;     /**< Adapt precision dynamically */
    float normal_fe_threshold;            /**< FE threshold for normal ops */
    float critical_fe_threshold;          /**< FE threshold for critical */

    /* Async metrics mapping */
    float rate_to_fe_scale;               /**< Scale factor: rate deviation -> FE */
    float timing_pe_weight;               /**< Weight for timing prediction errors */
    float queue_surprise_weight;          /**< Weight for queue anomaly surprises */
    float routing_anomaly_weight;         /**< Weight for routing anomalies */

    /* Timing analysis */
    uint32_t rate_window_ms;              /**< Window for rate calculation */
    float expected_msg_rate;              /**< Expected messages per second */
    float rate_deviation_threshold;       /**< Rate deviation for alarm */

    /* Active inference settings */
    bool enable_active_inference;         /**< Enable protective responses */
    float response_threshold;             /**< FE threshold for response */
    float action_temperature;             /**< Softmax temperature for actions */

    /* Learning */
    bool enable_online_learning;          /**< Update FEP from detections */
    float learning_rate;                  /**< Belief update rate */
    bool learn_from_false_positives;      /**< Reduce precision on FP */
} sec_async_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects on security async (FEP -> Security)
 *
 * WHAT: How FEP analysis affects async security operations
 * WHY:  Free energy provides threat detection signal
 */
typedef struct {
    float free_energy;                    /**< Current free energy */
    float surprise_level;                 /**< Current surprise */
    float prediction_error;               /**< Prediction error magnitude */

    sec_async_fep_threat_level_t threat_level; /**< Derived threat level */
    float threat_confidence;              /**< Threat detection confidence [0-1] */

    float detection_sensitivity;          /**< Precision-based sensitivity */
    float async_health_estimate;          /**< FEP-derived async health [0-1] */

    sec_async_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;               /**< Response urgency [0-1] */

    sec_async_anomaly_type_t detected_anomaly; /**< Most significant anomaly */
} fep_to_async_effects_t;

/**
 * @brief Security async effects on FEP (Security -> FEP)
 *
 * WHAT: How async security detections affect FEP beliefs
 * WHY:  Security events update the generative model
 */
typedef struct {
    uint64_t floods_detected;             /**< Total floods detected */
    uint64_t timing_anomalies;            /**< Timing anomalies detected */
    uint64_t routing_attacks;             /**< Routing attacks detected */
    uint64_t normal_operations;           /**< Normal operations counted */
    uint64_t false_positives;             /**< Known false positives */

    float current_msg_rate;               /**< Current message rate */
    float avg_interval_ms;                /**< Average message interval */
    float queue_utilization;              /**< Queue utilization [0-1] */

    float current_threat_level;           /**< Current async threat [0-1] */
    bool under_attack;                    /**< Active attack in progress */
} async_to_fep_effects_t;

/* ============================================================================
 * Async Pattern Tracking Structures
 * ============================================================================ */

/**
 * @brief Temporal pattern for async messages
 *
 * WHAT: Tracks timing patterns for prediction
 * WHY:  Enables FEP-based temporal anomaly detection
 */
typedef struct {
    uint64_t window_start_ms;             /**< Current window start */
    uint32_t messages_in_window;          /**< Messages in current window */
    float rate_history[16];               /**< Recent rate history */
    uint32_t rate_history_idx;            /**< Current history index */

    float mean_interval_ms;               /**< Mean message interval */
    float variance_interval_ms;           /**< Interval variance */
    uint64_t last_message_time_ms;        /**< Last message timestamp */

    float predicted_rate;                 /**< FEP predicted rate */
    float predicted_interval;             /**< FEP predicted interval */
} async_temporal_pattern_t;

/**
 * @brief Queue state observation for FEP
 *
 * WHAT: Observable queue state for surprise computation
 * WHY:  Queue manipulation causes high surprise
 */
typedef struct {
    uint32_t observed_depth;              /**< Observed queue depth */
    uint32_t predicted_depth;             /**< FEP predicted depth */
    float depth_prediction_error;         /**< Prediction error */

    float fill_rate;                      /**< Queue fill rate */
    float drain_rate;                     /**< Queue drain rate */
    bool anomaly_detected;                /**< Queue anomaly flag */
} async_queue_observation_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief FEP bridge state
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track real-time status for monitoring and debugging
 */
typedef struct {
    bool active;                          /**< Whether bridge is active */
    uint64_t update_count;                /**< Number of updates */
    uint64_t detection_count;             /**< Detections processed */

    float current_precision;              /**< Current precision level */
    float avg_surprise;                   /**< Running average surprise */
    float avg_prediction_error;           /**< Running average PE */

    sec_async_fep_threat_level_t last_threat; /**< Last detected threat */
    uint64_t last_threat_time_ms;         /**< Timestamp of last threat */

    async_temporal_pattern_t temporal;    /**< Temporal pattern state */
    async_queue_observation_t queue_obs;  /**< Queue observation state */
} sec_async_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;               /**< Total updates performed */
    uint64_t fep_detections;              /**< FEP-based detections */
    uint64_t threats_detected;            /**< Threats found */
    uint64_t protective_responses;        /**< Protective actions taken */
    uint64_t precision_adaptations;       /**< Precision updates */

    /* By anomaly type */
    uint64_t timing_anomalies;            /**< Timing anomalies detected */
    uint64_t flood_detections;            /**< Flood attacks detected */
    uint64_t queue_anomalies;             /**< Queue anomalies detected */
    uint64_t routing_anomalies;           /**< Routing anomalies detected */

    float avg_free_energy;                /**< Average free energy */
    float avg_surprise;                   /**< Average surprise */
    float avg_prediction_error;           /**< Average prediction error */
    float current_precision;              /**< Current precision */

    float max_free_energy;                /**< Maximum FE observed */
    float max_surprise;                   /**< Maximum surprise observed */
    float max_msg_rate;                   /**< Maximum message rate observed */

    uint64_t false_positive_count;        /**< False positives */
    uint64_t true_positive_count;         /**< True positives */
    float detection_accuracy;             /**< Detection accuracy rate */
} sec_async_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Security Async FEP Bridge
 *
 * WHAT: Main bridge structure connecting security async to FEP
 * WHY:  Centralized integration of async security with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                   /**< MUST be first: base bridge infrastructure */

    sec_async_fep_config_t config;        /**< Configuration */

    /* System connections */
    security_async_bridge_t* security_async; /**< Connected security async bridge */
    fep_system_t* fep_system;             /**< Connected FEP system */

    /* Bidirectional effects */
    fep_to_async_effects_t fep_effects;       /**< FEP -> Async effects */
    async_to_fep_effects_t async_effects;     /**< Async -> FEP effects */

    /* State and statistics */
    sec_async_fep_state_t state;          /**< Current state */
    sec_async_fep_stats_t stats;          /**< Statistics */
} sec_async_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for security-FEP async integration
 * WHY:  Simplify initialization with biologically-plausible settings
 * HOW:  Return balanced defaults for detection and learning
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sec_async_fep_default_config(sec_async_fep_config_t* config);

/**
 * @brief Create security async FEP bridge
 *
 * WHAT: Initialize FEP integration for async security
 * WHY:  Enable surprise-based threat detection for async patterns
 * HOW:  Connect FEP system to security async, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param security_async Security async bridge handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
sec_async_fep_bridge_t* sec_async_fep_create(
    const sec_async_fep_config_t* config,
    security_async_bridge_t* security_async,
    fep_system_t* fep_system
);

/**
 * @brief Destroy security async FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void sec_async_fep_destroy(sec_async_fep_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clear state while preserving connections
 * WHY:  Fresh start without reconnection
 * HOW:  Reset effects, state, and statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_async_fep_reset(sec_async_fep_bridge_t* bridge);

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
int sec_async_fep_get_config(
    const sec_async_fep_bridge_t* bridge,
    sec_async_fep_config_t* config
);

/**
 * @brief Set configuration
 *
 * WHAT: Update bridge configuration
 * WHY:  Allow runtime tuning of parameters
 * HOW:  Copy new config, validate bounds
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int sec_async_fep_set_config(
    sec_async_fep_bridge_t* bridge,
    const sec_async_fep_config_t* config
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Compute FEP effects on async security
 *
 * WHAT: Compute FEP-derived security metrics
 * WHY:  Use free energy for threat detection
 * HOW:  Process current FEP state, compute effects
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_async_fep_compute_effects(sec_async_fep_bridge_t* bridge);

/**
 * @brief Update from async message event
 *
 * WHAT: Process async message for FEP analysis
 * WHY:  Each message updates temporal predictions
 * HOW:  Update timing patterns, compute prediction errors
 *
 * @param bridge Bridge handle
 * @param source_module Source module ID
 * @param msg_type Message type
 * @param timestamp_ms Message timestamp
 * @return 0 on success, -1 on error
 */
int sec_async_fep_process_message(
    sec_async_fep_bridge_t* bridge,
    uint32_t source_module,
    uint32_t msg_type,
    uint64_t timestamp_ms
);

/**
 * @brief Update from queue state observation
 *
 * WHAT: Process queue state for FEP analysis
 * WHY:  Queue anomalies indicate potential attacks
 * HOW:  Compare observed vs predicted queue state
 *
 * @param bridge Bridge handle
 * @param queue_depth Current queue depth
 * @param fill_rate Current fill rate
 * @param drain_rate Current drain rate
 * @return 0 on success, -1 on error
 */
int sec_async_fep_observe_queue(
    sec_async_fep_bridge_t* bridge,
    uint32_t queue_depth,
    float fill_rate,
    float drain_rate
);

/**
 * @brief Update full bridge state
 *
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional synchronization
 * HOW:  Compute effects in both directions, update state
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 */
int sec_async_fep_update(sec_async_fep_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * Detection API
 * ============================================================================ */

/**
 * @brief Detect threat using FEP analysis
 *
 * WHAT: Analyze async state using FEP for threat detection
 * WHY:  Combine async metrics with FEP for enhanced detection
 * HOW:  Compute free energy, surprise, determine threat level
 *
 * @param bridge Bridge handle
 * @param msg_rate Current message rate
 * @param avg_interval Average message interval
 * @param queue_utilization Queue utilization [0-1]
 * @param threat_level_out Output threat level
 * @param confidence_out Output detection confidence [0-1]
 * @return 0 on success, -1 on error
 */
int sec_async_fep_detect_threat(
    sec_async_fep_bridge_t* bridge,
    float msg_rate,
    float avg_interval,
    float queue_utilization,
    sec_async_fep_threat_level_t* threat_level_out,
    float* confidence_out
);

/**
 * @brief Detect specific anomaly type
 *
 * WHAT: Identify the type of async anomaly
 * WHY:  Different anomalies require different responses
 * HOW:  Analyze temporal patterns and queue states
 *
 * @param bridge Bridge handle
 * @param anomaly_out Output anomaly type
 * @param severity_out Output severity [0-1]
 * @return 0 on success, -1 on error
 */
int sec_async_fep_detect_anomaly(
    sec_async_fep_bridge_t* bridge,
    sec_async_anomaly_type_t* anomaly_out,
    float* severity_out
);

/**
 * @brief Get recommended protective response
 *
 * WHAT: Determine appropriate response via active inference
 * WHY:  Actions minimize expected free energy
 * HOW:  Evaluate response policies, select optimal action
 *
 * @param bridge Bridge handle
 * @param response_out Output recommended response
 * @param urgency_out Output urgency level [0-1]
 * @return 0 on success, -1 on error
 */
int sec_async_fep_get_response(
    sec_async_fep_bridge_t* bridge,
    sec_async_fep_response_t* response_out,
    float* urgency_out
);

/**
 * @brief Report false positive detection
 *
 * WHAT: Update FEP on known false positive
 * WHY:  Reduce precision to prevent similar FPs
 * HOW:  Lower precision for observation type
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_async_fep_report_false_positive(sec_async_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get FEP effects on async security
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_async_fep_get_fep_effects(
    const sec_async_fep_bridge_t* bridge,
    fep_to_async_effects_t* effects
);

/**
 * @brief Get async security effects on FEP
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int sec_async_fep_get_async_effects(
    const sec_async_fep_bridge_t* bridge,
    async_to_fep_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
int sec_async_fep_get_state(
    const sec_async_fep_bridge_t* bridge,
    sec_async_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int sec_async_fep_get_stats(
    const sec_async_fep_bridge_t* bridge,
    sec_async_fep_stats_t* stats
);

/**
 * @brief Get current free energy
 *
 * @param bridge Bridge handle
 * @return Current free energy or -1.0f on error
 */
float sec_async_fep_get_free_energy(const sec_async_fep_bridge_t* bridge);

/**
 * @brief Get current surprise level
 *
 * @param bridge Bridge handle
 * @return Current surprise or -1.0f on error
 */
float sec_async_fep_get_surprise(const sec_async_fep_bridge_t* bridge);

/**
 * @brief Get current threat level
 *
 * @param bridge Bridge handle
 * @return Current threat level
 */
sec_async_fep_threat_level_t sec_async_fep_get_threat_level(
    const sec_async_fep_bridge_t* bridge
);

/**
 * @brief Get current message rate
 *
 * @param bridge Bridge handle
 * @return Messages per second or -1.0f on error
 */
float sec_async_fep_get_message_rate(const sec_async_fep_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module security notifications
 * HOW:  Register module, setup inbox, use bio_router_process_inbox for messages
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_async_fep_connect_bio_async(sec_async_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int sec_async_fep_disconnect_bio_async(sec_async_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool sec_async_fep_is_bio_async_connected(const sec_async_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages from bio-async router inbox
 * WHY:  Handle incoming security notifications
 * HOW:  Uses bio_router_process_inbox for message handling
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t sec_async_fep_process_bio_messages(
    sec_async_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get threat level name
 *
 * @param level Threat level
 * @return Human-readable name
 */
const char* sec_async_fep_threat_level_name(sec_async_fep_threat_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* sec_async_fep_response_name(sec_async_fep_response_t response);

/**
 * @brief Get anomaly type name
 *
 * @param anomaly Anomaly type
 * @return Human-readable name
 */
const char* sec_async_fep_anomaly_name(sec_async_anomaly_type_t anomaly);

/**
 * @brief Print bridge summary
 *
 * WHAT: Print current bridge state (debug)
 * WHY:  Facilitate debugging and monitoring
 * HOW:  Format and print key metrics
 *
 * @param bridge Bridge handle
 */
void sec_async_fep_print_summary(const sec_async_fep_bridge_t* bridge);

/**
 * @brief Print statistics
 *
 * @param stats Statistics to print
 */
void sec_async_fep_print_stats(const sec_async_fep_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_ASYNC_FEP_BRIDGE_H */
