/**
 * @file nimcp_hypo_bio_async_fep_bridge.h
 * @brief Free Energy Principle bridge for Hypothalamus Bio-Async Integration
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration for hypothalamic bio-async message processing
 * WHY:  Message timing anomalies and rate deviations represent high-surprise events
 *       in the predictive processing framework of homeostatic regulation
 * HOW:  Map message timing to prediction errors, message rate to free energy,
 *       and use precision modulation based on drive urgency
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * HYPOTHALAMIC SIGNALING AS TEMPORAL PREDICTION:
 * -----------------------------------------------
 * The hypothalamus maintains homeostasis through precisely timed neural signals.
 * Normal signaling patterns have predictable timing - deviations generate surprise:
 *
 * 1. Normal message flow = low free energy (expected timing patterns)
 * 2. Unexpected burst activity = high free energy (drive urgency spike)
 * 3. Missing signals = prediction error (expected update didn't arrive)
 * 4. Timing drift = belief violation (circadian/ultradian rhythm disruption)
 *
 * FEP INTEGRATION:
 * ```
 * Bio-Async Message (m) -> Timing/Rate Assessment
 *         |
 * Expected Message Pattern mu (learned from homeostatic rhythm)
 *         |
 * Prediction Error: epsilon = m - g(mu)
 *         |
 * Free Energy F = Complexity + Inaccuracy
 *         |
 * Surprise = -ln p(m) <= F
 *         |
 * Homeostatic Disruption = F / F_threshold
 * ```
 *
 * FEP MAPPINGS:
 * - Message timing -> Prediction error (unexpected intervals)
 * - Message rate -> Free energy (deviation from expected rate)
 * - Queue congestion -> Surprise level
 * - Pattern changes -> Belief divergence
 *
 * DETECTION MAPPING:
 * - Low FE (<2.0)  -> Normal homeostatic signaling
 * - Medium FE (2-5) -> Mild disruption (monitoring)
 * - High FE (5-10)  -> Significant disruption (compensatory response)
 * - Very high FE (>10) -> Critical homeostatic threat
 *
 * ARCHITECTURE:
 * ```
 * +============================================================================+
 * |        HYPOTHALAMUS BIO-ASYNC - FEP BRIDGE                                 |
 * +============================================================================+
 * |                                                                            |
 * |   +------------------+         +------------------+                        |
 * |   |  FEP System      |<------->|  Bio-Async       |                        |
 * |   |                  |         |  Router          |                        |
 * |   | - Free Energy    |         |                  |                        |
 * |   | - Surprise       |         | - Message Rates  |                        |
 * |   | - Precision      |         | - Timing Patterns|                        |
 * |   +------------------+         | - Queue States   |                        |
 * |           |                    +------------------+                        |
 * |           |                            |                                   |
 * |           v                            v                                   |
 * |   +------------------+         +------------------+                        |
 * |   |  Drive State     |         |  Homeostasis     |                        |
 * |   |                  |         |  Monitor         |                        |
 * |   | - Urgency        |         |                  |                        |
 * |   | - Fatigue        |         |                  |                        |
 * |   +------------------+         +------------------+                        |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * @see nimcp_hypothalamus_drives.h
 * @see nimcp_bio_async.h
 * @see nimcp_free_energy.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPO_BIO_ASYNC_FEP_BRIDGE_H
#define NIMCP_HYPO_BIO_ASYNC_FEP_BRIDGE_H

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

/** Bio-async module ID for hypothalamus bio-async FEP bridge */
#define BIO_MODULE_HYPO_BIO_ASYNC_FEP   0x0B01

/** Free energy thresholds for homeostatic disruption levels */
#define HYPO_BIO_ASYNC_FEP_NORMAL_THRESHOLD      2.0f   /**< Normal signaling */
#define HYPO_BIO_ASYNC_FEP_MILD_THRESHOLD        5.0f   /**< Mild disruption */
#define HYPO_BIO_ASYNC_FEP_SIGNIFICANT_THRESHOLD 10.0f  /**< Significant disruption */
#define HYPO_BIO_ASYNC_FEP_CRITICAL_THRESHOLD    20.0f  /**< Critical threat */

/** Precision bounds for timing sensitivity */
#define HYPO_BIO_ASYNC_FEP_MIN_PRECISION         0.1f   /**< Minimum precision */
#define HYPO_BIO_ASYNC_FEP_MAX_PRECISION         10.0f  /**< Maximum precision */
#define HYPO_BIO_ASYNC_FEP_DEFAULT_PRECISION     1.0f   /**< Default precision */

/** Surprise thresholds */
#define HYPO_BIO_ASYNC_FEP_SURPRISE_NORMAL       2.0f   /**< Normal surprise level */
#define HYPO_BIO_ASYNC_FEP_SURPRISE_ANOMALY      8.0f   /**< Anomaly threshold */

/** Timing thresholds (milliseconds) */
#define HYPO_BIO_ASYNC_FEP_RATE_WINDOW_MS        1000   /**< Rate calculation window */
#define HYPO_BIO_ASYNC_FEP_MAX_INTERVAL_MS       5000   /**< Max expected interval */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Homeostatic disruption levels based on FEP metrics
 *
 * WHAT: Categorization of bio-async disruption severity
 * WHY:  Enable graded response to timing anomalies
 */
typedef enum {
    HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL = 0,      /**< Normal operation */
    HYPO_BIO_ASYNC_FEP_LEVEL_MILD,             /**< Mild disruption */
    HYPO_BIO_ASYNC_FEP_LEVEL_MODERATE,         /**< Moderate disruption */
    HYPO_BIO_ASYNC_FEP_LEVEL_SIGNIFICANT,      /**< Significant disruption */
    HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL          /**< Critical - immediate response */
} hypo_bio_async_fep_level_t;

/**
 * @brief Active inference response types for bio-async
 *
 * WHAT: Types of compensatory responses via active inference
 * WHY:  Different disruption types require different countermeasures
 */
typedef enum {
    HYPO_BIO_ASYNC_FEP_RESPONSE_NONE = 0,     /**< No response needed */
    HYPO_BIO_ASYNC_FEP_RESPONSE_MONITOR,      /**< Increase monitoring */
    HYPO_BIO_ASYNC_FEP_RESPONSE_THROTTLE,     /**< Apply message throttling */
    HYPO_BIO_ASYNC_FEP_RESPONSE_PRIORITIZE,   /**< Prioritize critical messages */
    HYPO_BIO_ASYNC_FEP_RESPONSE_EMERGENCY     /**< Emergency homeostatic response */
} hypo_bio_async_fep_response_t;

/**
 * @brief Anomaly types detected by FEP
 *
 * WHAT: Classification of bio-async anomaly types
 * WHY:  Different anomalies require different handling
 */
typedef enum {
    HYPO_BIO_ASYNC_ANOMALY_NONE = 0,          /**< No anomaly */
    HYPO_BIO_ASYNC_ANOMALY_TIMING,            /**< Timing pattern anomaly */
    HYPO_BIO_ASYNC_ANOMALY_RATE,              /**< Message rate anomaly */
    HYPO_BIO_ASYNC_ANOMALY_QUEUE,             /**< Queue congestion */
    HYPO_BIO_ASYNC_ANOMALY_PATTERN            /**< General pattern anomaly */
} hypo_bio_async_anomaly_type_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus bio-async FEP configuration
 *
 * WHAT: Configuration for FEP-bio-async integration
 * WHY:  Control detection sensitivity and response parameters
 * HOW:  Adjustable thresholds, learning rates, and feature flags
 */
typedef struct {
    /* FEP parameters */
    float drive_fe_weight;                    /**< Weight of drives in free energy */
    float prediction_error_gain;              /**< PE gain from timing deviation */
    float precision_modulation;               /**< Precision based on fatigue */
    bool enable_active_inference;             /**< Allow timing-based actions */
    bool enable_bio_async;                    /**< Bio-async integration enabled */

    /* Detection parameters */
    float free_energy_threshold;              /**< FE threshold for detection */
    float surprise_threshold;                 /**< Surprise threshold */
    float precision_learning_rate;            /**< Precision adaptation rate */

    /* Timing analysis */
    uint32_t rate_window_ms;                  /**< Window for rate calculation */
    float expected_msg_rate;                  /**< Expected messages per second */
    float rate_deviation_threshold;           /**< Rate deviation for alarm */

    /* Drive integration */
    bool modulate_by_drive_urgency;           /**< Adjust precision by urgency */
    float urgency_precision_scale;            /**< Urgency to precision factor */

    /* Learning */
    bool enable_online_learning;              /**< Update FEP from events */
    float learning_rate;                      /**< Belief update rate */
} hypo_bio_async_fep_config_t;

/* ============================================================================
 * Effects Structures
 * ============================================================================ */

/**
 * @brief FEP effects output
 *
 * WHAT: How FEP analysis affects bio-async operations
 * WHY:  Free energy provides homeostatic disruption signal
 */
typedef struct {
    float free_energy;                        /**< Current FE from message timing */
    float prediction_error;                   /**< PE from timing deviation */
    float precision;                          /**< Current precision */
    float active_inference_strength;          /**< Action strength */

    hypo_bio_async_fep_level_t disruption_level;   /**< Disruption classification */
    float disruption_confidence;              /**< Detection confidence [0-1] */

    hypo_bio_async_fep_response_t recommended_response; /**< Recommended action */
    float response_urgency;                   /**< Response urgency [0-1] */

    hypo_bio_async_anomaly_type_t detected_anomaly; /**< Most significant anomaly */
    float homeostatic_health;                 /**< Homeostatic health estimate [0-1] */
} hypo_bio_async_fep_effects_t;

/**
 * @brief Bio-async effects on FEP
 *
 * WHAT: How bio-async events affect FEP beliefs
 * WHY:  Message patterns update the generative model
 */
typedef struct {
    uint64_t messages_processed;              /**< Total messages processed */
    uint64_t timing_anomalies;                /**< Timing anomalies detected */
    uint64_t rate_anomalies;                  /**< Rate anomalies detected */
    uint64_t queue_congestions;               /**< Queue congestion events */

    float current_msg_rate;                   /**< Current message rate */
    float avg_interval_ms;                    /**< Average message interval */
    float queue_utilization;                  /**< Queue utilization [0-1] */

    float current_drive_urgency;              /**< Current dominant drive urgency */
    bool homeostatic_stress;                  /**< Homeostatic stress active */
} bio_async_to_fep_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Temporal pattern tracking
 *
 * WHAT: Tracks timing patterns for prediction
 * WHY:  Enables FEP-based temporal anomaly detection
 */
typedef struct {
    uint64_t window_start_ms;                 /**< Current window start */
    uint32_t messages_in_window;              /**< Messages in current window */
    float rate_history[16];                   /**< Recent rate history */
    uint32_t rate_history_idx;                /**< Current history index */

    float mean_interval_ms;                   /**< Mean message interval */
    float variance_interval_ms;               /**< Interval variance */
    uint64_t last_message_time_ms;            /**< Last message timestamp */

    float predicted_rate;                     /**< FEP predicted rate */
    float predicted_interval;                 /**< FEP predicted interval */
} hypo_bio_async_temporal_pattern_t;

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

    hypo_bio_async_fep_level_t last_level;    /**< Last disruption level */
    uint64_t last_detection_time_ms;          /**< Timestamp of last detection */

    hypo_bio_async_temporal_pattern_t temporal; /**< Temporal pattern state */
} hypo_bio_async_fep_state_t;

/**
 * @brief FEP bridge statistics
 *
 * WHAT: Cumulative statistics for the bridge
 * WHY:  Performance monitoring and tuning
 */
typedef struct {
    uint64_t total_updates;                   /**< Total updates performed */
    uint64_t fep_detections;                  /**< FEP-based detections */
    uint64_t disruptions_detected;            /**< Disruptions found */
    uint64_t compensatory_responses;          /**< Responses triggered */
    uint64_t precision_adaptations;           /**< Precision updates */

    /* By anomaly type */
    uint64_t timing_anomalies;                /**< Timing anomalies detected */
    uint64_t rate_anomalies;                  /**< Rate anomalies detected */
    uint64_t queue_anomalies;                 /**< Queue anomalies detected */

    float avg_free_energy;                    /**< Average free energy */
    float avg_surprise;                       /**< Average surprise */
    float avg_prediction_error;               /**< Average prediction error */
    float current_precision;                  /**< Current precision */

    float max_free_energy;                    /**< Maximum FE observed */
    float max_surprise;                       /**< Maximum surprise observed */
} hypo_bio_async_fep_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Hypothalamus Bio-Async FEP Bridge
 *
 * WHAT: Main bridge connecting hypothalamus bio-async to FEP
 * WHY:  Centralized integration of bio-async timing with free energy principle
 * HOW:  Contains configuration, connections, effects, and state
 */
typedef struct {
    bridge_base_t base;                       /**< MUST be first: base infrastructure */

    hypo_bio_async_fep_config_t config;       /**< Configuration */

    /* System connections */
    hypo_drive_system_handle_t* drive_system; /**< Connected drive system */
    fep_system_t* fep_system;                 /**< Connected FEP system */

    /* Bidirectional effects */
    hypo_bio_async_fep_effects_t fep_effects;     /**< FEP -> Bio-async effects */
    bio_async_to_fep_effects_t async_effects;     /**< Bio-async -> FEP effects */

    /* State and statistics */
    hypo_bio_async_fep_state_t state;         /**< Current state */
    hypo_bio_async_fep_stats_t stats;         /**< Statistics */
} hypo_bio_async_fep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP bridge configuration
 *
 * WHAT: Provide sensible defaults for bio-async FEP integration
 * WHY:  Simplify initialization with biologically-plausible settings
 * HOW:  Return balanced defaults for detection and learning
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_default_config(hypo_bio_async_fep_config_t* config);

/**
 * @brief Create hypothalamus bio-async FEP bridge
 *
 * WHAT: Initialize FEP integration for bio-async
 * WHY:  Enable surprise-based timing anomaly detection
 * HOW:  Connect FEP system to drive system, allocate structures
 *
 * @param config Configuration (NULL for defaults)
 * @param drive_system Drive system handle
 * @param fep_system FEP system handle
 * @return Bridge handle or NULL on failure
 */
hypo_bio_async_fep_bridge_t* hypo_bio_async_fep_create(
    const hypo_bio_async_fep_config_t* config,
    hypo_drive_system_handle_t* drive_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy hypothalamus bio-async FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Prevent memory leaks
 * HOW:  Free allocations, disconnect bio-async
 *
 * @param bridge Bridge handle (NULL safe)
 */
void hypo_bio_async_fep_destroy(hypo_bio_async_fep_bridge_t* bridge);

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
int hypo_bio_async_fep_reset(hypo_bio_async_fep_bridge_t* bridge);

/**
 * @brief Update bridge state
 *
 * WHAT: Main update loop for bridge synchronization
 * WHY:  Maintain bidirectional synchronization
 * HOW:  Compute effects in both directions, update state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_update(hypo_bio_async_fep_bridge_t* bridge);

/* ============================================================================
 * Core Operations API
 * ============================================================================ */

/**
 * @brief Compute free energy from drive state
 *
 * WHAT: Calculate FE from current drive state and message timing
 * WHY:  Core FEP computation for homeostatic monitoring
 * HOW:  Map drive deviations and timing patterns to free energy
 *
 * @param bridge Bridge handle
 * @param drives Drive state input
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_compute_fe(
    hypo_bio_async_fep_bridge_t* bridge,
    const hypo_drive_system_t* drives
);

/**
 * @brief Modulate precision based on fatigue
 *
 * WHAT: Adjust detection precision based on fatigue level
 * WHY:  Precision represents confidence; fatigue reduces confidence
 * HOW:  Scale precision inversely with fatigue
 *
 * @param bridge Bridge handle
 * @param fatigue Fatigue level [0-1]
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_modulate_precision(
    hypo_bio_async_fep_bridge_t* bridge,
    float fatigue
);

/**
 * @brief Get current FEP effects
 *
 * @param bridge Bridge handle
 * @param effects Output effects
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_get_effects(
    const hypo_bio_async_fep_bridge_t* bridge,
    hypo_bio_async_fep_effects_t* effects
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_get_stats(
    const hypo_bio_async_fep_bridge_t* bridge,
    hypo_bio_async_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async system
 * WHY:  Enable inter-module homeostatic notifications
 * HOW:  Register module, setup inbox
 *
 * @param bridge Bridge handle
 * @param router Bio-router handle
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_connect_bio_async(
    hypo_bio_async_fep_bridge_t* bridge,
    bio_router_t* router
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_bio_async_fep_disconnect_bio_async(hypo_bio_async_fep_bridge_t* bridge);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages from bio-async router inbox
 * WHY:  Handle incoming homeostatic notifications
 * HOW:  Uses bio_router_process_inbox for message handling
 *
 * @param bridge Bridge handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
int hypo_bio_async_fep_process_messages(
    hypo_bio_async_fep_bridge_t* bridge,
    uint32_t max_messages
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get disruption level name
 *
 * @param level Disruption level
 * @return Human-readable name
 */
const char* hypo_bio_async_fep_level_name(hypo_bio_async_fep_level_t level);

/**
 * @brief Get response type name
 *
 * @param response Response type
 * @return Human-readable name
 */
const char* hypo_bio_async_fep_response_name(hypo_bio_async_fep_response_t response);

/**
 * @brief Get anomaly type name
 *
 * @param anomaly Anomaly type
 * @return Human-readable name
 */
const char* hypo_bio_async_fep_anomaly_name(hypo_bio_async_anomaly_type_t anomaly);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle
 */
void hypo_bio_async_fep_print_summary(const hypo_bio_async_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPO_BIO_ASYNC_FEP_BRIDGE_H */
