/**
 * @file nimcp_protocol_metrics.h
 * @brief Protocol metrics collection and dashboard
 *
 * WHAT: Collect and expose metrics for all communication protocols
 * WHY:  Monitor health, performance, and usage patterns
 * HOW:  Instrumentation -> aggregation -> export
 *
 * ARCHITECTURE:
 * ┌──────────────────────────────────────────────────────────────┐
 * │                  PROTOCOL METRICS DASHBOARD                   │
 * ├──────────────────────────────────────────────────────────────┤
 * │  ┌────────────┐  ┌────────────┐  ┌────────────┐             │
 * │  │ Bio-Router │  │    NLP     │  │   Swarm    │   ...       │
 * │  │  Metrics   │  │  Metrics   │  │  Metrics   │             │
 * │  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘             │
 * │        │               │               │                     │
 * │        └───────────────┴───────────────┘                     │
 * │                        │                                     │
 * │              ┌─────────▼─────────┐                           │
 * │              │ Metrics Aggregator│                           │
 * │              │  (time-series)    │                           │
 * │              └─────────┬─────────┘                           │
 * │                        │                                     │
 * │              ┌─────────▼─────────┐                           │
 * │              │  Export Formats   │                           │
 * │              │  JSON/Prometheus  │                           │
 * │              └───────────────────┘                           │
 * └──────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_PROTOCOL_METRICS_H
#define NIMCP_PROTOCOL_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct protocol_metrics_struct* protocol_metrics_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Metric types
 */
typedef enum {
    METRIC_TYPE_COUNTER = 0,      /**< Monotonically increasing counter */
    METRIC_TYPE_GAUGE = 1,        /**< Arbitrary value that can go up/down */
    METRIC_TYPE_HISTOGRAM = 2,    /**< Distribution of values */
    METRIC_TYPE_SUMMARY = 3       /**< Summary statistics (P50, P95, P99) */
} metric_type_t;

/**
 * @brief Export formats
 */
typedef enum {
    EXPORT_FORMAT_JSON = 0,       /**< JSON format */
    EXPORT_FORMAT_PROMETHEUS = 1, /**< Prometheus text format */
    EXPORT_FORMAT_CSV = 2         /**< CSV format */
} export_format_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Metric label for categorization
 */
typedef struct {
    char key[32];                 /**< Label key */
    char value[64];               /**< Label value */
} metric_label_t;

#define MAX_METRIC_LABELS 8

/**
 * @brief Individual metric
 */
typedef struct {
    char name[64];                /**< Metric name */
    metric_type_t type;           /**< Metric type */
    double value;                 /**< Current value */
    metric_label_t labels[MAX_METRIC_LABELS]; /**< Labels */
    uint32_t label_count;         /**< Number of labels */
    uint64_t timestamp_ms;        /**< Last update time */
    uint64_t sample_count;        /**< Number of samples (for histograms) */
} metric_t;

/**
 * @brief Histogram bucket
 */
typedef struct {
    double upper_bound;           /**< Upper bound (le) */
    uint64_t count;               /**< Count in bucket */
} histogram_bucket_t;

#define MAX_HISTOGRAM_BUCKETS 16

/**
 * @brief Histogram metric data
 */
typedef struct {
    char name[64];                /**< Metric name */
    histogram_bucket_t buckets[MAX_HISTOGRAM_BUCKETS];
    uint32_t bucket_count;        /**< Number of buckets */
    double sum;                   /**< Sum of all values */
    uint64_t count;               /**< Total count */
    metric_label_t labels[MAX_METRIC_LABELS];
    uint32_t label_count;
} histogram_metric_t;

/**
 * @brief Summary statistics
 */
typedef struct {
    uint64_t total_metrics;       /**< Total number of metrics */
    uint64_t counters;            /**< Number of counters */
    uint64_t gauges;              /**< Number of gauges */
    uint64_t histograms;          /**< Number of histograms */
    uint64_t summaries;           /**< Number of summaries */
    uint64_t total_samples;       /**< Total samples collected */
    size_t memory_usage_bytes;    /**< Approximate memory usage */
    uint64_t oldest_timestamp_ms; /**< Oldest metric timestamp */
    uint64_t newest_timestamp_ms; /**< Newest metric timestamp */
} metrics_summary_t;

/**
 * @brief Dashboard configuration
 */
typedef struct {
    uint32_t collection_interval_ms; /**< Collection interval (0 = manual) */
    uint32_t retention_ms;         /**< How long to keep metrics */
    export_format_t default_export_format; /**< Default export format */
    uint32_t max_metrics;          /**< Maximum number of metrics */
    bool enable_aggregation;       /**< Enable time-series aggregation */
    bool enable_histograms;        /**< Enable histogram support */
} dashboard_config_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create protocol metrics dashboard
 *
 * WHAT: Initialize metrics collection system
 * WHY:  Enable protocol monitoring
 * HOW:  Allocate storage, setup aggregation
 *
 * @param config Configuration (NULL for defaults)
 * @return Metrics handle or NULL on failure
 */
protocol_metrics_t protocol_metrics_create(const dashboard_config_t* config);

/**
 * @brief Destroy metrics dashboard
 *
 * WHAT: Clean up all metrics resources
 * WHY:  Prevent memory leaks
 * HOW:  Free storage, cleanup aggregation
 *
 * @param metrics Metrics handle
 */
void protocol_metrics_destroy(protocol_metrics_t metrics);

/**
 * @brief Get default dashboard configuration
 *
 * @return Default configuration
 */
dashboard_config_t protocol_metrics_default_config(void);

//=============================================================================
// Metric Recording API
//=============================================================================

/**
 * @brief Record a metric value
 *
 * WHAT: Record metric with name, value, and labels
 * WHY:  Track protocol behavior
 * HOW:  Store in hash table with time-series support
 *
 * @param metrics Metrics handle
 * @param name Metric name
 * @param type Metric type
 * @param value Metric value
 * @param labels Array of labels (NULL if none)
 * @param label_count Number of labels
 * @return true on success, false on failure
 */
bool protocol_metrics_record(
    protocol_metrics_t metrics,
    const char* name,
    metric_type_t type,
    double value,
    const metric_label_t* labels,
    uint32_t label_count
);

/**
 * @brief Increment counter metric
 *
 * WHAT: Increment counter by delta
 * WHY:  Convenient counter updates
 * HOW:  Find/create counter, add delta
 *
 * @param metrics Metrics handle
 * @param name Counter name
 * @param delta Amount to increment (default: 1.0)
 * @param labels Array of labels
 * @param label_count Number of labels
 * @return true on success, false on failure
 */
bool protocol_metrics_increment(
    protocol_metrics_t metrics,
    const char* name,
    double delta,
    const metric_label_t* labels,
    uint32_t label_count
);

/**
 * @brief Set gauge metric value
 *
 * WHAT: Set gauge to specific value
 * WHY:  Track current state
 * HOW:  Find/create gauge, set value
 *
 * @param metrics Metrics handle
 * @param name Gauge name
 * @param value New value
 * @param labels Array of labels
 * @param label_count Number of labels
 * @return true on success, false on failure
 */
bool protocol_metrics_set_gauge(
    protocol_metrics_t metrics,
    const char* name,
    double value,
    const metric_label_t* labels,
    uint32_t label_count
);

/**
 * @brief Observe value in histogram
 *
 * WHAT: Record value in histogram
 * WHY:  Track distribution of values
 * HOW:  Update appropriate buckets
 *
 * @param metrics Metrics handle
 * @param name Histogram name
 * @param value Observed value
 * @param labels Array of labels
 * @param label_count Number of labels
 * @return true on success, false on failure
 */
bool protocol_metrics_observe(
    protocol_metrics_t metrics,
    const char* name,
    double value,
    const metric_label_t* labels,
    uint32_t label_count
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get specific metric by name
 *
 * WHAT: Retrieve metric by name
 * WHY:  Access individual metrics
 * HOW:  Hash table lookup
 *
 * @param metrics Metrics handle
 * @param name Metric name
 * @param out_metric Output metric structure
 * @return true if found, false otherwise
 */
bool protocol_metrics_get(
    protocol_metrics_t metrics,
    const char* name,
    metric_t* out_metric
);

/**
 * @brief Get metrics summary
 *
 * WHAT: Get aggregate statistics
 * WHY:  Dashboard overview
 * HOW:  Aggregate all metrics
 *
 * @param metrics Metrics handle
 * @param out_summary Output summary structure
 * @return true on success, false on failure
 */
bool protocol_metrics_get_summary(
    protocol_metrics_t metrics,
    metrics_summary_t* out_summary
);

/**
 * @brief Get all metrics matching pattern
 *
 * WHAT: Query metrics by pattern
 * WHY:  Filtered metric access
 * HOW:  Pattern matching on names
 *
 * @param metrics Metrics handle
 * @param pattern Pattern to match (NULL = all)
 * @param out_metrics Output array
 * @param max_metrics Maximum metrics to return
 * @param out_count Number of metrics returned
 * @return true on success, false on failure
 */
bool protocol_metrics_query(
    protocol_metrics_t metrics,
    const char* pattern,
    metric_t* out_metrics,
    uint32_t max_metrics,
    uint32_t* out_count
);

//=============================================================================
// Export API
//=============================================================================

/**
 * @brief Export metrics in specified format
 *
 * WHAT: Export all metrics to string format
 * WHY:  Integration with monitoring systems
 * HOW:  Format conversion (JSON/Prometheus/CSV)
 *
 * @param metrics Metrics handle
 * @param format Export format
 * @param out_buffer Output buffer
 * @param buffer_size Size of output buffer
 * @param out_bytes Number of bytes written
 * @return true on success, false on failure
 */
bool protocol_metrics_export(
    protocol_metrics_t metrics,
    export_format_t format,
    char* out_buffer,
    size_t buffer_size,
    size_t* out_bytes
);

/**
 * @brief Export metrics to file
 *
 * WHAT: Write metrics to file
 * WHY:  Persistent storage
 * HOW:  Format and write
 *
 * @param metrics Metrics handle
 * @param format Export format
 * @param filename Output filename
 * @return true on success, false on failure
 */
bool protocol_metrics_export_file(
    protocol_metrics_t metrics,
    export_format_t format,
    const char* filename
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Reset all metrics
 *
 * WHAT: Clear all collected metrics
 * WHY:  Start fresh collection
 * HOW:  Clear storage
 *
 * @param metrics Metrics handle
 * @return true on success, false on failure
 */
bool protocol_metrics_reset(protocol_metrics_t metrics);

/**
 * @brief Remove expired metrics
 *
 * WHAT: Remove metrics older than retention period
 * WHY:  Manage memory usage
 * HOW:  Age-based cleanup
 *
 * @param metrics Metrics handle
 * @return Number of metrics removed
 */
uint32_t protocol_metrics_cleanup(protocol_metrics_t metrics);

/**
 * @brief Create metric labels helper
 *
 * WHAT: Create label array from key-value pairs
 * WHY:  Convenient label creation
 * HOW:  Variable argument parsing
 *
 * @param labels Output labels array
 * @param max_labels Maximum labels
 * @param ... Key-value pairs (NULL terminated)
 * @return Number of labels created
 */
uint32_t protocol_metrics_make_labels(
    metric_label_t* labels,
    uint32_t max_labels,
    ...
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTOCOL_METRICS_H */
