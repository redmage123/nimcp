/**
 * @file nimcp_stdp_health_metrics.h
 * @brief STDP and Plasticity Health Metrics for Health Agent
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: Health metrics and monitoring for STDP and plasticity systems
 * WHY:  Detect learning instabilities, weight divergence, and timing anomalies
 * HOW:  Extend health agent with spike-timing specific checks and thresholds
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    STDP Health Metrics Integration                         |
 * +===========================================================================+
 * |                                                                           |
 * |  +-----------------------+       +-----------------------+                |
 * |  |   Health Agent        |       |   Plasticity Systems  |                |
 * |  +-----------------------+       +-----------------------+                |
 * |  | Anomaly Detection     |       | STDP Rules            |                |
 * |  | Threshold Monitoring  |       | BCM Homeostatic       |                |
 * |  | Alert Generation      |       | Eligibility Traces    |                |
 * |  +----------+------------+       +------------+----------+                |
 * |             |                                 |                           |
 * |             v                                 v                           |
 * |  +----------------------------------------------------------+            |
 * |  |            STDP Health Metrics Bridge                    |            |
 * |  |  - Weight stability monitoring                           |            |
 * |  |  - Spike timing anomaly detection                        |            |
 * |  |  - Learning rate stability                               |            |
 * |  |  - Eligibility trace health                              |            |
 * |  +----------------------------------------------------------+            |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * METRICS MONITORED:
 * - Weight divergence (runaway potentiation/depression)
 * - Spike timing violations (pre-post timing outside window)
 * - Learning rate instability (oscillations, explosions)
 * - Eligibility trace decay anomalies
 * - BCM threshold drift
 * - Homeostatic scaling failures
 * - Synaptic saturation (weights at bounds)
 *
 * @see nimcp_health_agent.h
 * @see nimcp_stdp.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_STDP_HEALTH_METRICS_H
#define NIMCP_STDP_HEALTH_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Health agent */
#ifndef NIMCP_HEALTH_AGENT_H
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
#endif

/* STDP context */
#ifndef NIMCP_STDP_H
struct stdp_context;
typedef struct stdp_context stdp_context_t;
#endif

/* BCM context */
#ifndef NIMCP_BCM_H
struct bcm_context;
typedef struct bcm_context bcm_context_t;
#endif

/* Plasticity coordinator */
#ifndef NIMCP_PLASTICITY_COORDINATOR_H
struct plasticity_coordinator;
typedef struct plasticity_coordinator plasticity_coordinator_t;
#endif

/* Bio-async router */
#ifndef NIMCP_BIO_ROUTER_H
typedef struct bio_router_struct* bio_router_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define STDP_HEALTH_METRICS_VERSION     "1.0.0"
#define STDP_HEALTH_METRICS_MAGIC       0x53484D54  /* 'SHMT' */

/** Bio-async module ID */
#define BIO_MODULE_STDP_HEALTH          0x1D10

/** Maximum monitored STDP contexts */
#define STDP_HEALTH_MAX_CONTEXTS        16

/** Default check interval (ms) */
#define STDP_HEALTH_CHECK_INTERVAL_MS   100

/* ============================================================================
 * STDP Anomaly Types (extend base anomaly_type_t)
 * ============================================================================ */

/**
 * @brief STDP-specific anomaly types
 *
 * These extend the base anomaly_type_t with plasticity-specific anomalies
 */
typedef enum {
    /* Weight anomalies (0x100 range) */
    STDP_ANOMALY_WEIGHT_DIVERGENCE = 0x100,    /**< Weights growing unbounded */
    STDP_ANOMALY_WEIGHT_SATURATION,            /**< Weights stuck at bounds */
    STDP_ANOMALY_WEIGHT_OSCILLATION,           /**< Weights oscillating rapidly */
    STDP_ANOMALY_WEIGHT_NAN,                   /**< NaN in weight values */
    STDP_ANOMALY_WEIGHT_COLLAPSE,              /**< All weights collapsed to zero */

    /* Timing anomalies (0x110 range) */
    STDP_ANOMALY_TIMING_VIOLATION = 0x110,     /**< Spike timing outside window */
    STDP_ANOMALY_TIMING_SKEW,                  /**< Systematic timing bias */
    STDP_ANOMALY_TIMING_JITTER,                /**< Excessive timing variability */

    /* Learning rate anomalies (0x120 range) */
    STDP_ANOMALY_LR_EXPLOSION = 0x120,         /**< Learning rate too high */
    STDP_ANOMALY_LR_COLLAPSE,                  /**< Learning rate too low */
    STDP_ANOMALY_LR_OSCILLATION,               /**< Learning rate oscillating */

    /* Eligibility trace anomalies (0x130 range) */
    STDP_ANOMALY_TRACE_OVERFLOW = 0x130,       /**< Trace values overflow */
    STDP_ANOMALY_TRACE_DECAY_FAILURE,          /**< Traces not decaying */
    STDP_ANOMALY_TRACE_ACCUMULATION,           /**< Traces accumulating abnormally */

    /* BCM anomalies (0x140 range) */
    STDP_ANOMALY_BCM_THRESHOLD_DRIFT = 0x140,  /**< BCM threshold drifting */
    STDP_ANOMALY_BCM_RUNAWAY,                  /**< BCM instability */

    /* Homeostatic anomalies (0x150 range) */
    STDP_ANOMALY_HOMEOSTATIC_FAILURE = 0x150,  /**< Homeostatic scaling failed */
    STDP_ANOMALY_ACTIVITY_IMBALANCE,           /**< Activity too high/low */

    /* General plasticity anomalies (0x160 range) */
    STDP_ANOMALY_PLASTICITY_FROZEN = 0x160,    /**< No weight changes occurring */
    STDP_ANOMALY_PLASTICITY_RUNAWAY,           /**< Uncontrolled plasticity */
    STDP_ANOMALY_SYNAPSE_DEATH                 /**< Synapses becoming inactive */
} stdp_anomaly_type_t;

/**
 * @brief STDP anomaly severity
 */
typedef enum {
    STDP_SEVERITY_INFO = 0,          /**< Informational only */
    STDP_SEVERITY_WARNING = 1,       /**< Potential issue */
    STDP_SEVERITY_ERROR = 2,         /**< Requires attention */
    STDP_SEVERITY_CRITICAL = 3       /**< Immediate intervention needed */
} stdp_anomaly_severity_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Weight health thresholds
 */
typedef struct {
    float max_weight_value;          /**< Maximum allowed weight */
    float min_weight_value;          /**< Minimum allowed weight */
    float max_weight_change_rate;    /**< Max change per update */
    float saturation_threshold;      /**< Fraction at bounds = saturation */
    float oscillation_threshold;     /**< Sign changes = oscillation */
    uint32_t oscillation_window;     /**< Window for oscillation detection */
} stdp_weight_thresholds_t;

/**
 * @brief Timing health thresholds
 */
typedef struct {
    float max_timing_window_ms;      /**< Maximum STDP window */
    float timing_jitter_threshold;   /**< Max timing variability */
    float timing_skew_threshold;     /**< Max systematic bias */
} stdp_timing_thresholds_t;

/**
 * @brief Learning rate health thresholds
 */
typedef struct {
    float max_learning_rate;         /**< Maximum allowed LR */
    float min_learning_rate;         /**< Minimum allowed LR */
    float lr_change_threshold;       /**< Max LR change per step */
} stdp_lr_thresholds_t;

/**
 * @brief Eligibility trace health thresholds
 */
typedef struct {
    float max_trace_value;           /**< Maximum trace value */
    float trace_decay_min;           /**< Minimum decay rate */
    float trace_accumulation_limit;  /**< Max accumulated trace */
} stdp_trace_thresholds_t;

/**
 * @brief BCM health thresholds
 */
typedef struct {
    float threshold_drift_limit;     /**< Max BCM threshold change */
    float target_activity_tolerance; /**< Tolerance from target */
} stdp_bcm_thresholds_t;

/**
 * @brief Homeostatic health thresholds
 */
typedef struct {
    float activity_min;              /**< Minimum acceptable activity */
    float activity_max;              /**< Maximum acceptable activity */
    float scaling_rate_limit;        /**< Max scaling adjustment */
} stdp_homeostatic_thresholds_t;

/**
 * @brief STDP health metrics configuration
 */
typedef struct {
    /* Enable flags */
    bool enable_weight_monitoring;   /**< Monitor weight health */
    bool enable_timing_monitoring;   /**< Monitor spike timing */
    bool enable_lr_monitoring;       /**< Monitor learning rate */
    bool enable_trace_monitoring;    /**< Monitor eligibility traces */
    bool enable_bcm_monitoring;      /**< Monitor BCM dynamics */
    bool enable_homeostatic_monitoring; /**< Monitor homeostatic scaling */

    /* Check frequency */
    uint32_t check_interval_ms;      /**< How often to check */
    uint32_t sample_window_size;     /**< Samples for statistics */

    /* Thresholds */
    stdp_weight_thresholds_t weight_thresholds;
    stdp_timing_thresholds_t timing_thresholds;
    stdp_lr_thresholds_t lr_thresholds;
    stdp_trace_thresholds_t trace_thresholds;
    stdp_bcm_thresholds_t bcm_thresholds;
    stdp_homeostatic_thresholds_t homeostatic_thresholds;

    /* Response configuration */
    bool auto_pause_on_critical;     /**< Pause plasticity on critical */
    bool auto_reset_on_divergence;   /**< Reset weights on divergence */
    bool notify_health_agent;        /**< Notify health agent */

    /* Bio-async integration */
    bool enable_bio_async;           /**< Enable bio-async notifications */

    /* Logging */
    bool verbose_logging;            /**< Verbose output */
} stdp_health_config_t;

/* ============================================================================
 * Anomaly Report
 * ============================================================================ */

/**
 * @brief STDP anomaly report
 */
typedef struct {
    stdp_anomaly_type_t type;        /**< Anomaly type */
    stdp_anomaly_severity_t severity; /**< Severity level */

    /* Context */
    uint32_t context_id;             /**< STDP context ID */
    char context_name[64];           /**< Context name */

    /* Details */
    char description[256];           /**< Human-readable description */
    float current_value;             /**< Current metric value */
    float threshold_value;           /**< Threshold that was violated */
    float deviation;                 /**< Deviation from normal */

    /* Location */
    uint32_t layer_id;               /**< Affected layer */
    uint32_t synapse_start;          /**< Start synapse index */
    uint32_t synapse_count;          /**< Number of affected synapses */

    /* Timing */
    uint64_t detection_time_us;      /**< When detected */
    uint64_t duration_us;            /**< How long anomaly persisted */

    /* Suggested action */
    char suggested_action[128];      /**< Recommended response */
} stdp_anomaly_report_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Weight health statistics
 */
typedef struct {
    float mean_weight;               /**< Mean weight value */
    float weight_variance;           /**< Weight variance */
    float max_weight;                /**< Maximum weight */
    float min_weight;                /**< Minimum weight */
    float saturation_ratio;          /**< Fraction at bounds */
    uint64_t sign_changes;           /**< Weight sign changes */
    uint64_t nan_count;              /**< NaN occurrences */
} stdp_weight_stats_t;

/**
 * @brief Timing health statistics
 */
typedef struct {
    float mean_timing_ms;            /**< Mean spike timing */
    float timing_std_dev;            /**< Timing standard deviation */
    float timing_skew;               /**< Systematic timing bias */
    uint64_t violations;             /**< Timing window violations */
} stdp_timing_stats_t;

/**
 * @brief Learning statistics
 */
typedef struct {
    float current_lr;                /**< Current learning rate */
    float lr_trend;                  /**< Learning rate trend */
    uint64_t lr_changes;             /**< Learning rate changes */
    float total_weight_delta;        /**< Total weight change */
} stdp_learning_stats_t;

/**
 * @brief Overall STDP health statistics
 */
typedef struct {
    /* Per-metric statistics */
    stdp_weight_stats_t weight_stats;
    stdp_timing_stats_t timing_stats;
    stdp_learning_stats_t learning_stats;

    /* Anomaly counts */
    uint64_t anomalies_detected;     /**< Total anomalies */
    uint64_t anomalies_by_type[32];  /**< Count per type */
    uint64_t critical_count;         /**< Critical anomalies */
    uint64_t warning_count;          /**< Warning anomalies */

    /* Health scores (0.0 = unhealthy, 1.0 = healthy) */
    float weight_health_score;       /**< Weight health */
    float timing_health_score;       /**< Timing health */
    float lr_health_score;           /**< Learning rate health */
    float overall_health_score;      /**< Overall STDP health */

    /* Monitoring info */
    uint64_t checks_performed;       /**< Total health checks */
    uint64_t last_check_time_us;     /**< Last check timestamp */
    uint64_t contexts_monitored;     /**< STDP contexts monitored */
} stdp_health_stats_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Anomaly detection callback
 */
typedef void (*stdp_anomaly_callback_t)(
    const stdp_anomaly_report_t* report,
    void* user_data
);

/**
 * @brief Health check callback (periodic)
 */
typedef void (*stdp_health_check_callback_t)(
    const stdp_health_stats_t* stats,
    void* user_data
);

/* ============================================================================
 * Handle
 * ============================================================================ */

/**
 * @brief Opaque STDP health metrics handle
 */
typedef struct stdp_health_metrics stdp_health_metrics_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int stdp_health_default_config(stdp_health_config_t* config);

/**
 * @brief Create STDP health metrics monitor
 *
 * @param config Configuration (NULL for defaults)
 * @param health_agent Health agent to extend (optional)
 * @return Handle or NULL on failure
 */
stdp_health_metrics_t* stdp_health_create(
    const stdp_health_config_t* config,
    nimcp_health_agent_t* health_agent
);

/**
 * @brief Destroy STDP health metrics monitor
 *
 * @param metrics Handle (NULL safe)
 */
void stdp_health_destroy(stdp_health_metrics_t* metrics);

/* ============================================================================
 * Registration API
 * ============================================================================ */

/**
 * @brief Register STDP context for monitoring
 *
 * @param metrics Health metrics handle
 * @param stdp STDP context to monitor
 * @param name Context name for reporting
 * @return Context ID on success, -1 on error
 */
int stdp_health_register_stdp(
    stdp_health_metrics_t* metrics,
    stdp_context_t* stdp,
    const char* name
);

/**
 * @brief Register BCM context for monitoring
 *
 * @param metrics Health metrics handle
 * @param bcm BCM context to monitor
 * @param name Context name for reporting
 * @return Context ID on success, -1 on error
 */
int stdp_health_register_bcm(
    stdp_health_metrics_t* metrics,
    bcm_context_t* bcm,
    const char* name
);

/**
 * @brief Register plasticity coordinator
 *
 * @param metrics Health metrics handle
 * @param coordinator Plasticity coordinator
 * @return 0 on success, -1 on error
 */
int stdp_health_register_coordinator(
    stdp_health_metrics_t* metrics,
    plasticity_coordinator_t* coordinator
);

/**
 * @brief Unregister context from monitoring
 *
 * @param metrics Health metrics handle
 * @param context_id Context ID to unregister
 * @return 0 on success, -1 on error
 */
int stdp_health_unregister(
    stdp_health_metrics_t* metrics,
    int context_id
);

/* ============================================================================
 * Monitoring API
 * ============================================================================ */

/**
 * @brief Perform health check
 *
 * @param metrics Health metrics handle
 * @return Number of anomalies detected, -1 on error
 */
int stdp_health_check(stdp_health_metrics_t* metrics);

/**
 * @brief Check specific weight array
 *
 * @param metrics Health metrics handle
 * @param weights Weight array
 * @param count Number of weights
 * @param context_name Context name for reporting
 * @return Number of anomalies, -1 on error
 */
int stdp_health_check_weights(
    stdp_health_metrics_t* metrics,
    const float* weights,
    size_t count,
    const char* context_name
);

/**
 * @brief Check spike timing
 *
 * @param metrics Health metrics handle
 * @param pre_times Pre-synaptic spike times
 * @param post_times Post-synaptic spike times
 * @param count Number of spike pairs
 * @return Number of violations, -1 on error
 */
int stdp_health_check_timing(
    stdp_health_metrics_t* metrics,
    const float* pre_times,
    const float* post_times,
    size_t count
);

/**
 * @brief Check learning rate stability
 *
 * @param metrics Health metrics handle
 * @param learning_rate Current learning rate
 * @param context_name Context name
 * @return 0 if healthy, anomaly type if not
 */
int stdp_health_check_learning_rate(
    stdp_health_metrics_t* metrics,
    float learning_rate,
    const char* context_name
);

/**
 * @brief Check eligibility traces
 *
 * @param metrics Health metrics handle
 * @param traces Trace values
 * @param count Number of traces
 * @return Number of anomalies, -1 on error
 */
int stdp_health_check_traces(
    stdp_health_metrics_t* metrics,
    const float* traces,
    size_t count
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set anomaly callback
 *
 * @param metrics Health metrics handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, -1 on error
 */
int stdp_health_set_anomaly_callback(
    stdp_health_metrics_t* metrics,
    stdp_anomaly_callback_t callback,
    void* user_data
);

/**
 * @brief Set periodic health check callback
 *
 * @param metrics Health metrics handle
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, -1 on error
 */
int stdp_health_set_check_callback(
    stdp_health_metrics_t* metrics,
    stdp_health_check_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get health statistics
 *
 * @param metrics Health metrics handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int stdp_health_get_stats(
    const stdp_health_metrics_t* metrics,
    stdp_health_stats_t* stats
);

/**
 * @brief Get overall health score
 *
 * @param metrics Health metrics handle
 * @return Health score (0.0-1.0), -1.0 on error
 */
float stdp_health_get_score(const stdp_health_metrics_t* metrics);

/**
 * @brief Reset statistics
 *
 * @param metrics Health metrics handle
 */
void stdp_health_reset_stats(stdp_health_metrics_t* metrics);

/**
 * @brief Check if monitoring is healthy
 *
 * @param metrics Health metrics handle
 * @return true if healthy, false if anomalies present
 */
bool stdp_health_is_healthy(const stdp_health_metrics_t* metrics);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param metrics Health metrics handle
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
int stdp_health_connect_bio_async(
    stdp_health_metrics_t* metrics,
    bio_router_t router
);

/**
 * @brief Broadcast health status
 *
 * @param metrics Health metrics handle
 * @return 0 on success, -1 on error
 */
int stdp_health_broadcast_status(stdp_health_metrics_t* metrics);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get anomaly type name
 *
 * @param type Anomaly type
 * @return String name (static)
 */
const char* stdp_anomaly_type_name(stdp_anomaly_type_t type);

/**
 * @brief Get severity name
 *
 * @param severity Severity level
 * @return String name (static)
 */
const char* stdp_anomaly_severity_name(stdp_anomaly_severity_t severity);

/**
 * @brief Get version string
 *
 * @return Version string
 */
const char* stdp_health_metrics_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_HEALTH_METRICS_H */
