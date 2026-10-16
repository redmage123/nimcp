/**
 * @file nimcp_immune_metrics.h
 * @brief Immune System Metrics and Observability - Prometheus/JSON Export
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Comprehensive metrics collection and export for the self-healing
 *       immune system, supporting Prometheus and JSON output formats
 * WHY:  Enable monitoring, alerting, and observability for production
 *       deployments of the self-healing crash recovery system
 * HOW:  Collect counters, gauges, and histograms for crashes, fixes,
 *       B-cells, antibodies; export in Prometheus text format or JSON
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * -----------------------------------------------------------------
 * Blood work metrics           -> Crash and fix counters
 * Immune cell counts           -> B-cell/T-cell population metrics
 * Antibody titers              -> Antibody effectiveness scores
 * Cytokine levels              -> Pattern usage statistics
 * Response time                -> Fix latency histograms
 * Immune memory                -> Memory usage tracking
 * ```
 *
 * METRIC TYPES:
 * - Counter: Monotonically increasing values (crashes, fixes)
 * - Gauge: Values that can go up/down (active cells, memory)
 * - Histogram: Distribution of values (latency buckets)
 *
 * EXPORT FORMATS:
 * - Prometheus: Standard text exposition format for scraping
 * - JSON: Structured format for dashboards and APIs
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_IMMUNE_METRICS_H
#define NIMCP_IMMUNE_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define IMMUNE_METRICS_MAX_LABEL_LEN     64    /**< Max label key/value length */
#define IMMUNE_METRICS_MAX_LABELS        8     /**< Max labels per metric */
#define IMMUNE_METRICS_MAX_BUCKETS       16    /**< Max histogram buckets */
#define IMMUNE_METRICS_MAX_NAME_LEN      128   /**< Max metric name length */
#define IMMUNE_METRICS_MAX_HELP_LEN      256   /**< Max help string length */
#define IMMUNE_METRICS_DEFAULT_PORT      9100  /**< Default HTTP server port */
#define IMMUNE_METRICS_EXPORT_BUFFER     65536 /**< Default export buffer size */
#define IMMUNE_METRICS_MODULE_NAME       "immune_metrics"

/* Histogram bucket defaults for latency (microseconds) */
#define IMMUNE_METRICS_LATENCY_BUCKET_1   100     /**< 100us */
#define IMMUNE_METRICS_LATENCY_BUCKET_2   500     /**< 500us */
#define IMMUNE_METRICS_LATENCY_BUCKET_3   1000    /**< 1ms */
#define IMMUNE_METRICS_LATENCY_BUCKET_4   5000    /**< 5ms */
#define IMMUNE_METRICS_LATENCY_BUCKET_5   10000   /**< 10ms */
#define IMMUNE_METRICS_LATENCY_BUCKET_6   50000   /**< 50ms */
#define IMMUNE_METRICS_LATENCY_BUCKET_7   100000  /**< 100ms */
#define IMMUNE_METRICS_LATENCY_BUCKET_8   500000  /**< 500ms */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct immune_metrics_s immune_metrics_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Metric types following Prometheus conventions
 */
typedef enum {
    METRIC_TYPE_COUNTER = 0,   /**< Monotonically increasing counter */
    METRIC_TYPE_GAUGE,         /**< Value that can increase or decrease */
    METRIC_TYPE_HISTOGRAM,     /**< Bucketed distribution of values */
    METRIC_TYPE_SUMMARY        /**< Quantile-based distribution (not implemented) */
} immune_metric_type_t;

/**
 * @brief Crash signal types for labeling
 */
typedef enum {
    CRASH_SIGNAL_SIGSEGV = 0,  /**< Segmentation fault */
    CRASH_SIGNAL_SIGFPE,       /**< Floating point exception */
    CRASH_SIGNAL_SIGBUS,       /**< Bus error */
    CRASH_SIGNAL_SIGABRT,      /**< Abort signal */
    CRASH_SIGNAL_SIGILL,       /**< Illegal instruction */
    CRASH_SIGNAL_SIGTRAP,      /**< Trace/breakpoint trap */
    CRASH_SIGNAL_OTHER,        /**< Other signals */
    CRASH_SIGNAL_COUNT
} crash_signal_type_t;

/**
 * @brief Alert severity levels
 */
typedef enum {
    ALERT_SEVERITY_INFO = 0,   /**< Informational alert */
    ALERT_SEVERITY_WARNING,    /**< Warning - attention needed */
    ALERT_SEVERITY_CRITICAL,   /**< Critical - immediate action required */
    ALERT_SEVERITY_EMERGENCY   /**< Emergency - system at risk */
} alert_severity_t;

/**
 * @brief Alert types for callbacks
 */
typedef enum {
    ALERT_REPEATED_FAILURES = 0,   /**< Multiple consecutive failures */
    ALERT_RESOURCE_EXHAUSTION,     /**< Memory/cell pool exhausted */
    ALERT_HIGH_CRASH_RATE,         /**< Crash rate above threshold */
    ALERT_LOW_SUCCESS_RATE,        /**< Fix success rate below threshold */
    ALERT_PATTERN_DEGRADATION,     /**< Pattern effectiveness declining */
    ALERT_LNN_TRAINING_STALL,      /**< LNN not improving */
    ALERT_MEMORY_PRESSURE,         /**< Memory usage critical */
    ALERT_B_CELL_EXHAUSTION,       /**< B-cell pool depleted */
    ALERT_COUNT
} alert_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Label key-value pair for metrics
 */
typedef struct {
    char key[IMMUNE_METRICS_MAX_LABEL_LEN];    /**< Label key */
    char value[IMMUNE_METRICS_MAX_LABEL_LEN];  /**< Label value */
} metric_label_t;

/**
 * @brief Histogram bucket definition
 */
typedef struct {
    double upper_bound;    /**< Upper bound for this bucket (exclusive) */
    uint64_t count;        /**< Count of observations <= upper_bound */
} histogram_bucket_t;

/**
 * @brief Histogram data structure
 */
typedef struct {
    histogram_bucket_t buckets[IMMUNE_METRICS_MAX_BUCKETS]; /**< Bucket array */
    size_t n_buckets;                                        /**< Number of buckets */
    uint64_t total_count;                                    /**< Total observations */
    double sum;                                              /**< Sum of all observations */
} histogram_data_t;

/**
 * @brief Crash statistics by signal type
 */
typedef struct {
    uint64_t count[CRASH_SIGNAL_COUNT];    /**< Crash counts by signal */
    uint64_t total;                         /**< Total crashes */
    uint64_t last_timestamp;                /**< Last crash timestamp */
} crash_stats_t;

/**
 * @brief Fix attempt statistics
 */
typedef struct {
    uint64_t total_attempts;               /**< Total fix attempts */
    uint64_t successes;                    /**< Successful fixes */
    uint64_t failures;                     /**< Failed fixes */
    uint64_t pattern_fixes;                /**< Fixes from patterns */
    uint64_t lnn_fixes;                    /**< Fixes from LNN */
    uint64_t hybrid_fixes;                 /**< Fixes from pattern+LNN */
    float success_rate;                    /**< Current success rate (0-1) */
    float avg_confidence;                  /**< Average fix confidence */
} fix_stats_t;

/**
 * @brief Pattern usage statistics
 */
typedef struct {
    uint64_t null_check_uses;              /**< NULL check pattern uses */
    uint64_t bounds_check_uses;            /**< Bounds check pattern uses */
    uint64_t zero_check_uses;              /**< Division-by-zero check uses */
    uint64_t uaf_check_uses;               /**< Use-after-free check uses */
    uint64_t align_fix_uses;               /**< Alignment fix uses */
    uint64_t double_free_uses;             /**< Double-free protection uses */
    uint64_t overflow_check_uses;          /**< Overflow check uses */
    uint64_t lnn_generated_uses;           /**< LNN-generated fix uses */
    uint64_t custom_pattern_uses;          /**< Custom pattern uses */
} pattern_stats_t;

/**
 * @brief B-cell population metrics
 */
typedef struct {
    uint64_t total_created;                /**< Total B-cells created */
    uint64_t current_count;                /**< Current active count */
    uint64_t naive_count;                  /**< Naive B-cells */
    uint64_t activated_count;              /**< Activated B-cells */
    uint64_t plasma_count;                 /**< Plasma B-cells */
    uint64_t memory_count;                 /**< Memory B-cells */
    uint64_t apoptotic_count;              /**< Apoptotic B-cells */
    float avg_affinity;                    /**< Average antigen affinity */
} bcell_metrics_t;

/**
 * @brief Antibody metrics
 */
typedef struct {
    uint64_t total_produced;               /**< Total antibodies produced */
    uint64_t current_active;               /**< Currently active antibodies */
    uint64_t igm_count;                    /**< IgM class count */
    uint64_t igg_count;                    /**< IgG class count */
    uint64_t ige_count;                    /**< IgE class count */
    uint64_t neutralizations;              /**< Successful neutralizations */
    float avg_effectiveness;               /**< Average effectiveness score */
} antibody_metrics_t;

/**
 * @brief Memory usage metrics
 */
typedef struct {
    size_t total_allocated;                /**< Total bytes allocated */
    size_t training_samples;               /**< Training sample memory */
    size_t pattern_library;                /**< Pattern library memory */
    size_t lnn_network;                    /**< LNN network memory */
    size_t b_cell_pool;                    /**< B-cell pool memory */
    size_t antibody_pool;                  /**< Antibody pool memory */
    size_t peak_usage;                     /**< Peak memory usage */
} memory_metrics_t;

/**
 * @brief Complete metrics snapshot
 */
typedef struct {
    crash_stats_t crashes;                 /**< Crash statistics */
    fix_stats_t fixes;                     /**< Fix statistics */
    pattern_stats_t patterns;              /**< Pattern usage */
    bcell_metrics_t b_cells;               /**< B-cell metrics */
    antibody_metrics_t antibodies;         /**< Antibody metrics */
    memory_metrics_t memory;               /**< Memory usage */
    histogram_data_t fix_latency;          /**< Fix latency histogram */

    /* System state */
    uint64_t uptime_ms;                    /**< Uptime in milliseconds */
    uint64_t last_update_time;             /**< Last metrics update time */
    bool engine_initialized;               /**< Engine is initialized */
    bool lnn_enabled;                      /**< LNN is enabled */
    bool learning_enabled;                 /**< Learning is enabled */
} immune_metrics_snapshot_t;

/**
 * @brief Alert callback function type
 */
typedef void (*immune_alert_callback_t)(
    alert_type_t type,
    alert_severity_t severity,
    const char* message,
    void* user_data
);

/**
 * @brief Alert configuration
 */
typedef struct {
    uint32_t failure_threshold;            /**< Consecutive failures for alert */
    float success_rate_threshold;          /**< Minimum success rate (0-1) */
    float crash_rate_threshold;            /**< Max crashes per minute */
    size_t memory_threshold_bytes;         /**< Memory usage threshold */
    uint32_t b_cell_min_count;             /**< Minimum B-cell count */
    float pattern_effectiveness_min;       /**< Min pattern effectiveness */
    uint64_t check_interval_ms;            /**< Alert check interval */
} alert_config_t;

/**
 * @brief Metrics configuration
 */
typedef struct {
    bool enable_histogram;                 /**< Enable latency histograms */
    bool enable_http_server;               /**< Enable HTTP metrics endpoint */
    uint16_t http_port;                    /**< HTTP server port */
    uint64_t update_interval_ms;           /**< Metrics update interval */
    bool enable_alerting;                  /**< Enable alerting system */
    alert_config_t alert_config;           /**< Alert configuration */
} immune_metrics_config_t;

/**
 * @brief Metrics collector state (opaque)
 */
struct immune_metrics_s {
    immune_metrics_config_t config;        /**< Configuration */

    /* Metrics storage */
    crash_stats_t crashes;                 /**< Crash counters */
    fix_stats_t fixes;                     /**< Fix counters */
    pattern_stats_t patterns;              /**< Pattern usage */
    bcell_metrics_t b_cells;               /**< B-cell metrics */
    antibody_metrics_t antibodies;         /**< Antibody metrics */
    memory_metrics_t memory;               /**< Memory metrics */
    histogram_data_t fix_latency;          /**< Fix latency histogram */

    /* Alerting */
    immune_alert_callback_t alert_callback; /**< Alert callback */
    void* alert_user_data;                  /**< Alert callback user data */
    uint32_t consecutive_failures;          /**< Current failure streak */
    uint64_t last_alert_time;               /**< Last alert timestamp */

    /* HTTP server (if enabled) */
    void* http_server;                     /**< HTTP server handle */
    bool http_running;                     /**< HTTP server is running */

    /* Thread safety */
    void* mutex;                           /**< Access mutex */

    /* State */
    uint64_t start_time;                   /**< Collector start time */
    bool initialized;                      /**< Collector initialized */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default metrics configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Easy initialization
 * HOW:  Set reasonable thresholds and intervals
 *
 * @param config Configuration to populate
 * @return 0 on success, -1 on error
 */
int immune_metrics_default_config(immune_metrics_config_t* config);

/**
 * @brief Create metrics collector
 *
 * WHAT: Initialize metrics collection system
 * WHY:  Set up counters, histograms, optional HTTP server
 * HOW:  Allocate resources, initialize histogram buckets
 *
 * @param config Configuration (NULL for defaults)
 * @return Metrics collector or NULL on failure
 */
immune_metrics_t* immune_metrics_create(const immune_metrics_config_t* config);

/**
 * @brief Destroy metrics collector
 *
 * WHAT: Clean up metrics collector resources
 * WHY:  Proper resource deallocation
 * HOW:  Stop HTTP server, free memory
 *
 * @param metrics Metrics collector (NULL-safe)
 */
void immune_metrics_destroy(immune_metrics_t* metrics);

/* ============================================================================
 * Recording API
 * ============================================================================ */

/**
 * @brief Record a crash event
 *
 * WHAT: Increment crash counter for signal type
 * WHY:  Track crash frequency by type
 * HOW:  Atomic increment of appropriate counter
 *
 * @param metrics Metrics collector
 * @param signal_type Type of crash signal
 * @return 0 on success, -1 on error
 */
int immune_metrics_record_crash(
    immune_metrics_t* metrics,
    crash_signal_type_t signal_type
);

/**
 * @brief Record a fix attempt
 *
 * WHAT: Record fix attempt result
 * WHY:  Track fix success/failure rates
 * HOW:  Update counters based on result
 *
 * @param metrics Metrics collector
 * @param success Whether fix was successful
 * @param pattern_based Whether fix was from pattern
 * @param lnn_based Whether fix was from LNN
 * @param confidence Fix confidence score
 * @return 0 on success, -1 on error
 */
int immune_metrics_record_fix(
    immune_metrics_t* metrics,
    bool success,
    bool pattern_based,
    bool lnn_based,
    float confidence
);

/**
 * @brief Record fix latency
 *
 * WHAT: Add latency observation to histogram
 * WHY:  Track fix time distribution
 * HOW:  Find appropriate bucket, increment count
 *
 * @param metrics Metrics collector
 * @param latency_us Latency in microseconds
 * @return 0 on success, -1 on error
 */
int immune_metrics_record_latency(
    immune_metrics_t* metrics,
    uint64_t latency_us
);

/**
 * @brief Record pattern usage
 *
 * WHAT: Increment pattern usage counter
 * WHY:  Track which patterns are most used
 * HOW:  Increment counter for pattern type
 *
 * @param metrics Metrics collector
 * @param pattern_type Pattern type (from fix_pattern_type_t)
 * @return 0 on success, -1 on error
 */
int immune_metrics_record_pattern_use(
    immune_metrics_t* metrics,
    int pattern_type
);

/**
 * @brief Update B-cell population metrics
 *
 * WHAT: Set current B-cell population state
 * WHY:  Track B-cell lifecycle
 * HOW:  Update gauge values
 *
 * @param metrics Metrics collector
 * @param bcell_metrics Current B-cell state
 * @return 0 on success, -1 on error
 */
int immune_metrics_update_b_cells(
    immune_metrics_t* metrics,
    const bcell_metrics_t* bcell_metrics
);

/**
 * @brief Update antibody metrics
 *
 * WHAT: Set current antibody state
 * WHY:  Track antibody production and effectiveness
 * HOW:  Update gauge values
 *
 * @param metrics Metrics collector
 * @param antibody_metrics Current antibody state
 * @return 0 on success, -1 on error
 */
int immune_metrics_update_antibodies(
    immune_metrics_t* metrics,
    const antibody_metrics_t* antibody_metrics
);

/**
 * @brief Update memory usage metrics
 *
 * WHAT: Set current memory usage
 * WHY:  Track resource consumption
 * HOW:  Update gauge values, check peak
 *
 * @param metrics Metrics collector
 * @param memory_metrics Current memory state
 * @return 0 on success, -1 on error
 */
int immune_metrics_update_memory(
    immune_metrics_t* metrics,
    const memory_metrics_t* memory_metrics
);

/* ============================================================================
 * Export API
 * ============================================================================ */

/**
 * @brief Export metrics in Prometheus text format
 *
 * WHAT: Generate Prometheus exposition format output
 * WHY:  Enable Prometheus scraping
 * HOW:  Format all metrics with TYPE, HELP, and values
 *
 * @param metrics Metrics collector
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or -1 on error
 */
int immune_metrics_export_prometheus(
    immune_metrics_t* metrics,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Export metrics in JSON format
 *
 * WHAT: Generate JSON metrics output
 * WHY:  Enable dashboard/API consumption
 * HOW:  Format all metrics as JSON object
 *
 * @param metrics Metrics collector
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or -1 on error
 */
int immune_metrics_export_json(
    immune_metrics_t* metrics,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get metrics snapshot
 *
 * WHAT: Get complete metrics snapshot
 * WHY:  Allow atomic read of all metrics
 * HOW:  Copy all metrics under lock
 *
 * @param metrics Metrics collector
 * @param snapshot Output snapshot
 * @return 0 on success, -1 on error
 */
int immune_metrics_get_snapshot(
    immune_metrics_t* metrics,
    immune_metrics_snapshot_t* snapshot
);

/* ============================================================================
 * HTTP Server API (Optional)
 * ============================================================================ */

/**
 * @brief Start HTTP metrics server
 *
 * WHAT: Start simple HTTP server for metrics
 * WHY:  Enable Prometheus scraping via /metrics endpoint
 * HOW:  Listen on configured port, serve Prometheus format
 *
 * @param metrics Metrics collector
 * @return 0 on success, -1 on error
 */
int immune_metrics_start_http_server(immune_metrics_t* metrics);

/**
 * @brief Stop HTTP metrics server
 *
 * WHAT: Stop HTTP server
 * WHY:  Clean shutdown
 * HOW:  Close listening socket, stop thread
 *
 * @param metrics Metrics collector
 * @return 0 on success, -1 on error
 */
int immune_metrics_stop_http_server(immune_metrics_t* metrics);

/**
 * @brief Check if HTTP server is running
 *
 * @param metrics Metrics collector
 * @return true if running
 */
bool immune_metrics_http_is_running(immune_metrics_t* metrics);

/* ============================================================================
 * Alerting API
 * ============================================================================ */

/**
 * @brief Set alert callback
 *
 * WHAT: Register callback for alert notifications
 * WHY:  Enable external alerting integration
 * HOW:  Store callback and user data
 *
 * @param metrics Metrics collector
 * @param callback Alert callback function
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
int immune_metrics_set_alert_callback(
    immune_metrics_t* metrics,
    immune_alert_callback_t callback,
    void* user_data
);

/**
 * @brief Configure alert thresholds
 *
 * WHAT: Set alerting thresholds
 * WHY:  Customize when alerts fire
 * HOW:  Update alert configuration
 *
 * @param metrics Metrics collector
 * @param config Alert configuration
 * @return 0 on success, -1 on error
 */
int immune_metrics_configure_alerts(
    immune_metrics_t* metrics,
    const alert_config_t* config
);

/**
 * @brief Check alert conditions
 *
 * WHAT: Evaluate all alert conditions
 * WHY:  Trigger alerts when thresholds exceeded
 * HOW:  Check each condition, call callback if needed
 *
 * @param metrics Metrics collector
 * @return Number of alerts triggered or -1 on error
 */
int immune_metrics_check_alerts(immune_metrics_t* metrics);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Reset all metrics
 *
 * WHAT: Clear all metric values
 * WHY:  Start fresh measurement period
 * HOW:  Zero all counters and gauges
 *
 * @param metrics Metrics collector
 * @return 0 on success, -1 on error
 */
int immune_metrics_reset(immune_metrics_t* metrics);

/**
 * @brief Get crash signal type name
 *
 * @param signal_type Signal type
 * @return String name
 */
const char* immune_metrics_signal_to_string(crash_signal_type_t signal_type);

/**
 * @brief Get alert type name
 *
 * @param alert_type Alert type
 * @return String name
 */
const char* immune_metrics_alert_to_string(alert_type_t alert_type);

/**
 * @brief Get alert severity name
 *
 * @param severity Severity level
 * @return String name
 */
const char* immune_metrics_severity_to_string(alert_severity_t severity);

/**
 * @brief Get uptime in milliseconds
 *
 * @param metrics Metrics collector
 * @return Uptime in ms or 0 on error
 */
uint64_t immune_metrics_get_uptime(immune_metrics_t* metrics);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNE_METRICS_H */
