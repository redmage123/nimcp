/**
 * @file nimcp_plasticity_anomaly_detection.h
 * @brief Plasticity Anomaly Detection Patterns for Health Monitor
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: Anomaly detection rules and patterns specific to plasticity systems
 * WHY:  Enable early detection of learning instabilities and plasticity failures
 * HOW:  Define detection rules, thresholds, and response strategies
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                Plasticity Anomaly Detection System                         |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |   Health Monitor      |       |   Plasticity Systems  |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Anomaly Framework     |       | STDP, BCM, Homeostatic|                |
 * |  | Threshold Engine      |       | Eligibility Traces    |                |
 * |  | Alert System          |       | Reward Modulation     |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |         Plasticity Anomaly Detection Rules               |            |
 * |  +----------------------------------------------------------+            |
 * |  | Rule Categories:                                         |            |
 * |  | - Weight dynamics rules (divergence, saturation, etc.)   |            |
 * |  | - Timing rules (STDP window violations)                  |            |
 * |  | - BCM rules (threshold drift, runaway)                   |            |
 * |  | - Homeostatic rules (activity imbalance)                 |            |
 * |  | - Learning stability rules (convergence, oscillation)    |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * DETECTION APPROACH:
 * 1. Statistical baseline establishment
 * 2. Continuous metric collection
 * 3. Rule-based threshold checking
 * 4. Trend analysis for early warning
 * 5. Correlation analysis across metrics
 *
 * @see nimcp_health_monitor.h
 * @see nimcp_stdp_health_metrics.h
 */

#ifndef NIMCP_PLASTICITY_ANOMALY_DETECTION_H
#define NIMCP_PLASTICITY_ANOMALY_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Health monitor - must match health_monitor.h */
#ifndef NIMCP_HEALTH_MONITOR_H
struct health_monitor_internal;
typedef struct health_monitor_internal* health_monitor_t;
#endif

/* Bio-async router */
#ifndef NIMCP_BIO_ROUTER_H
typedef struct bio_router_struct* bio_router_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PLASTICITY_ANOMALY_VERSION      "1.0.0"
#define PLASTICITY_ANOMALY_MAGIC        0x504C4144  /* 'PLAD' */

/** Bio-async module ID */
#define BIO_MODULE_PLASTICITY_ANOMALY   0x1D11

/** Maximum detection rules */
#define PLASTICITY_MAX_RULES            64

/** Maximum metric history */
#define PLASTICITY_METRIC_HISTORY_SIZE  256

/** Default detection window (ms) */
#define PLASTICITY_DETECTION_WINDOW_MS  1000

/* ============================================================================
 * Plasticity Anomaly Categories
 * ============================================================================ */

/**
 * @brief Plasticity anomaly category (high-level)
 */
typedef enum {
    PLASTICITY_CATEGORY_WEIGHT = 0,      /**< Weight-related anomalies */
    PLASTICITY_CATEGORY_TIMING,          /**< Spike timing anomalies */
    PLASTICITY_CATEGORY_BCM,             /**< BCM rule anomalies */
    PLASTICITY_CATEGORY_HOMEOSTATIC,     /**< Homeostatic scaling anomalies */
    PLASTICITY_CATEGORY_LEARNING,        /**< Learning dynamics anomalies */
    PLASTICITY_CATEGORY_STRUCTURAL,      /**< Structural plasticity anomalies */
    PLASTICITY_CATEGORY_COUNT
} plasticity_anomaly_category_t;

/**
 * @brief Specific plasticity anomaly types
 */
typedef enum {
    /* Weight anomalies (0x200 range) */
    PLASTICITY_ANOMALY_NONE = 0,

    PLASTICITY_ANOMALY_WEIGHT_EXPLOSION = 0x200, /**< Weights growing exponentially */
    PLASTICITY_ANOMALY_WEIGHT_VANISHING,     /**< Weights approaching zero */
    PLASTICITY_ANOMALY_WEIGHT_SATURATION,    /**< Weights stuck at bounds */
    PLASTICITY_ANOMALY_WEIGHT_BIMODAL,       /**< Bimodal weight distribution */
    PLASTICITY_ANOMALY_WEIGHT_GRADIENT_NAN,  /**< NaN in weight gradients */
    PLASTICITY_ANOMALY_WEIGHT_GRADIENT_INF,  /**< Infinity in gradients */

    /* Timing anomalies (0x210 range) */
    PLASTICITY_ANOMALY_STDP_WINDOW_VIOLATION = 0x210, /**< Outside STDP window */
    PLASTICITY_ANOMALY_STDP_ASYMMETRY,       /**< Abnormal pre/post asymmetry */
    PLASTICITY_ANOMALY_STDP_RATE_MISMATCH,   /**< Pre/post firing rate mismatch */
    PLASTICITY_ANOMALY_SPIKE_BURST,          /**< Abnormal spike bursting */

    /* BCM anomalies (0x220 range) */
    PLASTICITY_ANOMALY_BCM_THRESHOLD_HIGH = 0x220, /**< BCM threshold too high */
    PLASTICITY_ANOMALY_BCM_THRESHOLD_LOW,    /**< BCM threshold too low */
    PLASTICITY_ANOMALY_BCM_THRESHOLD_DRIFT,  /**< BCM threshold drifting */
    PLASTICITY_ANOMALY_BCM_INVERSION,        /**< BCM potentiation/depression inverted */

    /* Homeostatic anomalies (0x230 range) */
    PLASTICITY_ANOMALY_ACTIVITY_TOO_HIGH = 0x230, /**< Activity above target */
    PLASTICITY_ANOMALY_ACTIVITY_TOO_LOW,     /**< Activity below target */
    PLASTICITY_ANOMALY_SCALING_FAILURE,      /**< Scaling not converging */
    PLASTICITY_ANOMALY_INTRINSIC_INSTABILITY, /**< Intrinsic plasticity unstable */

    /* Learning dynamics anomalies (0x240 range) */
    PLASTICITY_ANOMALY_LEARNING_STALLED = 0x240, /**< No weight changes */
    PLASTICITY_ANOMALY_LEARNING_OSCILLATING, /**< Weights oscillating */
    PLASTICITY_ANOMALY_LEARNING_DIVERGING,   /**< Loss increasing */
    PLASTICITY_ANOMALY_CONVERGENCE_FAILURE,  /**< Failed to converge */
    PLASTICITY_ANOMALY_LOCAL_MINIMUM,        /**< Stuck in local minimum */

    /* Structural anomalies (0x250 range) */
    PLASTICITY_ANOMALY_SYNAPSE_DEATH = 0x250, /**< Synapses becoming silent */
    PLASTICITY_ANOMALY_SYNAPSE_PROLIFERATION, /**< Too many new synapses */
    PLASTICITY_ANOMALY_CONNECTIVITY_COLLAPSE, /**< Network connectivity lost */
    PLASTICITY_ANOMALY_SPINE_INSTABILITY     /**< Dendritic spine instability */
} plasticity_anomaly_type_t;

/**
 * @brief Anomaly severity levels
 */
typedef enum {
    PLASTICITY_SEVERITY_INFO = 0,        /**< Informational */
    PLASTICITY_SEVERITY_WARNING = 1,     /**< Warning - monitor closely */
    PLASTICITY_SEVERITY_ERROR = 2,       /**< Error - intervention needed */
    PLASTICITY_SEVERITY_CRITICAL = 3     /**< Critical - immediate action */
} plasticity_severity_t;

/**
 * @brief Detection rule types
 */
typedef enum {
    PLASTICITY_RULE_THRESHOLD = 0,       /**< Simple threshold check */
    PLASTICITY_RULE_TREND,               /**< Trend detection */
    PLASTICITY_RULE_RATE_OF_CHANGE,      /**< Rate of change check */
    PLASTICITY_RULE_STATISTICAL,         /**< Statistical deviation */
    PLASTICITY_RULE_CORRELATION,         /**< Cross-metric correlation */
    PLASTICITY_RULE_PATTERN,             /**< Pattern matching */
    PLASTICITY_RULE_COMPOSITE            /**< Multiple conditions */
} plasticity_rule_type_t;

/**
 * @brief Response actions for anomalies
 */
typedef enum {
    PLASTICITY_ACTION_NONE = 0,          /**< No action */
    PLASTICITY_ACTION_LOG,               /**< Log only */
    PLASTICITY_ACTION_ALERT,             /**< Send alert */
    PLASTICITY_ACTION_REDUCE_LR,         /**< Reduce learning rate */
    PLASTICITY_ACTION_PAUSE_LEARNING,    /**< Pause plasticity */
    PLASTICITY_ACTION_RESET_WEIGHTS,     /**< Reset to baseline */
    PLASTICITY_ACTION_QUARANTINE,        /**< Isolate affected region */
    PLASTICITY_ACTION_NOTIFY_HEALTH      /**< Notify health agent */
} plasticity_response_action_t;

/* ============================================================================
 * Detection Rule Structures
 * ============================================================================ */

/**
 * @brief Threshold rule parameters
 */
typedef struct {
    float upper_threshold;               /**< Upper bound */
    float lower_threshold;               /**< Lower bound */
    bool check_upper;                    /**< Check upper bound */
    bool check_lower;                    /**< Check lower bound */
    uint32_t consecutive_violations;     /**< Required consecutive violations */
} threshold_rule_params_t;

/**
 * @brief Trend rule parameters
 */
typedef struct {
    float slope_threshold;               /**< Minimum slope for trend */
    uint32_t window_size;                /**< Samples for trend calculation */
    bool detect_increasing;              /**< Detect increasing trend */
    bool detect_decreasing;              /**< Detect decreasing trend */
} trend_rule_params_t;

/**
 * @brief Rate of change rule parameters
 */
typedef struct {
    float max_rate;                      /**< Maximum allowed rate */
    float min_rate;                      /**< Minimum expected rate */
    uint32_t time_window_ms;             /**< Time window for rate calc */
} rate_rule_params_t;

/**
 * @brief Statistical rule parameters
 */
typedef struct {
    float std_dev_threshold;             /**< Standard deviations for anomaly */
    uint32_t baseline_samples;           /**< Samples for baseline */
    bool use_mad;                        /**< Use median absolute deviation */
} statistical_rule_params_t;

/**
 * @brief Detection rule definition
 */
typedef struct {
    uint32_t rule_id;                    /**< Unique rule ID */
    char name[64];                       /**< Rule name */
    bool enabled;                        /**< Is rule active */

    /* Rule specification */
    plasticity_rule_type_t type;         /**< Rule type */
    plasticity_anomaly_type_t anomaly;   /**< Anomaly this detects */
    plasticity_anomaly_category_t category; /**< Category */
    plasticity_severity_t severity;      /**< Severity if triggered */

    /* Parameters (union based on type) */
    union {
        threshold_rule_params_t threshold;
        trend_rule_params_t trend;
        rate_rule_params_t rate;
        statistical_rule_params_t statistical;
    } params;

    /* Response */
    plasticity_response_action_t action; /**< Action when triggered */
    uint32_t cooldown_ms;                /**< Minimum time between triggers */

    /* State */
    uint64_t last_triggered;             /**< Last trigger time */
    uint64_t trigger_count;              /**< Total triggers */
} plasticity_detection_rule_t;

/* ============================================================================
 * Anomaly Report
 * ============================================================================ */

/**
 * @brief Plasticity anomaly report
 */
typedef struct {
    /* Identification */
    uint32_t report_id;                  /**< Unique report ID */
    plasticity_anomaly_type_t type;      /**< Anomaly type */
    plasticity_anomaly_category_t category; /**< Category */
    plasticity_severity_t severity;      /**< Severity */

    /* Rule that triggered */
    uint32_t rule_id;                    /**< Triggering rule ID */
    char rule_name[64];                  /**< Rule name */

    /* Details */
    char description[256];               /**< Human-readable description */
    float metric_value;                  /**< Current metric value */
    float baseline_value;                /**< Expected baseline */
    float deviation;                     /**< Deviation from baseline */
    float confidence;                    /**< Detection confidence */

    /* Context */
    char component_name[64];             /**< Affected component */
    uint32_t layer_id;                   /**< Affected layer */
    uint32_t region_id;                  /**< Affected region */

    /* Timing */
    uint64_t detection_time_us;          /**< When detected */
    uint64_t first_occurrence_us;        /**< First occurrence */
    uint32_t occurrence_count;           /**< Times detected */

    /* Response */
    plasticity_response_action_t action_taken; /**< Action taken */
    bool action_successful;              /**< Was action effective */
    char recommendation[128];            /**< Recommended follow-up */
} plasticity_anomaly_report_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Plasticity anomaly detection configuration
 */
typedef struct {
    /* Detection settings */
    bool enabled;                        /**< Enable detection */
    uint32_t check_interval_ms;          /**< Check frequency */
    uint32_t history_size;               /**< Metric history size */

    /* Category enables */
    bool detect_weight_anomalies;        /**< Enable weight detection */
    bool detect_timing_anomalies;        /**< Enable timing detection */
    bool detect_bcm_anomalies;           /**< Enable BCM detection */
    bool detect_homeostatic_anomalies;   /**< Enable homeostatic detection */
    bool detect_learning_anomalies;      /**< Enable learning detection */
    bool detect_structural_anomalies;    /**< Enable structural detection */

    /* Sensitivity */
    float sensitivity;                   /**< Overall sensitivity (0.0-1.0) */
    float false_positive_tolerance;      /**< Acceptable false positive rate */

    /* Response configuration */
    bool auto_respond;                   /**< Automatically take action */
    bool escalate_to_health_agent;       /**< Report to health agent */
    plasticity_severity_t min_report_severity; /**< Min severity to report */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async notifications */

    /* Logging */
    bool verbose_logging;                /**< Verbose output */
} plasticity_anomaly_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Detection statistics
 */
typedef struct {
    /* Detection counts */
    uint64_t checks_performed;           /**< Total checks */
    uint64_t anomalies_detected;         /**< Total anomalies */
    uint64_t by_category[PLASTICITY_CATEGORY_COUNT]; /**< Per category */
    uint64_t by_severity[4];             /**< Per severity */

    /* Response statistics */
    uint64_t actions_taken;              /**< Actions taken */
    uint64_t actions_successful;         /**< Successful actions */

    /* Rule statistics */
    uint32_t rules_active;               /**< Active rules */
    uint32_t rules_triggered;            /**< Rules that triggered */

    /* Health metrics */
    float current_health_score;          /**< Overall plasticity health */
    float trend;                         /**< Health trend */

    /* Timing */
    uint64_t last_check_time_us;         /**< Last check timestamp */
    uint64_t total_check_time_us;        /**< Total time in checks */
} plasticity_detection_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

/**
 * @brief Opaque plasticity anomaly detector handle
 */
typedef struct plasticity_anomaly_detector plasticity_anomaly_detector_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Anomaly detected callback
 */
typedef void (*plasticity_anomaly_cb_t)(
    const plasticity_anomaly_report_t* report,
    void* user_data
);

/**
 * @brief Health status callback
 */
typedef void (*plasticity_health_cb_t)(
    float health_score,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_default_config(plasticity_anomaly_config_t* config);

/**
 * @brief Create plasticity anomaly detector
 *
 * @param config Configuration (NULL for defaults)
 * @param health_monitor Health monitor to extend (optional)
 * @return Handle or NULL on failure
 */
plasticity_anomaly_detector_t* plasticity_anomaly_create(
    const plasticity_anomaly_config_t* config,
    health_monitor_t* health_monitor
);

/**
 * @brief Destroy plasticity anomaly detector
 *
 * @param detector Handle (NULL safe)
 */
void plasticity_anomaly_destroy(plasticity_anomaly_detector_t* detector);

/* ============================================================================
 * Rule Management API
 * ============================================================================ */

/**
 * @brief Add detection rule
 *
 * @param detector Detector handle
 * @param rule Rule definition
 * @return Rule ID on success, -1 on error
 */
int plasticity_anomaly_add_rule(
    plasticity_anomaly_detector_t* detector,
    const plasticity_detection_rule_t* rule
);

/**
 * @brief Remove detection rule
 *
 * @param detector Detector handle
 * @param rule_id Rule ID to remove
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_remove_rule(
    plasticity_anomaly_detector_t* detector,
    uint32_t rule_id
);

/**
 * @brief Enable/disable rule
 *
 * @param detector Detector handle
 * @param rule_id Rule ID
 * @param enabled Enable flag
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_set_rule_enabled(
    plasticity_anomaly_detector_t* detector,
    uint32_t rule_id,
    bool enabled
);

/**
 * @brief Load default rules for category
 *
 * @param detector Detector handle
 * @param category Category to load rules for
 * @return Number of rules loaded, -1 on error
 */
int plasticity_anomaly_load_default_rules(
    plasticity_anomaly_detector_t* detector,
    plasticity_anomaly_category_t category
);

/**
 * @brief Load all default rules
 *
 * @param detector Detector handle
 * @return Number of rules loaded, -1 on error
 */
int plasticity_anomaly_load_all_default_rules(
    plasticity_anomaly_detector_t* detector
);

/* ============================================================================
 * Detection API
 * ============================================================================ */

/**
 * @brief Run anomaly detection
 *
 * @param detector Detector handle
 * @return Number of anomalies detected, -1 on error
 */
int plasticity_anomaly_detect(plasticity_anomaly_detector_t* detector);

/**
 * @brief Submit metric for detection
 *
 * @param detector Detector handle
 * @param metric_name Metric name
 * @param value Metric value
 * @param category Metric category
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_submit_metric(
    plasticity_anomaly_detector_t* detector,
    const char* metric_name,
    float value,
    plasticity_anomaly_category_t category
);

/**
 * @brief Submit weight array for analysis
 *
 * @param detector Detector handle
 * @param weights Weight values
 * @param count Number of weights
 * @param component_name Component name
 * @return Number of anomalies, -1 on error
 */
int plasticity_anomaly_analyze_weights(
    plasticity_anomaly_detector_t* detector,
    const float* weights,
    size_t count,
    const char* component_name
);

/**
 * @brief Submit spike timing for analysis
 *
 * @param detector Detector handle
 * @param pre_times Pre-synaptic times
 * @param post_times Post-synaptic times
 * @param count Number of pairs
 * @return Number of anomalies, -1 on error
 */
int plasticity_anomaly_analyze_timing(
    plasticity_anomaly_detector_t* detector,
    const float* pre_times,
    const float* post_times,
    size_t count
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set anomaly callback
 *
 * @param detector Detector handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_set_callback(
    plasticity_anomaly_detector_t* detector,
    plasticity_anomaly_cb_t callback,
    void* user_data
);

/**
 * @brief Set health status callback
 *
 * @param detector Detector handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_set_health_callback(
    plasticity_anomaly_detector_t* detector,
    plasticity_health_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get detection statistics
 *
 * @param detector Detector handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_get_stats(
    const plasticity_anomaly_detector_t* detector,
    plasticity_detection_stats_t* stats
);

/**
 * @brief Get health score
 *
 * @param detector Detector handle
 * @return Health score (0.0-1.0), -1.0 on error
 */
float plasticity_anomaly_get_health(
    const plasticity_anomaly_detector_t* detector
);

/**
 * @brief Get recent anomaly reports
 *
 * @param detector Detector handle
 * @param reports Output report array
 * @param max_reports Maximum reports to return
 * @return Number of reports, -1 on error
 */
int plasticity_anomaly_get_reports(
    const plasticity_anomaly_detector_t* detector,
    plasticity_anomaly_report_t* reports,
    size_t max_reports
);

/**
 * @brief Reset statistics
 *
 * @param detector Detector handle
 */
void plasticity_anomaly_reset_stats(plasticity_anomaly_detector_t* detector);

/**
 * @brief Clear anomaly history
 *
 * @param detector Detector handle
 */
void plasticity_anomaly_clear_history(plasticity_anomaly_detector_t* detector);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param detector Detector handle
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_connect_bio_async(
    plasticity_anomaly_detector_t* detector,
    bio_router_t router
);

/**
 * @brief Broadcast detection status
 *
 * @param detector Detector handle
 * @return 0 on success, -1 on error
 */
int plasticity_anomaly_broadcast(plasticity_anomaly_detector_t* detector);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get anomaly type name
 *
 * @param type Anomaly type
 * @return String name (static)
 */
const char* plasticity_anomaly_type_name(plasticity_anomaly_type_t type);

/**
 * @brief Get category name
 *
 * @param category Category
 * @return String name (static)
 */
const char* plasticity_anomaly_category_name(plasticity_anomaly_category_t category);

/**
 * @brief Get severity name
 *
 * @param severity Severity
 * @return String name (static)
 */
const char* plasticity_anomaly_severity_name(plasticity_severity_t severity);

/**
 * @brief Get action name
 *
 * @param action Action
 * @return String name (static)
 */
const char* plasticity_anomaly_action_name(plasticity_response_action_t action);

/**
 * @brief Get version string
 *
 * @return Version string
 */
const char* plasticity_anomaly_detection_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_ANOMALY_DETECTION_H */
