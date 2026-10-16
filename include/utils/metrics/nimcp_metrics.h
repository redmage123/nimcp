/**
 * @file nimcp_metrics.h
 * @brief NIMCP Metrics Collection and Export System
 * @version 2.6.1
 * @date 2025-11-04
 *
 * Comprehensive metrics collection with support for:
 * - Tableau (CSV, TDE format)
 * - Microsoft PowerBI (CSV, JSON)
 * - Real-time streaming to configurable directory
 * - Time-series data with timestamps
 * - Hierarchical brain performance metrics
 */

#ifndef NIMCP_METRICS_H
#define NIMCP_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_METRICS_MAX_PATH 512
#define NIMCP_METRICS_MAX_NAME 128
#define NIMCP_METRICS_DEFAULT_DIR "./nimcp_metrics"
#define NIMCP_METRICS_BUFFER_SIZE 10000  // Buffer before flush

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Metrics export format
 */
typedef enum {
    NIMCP_METRICS_FORMAT_CSV,           /**< CSV format (Tableau/PowerBI) */
    NIMCP_METRICS_FORMAT_JSON,          /**< JSON format (PowerBI) */
    NIMCP_METRICS_FORMAT_PARQUET,       /**< Parquet format (future) */
    NIMCP_METRICS_FORMAT_TDE            /**< Tableau Data Extract (future) */
} nimcp_metrics_format_t;

/**
 * @brief Metric types
 */
typedef enum {
    NIMCP_METRIC_TYPE_COUNTER,          /**< Monotonically increasing counter */
    NIMCP_METRIC_TYPE_GAUGE,            /**< Point-in-time value */
    NIMCP_METRIC_TYPE_HISTOGRAM,        /**< Distribution of values */
    NIMCP_METRIC_TYPE_TIMER,            /**< Duration measurements */
    NIMCP_METRIC_TYPE_EVENT             /**< Discrete events */
} nimcp_metric_type_t;

/**
 * @brief Metric categories
 */
typedef enum {
    NIMCP_METRIC_CATEGORY_PERFORMANCE,  /**< Performance metrics */
    NIMCP_METRIC_CATEGORY_MEMORY,       /**< Memory usage */
    NIMCP_METRIC_CATEGORY_NETWORK,      /**< Network activity */
    NIMCP_METRIC_CATEGORY_LEARNING,     /**< Learning statistics */
    NIMCP_METRIC_CATEGORY_INFERENCE,    /**< Inference statistics */
    NIMCP_METRIC_CATEGORY_SYSTEM,       /**< System-level metrics */
    NIMCP_METRIC_CATEGORY_CUSTOM        /**< Custom metrics */
} nimcp_metric_category_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Opaque metrics collector handle
 */
typedef struct nimcp_metrics_collector_internal* nimcp_metrics_collector_t;

/**
 * @brief Metrics configuration
 */
typedef struct {
    char output_directory[NIMCP_METRICS_MAX_PATH]; /**< Where to store metrics */
    nimcp_metrics_format_t format;                  /**< Export format */
    uint32_t flush_interval_ms;                     /**< Auto-flush interval */
    uint32_t buffer_size;                           /**< Buffer size before flush */
    bool enable_streaming;                          /**< Enable real-time streaming */
    bool enable_compression;                        /**< Compress output files */
    bool include_timestamps;                        /**< Include timestamps */
    bool include_hostname;                          /**< Include hostname in data */
} nimcp_metrics_config_t;

/**
 * @brief Single metric data point
 */
typedef struct {
    char name[NIMCP_METRICS_MAX_NAME];             /**< Metric name */
    nimcp_metric_type_t type;                       /**< Metric type */
    nimcp_metric_category_t category;               /**< Metric category */
    double value;                                   /**< Metric value */
    uint64_t timestamp_ms;                          /**< Unix timestamp (ms) */
    char labels[256];                               /**< Key-value labels (JSON) */
} nimcp_metric_point_t;

/**
 * @brief Hierarchical brain metrics snapshot
 */
typedef struct {
    // System metrics
    uint64_t timestamp_ms;
    uint32_t num_regions;
    uint32_t num_layers;

    // Performance metrics
    uint64_t total_forward_passes;
    uint64_t total_learning_updates;
    double avg_forward_time_ms;
    double avg_learning_time_ms;

    // Memory metrics
    uint64_t total_memory_bytes;
    uint64_t active_memory_bytes;
    uint32_t num_allocations;

    // Learning metrics
    float avg_learning_rate;
    float avg_error;
    float avg_accuracy;

    // Neuromodulation metrics
    float dopamine_level;
    float acetylcholine_level;
    float serotonin_level;

    // Region metrics
    uint32_t active_regions;
    uint32_t saturated_regions;
} nimcp_hierarchical_metrics_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create metrics collector with default configuration
 *
 * @return Metrics collector handle, NULL on failure
 */
nimcp_metrics_collector_t nimcp_metrics_create(void);

/**
 * @brief Create metrics collector with custom configuration
 *
 * @param config Configuration structure
 * @return Metrics collector handle, NULL on failure
 */
nimcp_metrics_collector_t nimcp_metrics_create_with_config(
    const nimcp_metrics_config_t* config
);

/**
 * @brief Destroy metrics collector and flush pending data
 *
 * @param collector Metrics collector handle
 */
void nimcp_metrics_destroy(nimcp_metrics_collector_t collector);

/**
 * @brief Get default configuration
 *
 * @param config Configuration structure to populate
 */
void nimcp_metrics_get_default_config(nimcp_metrics_config_t* config);

/**
 * @brief Set output directory for metrics
 *
 * @param collector Metrics collector handle
 * @param directory Path to metrics directory
 * @return true on success
 */
bool nimcp_metrics_set_directory(
    nimcp_metrics_collector_t collector,
    const char* directory
);

/**
 * @brief Set export format
 *
 * @param collector Metrics collector handle
 * @param format Export format
 * @return true on success
 */
bool nimcp_metrics_set_format(
    nimcp_metrics_collector_t collector,
    nimcp_metrics_format_t format
);

//=============================================================================
// Metrics Recording API
//=============================================================================

/**
 * @brief Record a counter metric
 *
 * @param collector Metrics collector handle
 * @param name Metric name
 * @param value Counter value
 * @param category Metric category
 * @return true on success
 */
bool nimcp_metrics_record_counter(
    nimcp_metrics_collector_t collector,
    const char* name,
    uint64_t value,
    nimcp_metric_category_t category
);

/**
 * @brief Record a gauge metric
 *
 * @param collector Metrics collector handle
 * @param name Metric name
 * @param value Gauge value
 * @param category Metric category
 * @return true on success
 */
bool nimcp_metrics_record_gauge(
    nimcp_metrics_collector_t collector,
    const char* name,
    double value,
    nimcp_metric_category_t category
);

/**
 * @brief Record a timer metric (duration)
 *
 * @param collector Metrics collector handle
 * @param name Metric name
 * @param duration_ms Duration in milliseconds
 * @param category Metric category
 * @return true on success
 */
bool nimcp_metrics_record_timer(
    nimcp_metrics_collector_t collector,
    const char* name,
    double duration_ms,
    nimcp_metric_category_t category
);

/**
 * @brief Record an event metric
 *
 * @param collector Metrics collector handle
 * @param name Event name
 * @param labels Optional JSON labels
 * @param category Metric category
 * @return true on success
 */
bool nimcp_metrics_record_event(
    nimcp_metrics_collector_t collector,
    const char* name,
    const char* labels,
    nimcp_metric_category_t category
);

/**
 * @brief Record generic metric point
 *
 * @param collector Metrics collector handle
 * @param point Metric data point
 * @return true on success
 */
bool nimcp_metrics_record_point(
    nimcp_metrics_collector_t collector,
    const nimcp_metric_point_t* point
);

//=============================================================================
// Hierarchical Brain Metrics API
//=============================================================================

/**
 * @brief Record hierarchical brain metrics snapshot
 *
 * @param collector Metrics collector handle
 * @param metrics Hierarchical brain metrics
 * @return true on success
 */
bool nimcp_metrics_record_hierarchical(
    nimcp_metrics_collector_t collector,
    const nimcp_hierarchical_metrics_t* metrics
);

/**
 * @brief Start performance timer
 *
 * @param collector Metrics collector handle
 * @param timer_name Timer name
 * @return Timer start timestamp, 0 on failure
 */
uint64_t nimcp_metrics_timer_start(
    nimcp_metrics_collector_t collector,
    const char* timer_name
);

/**
 * @brief Stop performance timer and record
 *
 * @param collector Metrics collector handle
 * @param timer_name Timer name
 * @param start_time Timer start timestamp
 * @param category Metric category
 * @return true on success
 */
bool nimcp_metrics_timer_stop(
    nimcp_metrics_collector_t collector,
    const char* timer_name,
    uint64_t start_time,
    nimcp_metric_category_t category
);

//=============================================================================
// Export and Streaming API
//=============================================================================

/**
 * @brief Flush buffered metrics to disk
 *
 * @param collector Metrics collector handle
 * @return Number of metrics flushed, -1 on error
 */
int32_t nimcp_metrics_flush(nimcp_metrics_collector_t collector);

/**
 * @brief Export metrics to Tableau-compatible CSV
 *
 * @param collector Metrics collector handle
 * @param filename Output filename (relative to metrics dir)
 * @return true on success
 */
bool nimcp_metrics_export_tableau_csv(
    nimcp_metrics_collector_t collector,
    const char* filename
);

/**
 * @brief Export metrics to PowerBI-compatible JSON
 *
 * @param collector Metrics collector handle
 * @param filename Output filename (relative to metrics dir)
 * @return true on success
 */
bool nimcp_metrics_export_powerbi_json(
    nimcp_metrics_collector_t collector,
    const char* filename
);

/**
 * @brief Get metrics statistics
 *
 * @param collector Metrics collector handle
 * @param stats_json Output buffer for JSON statistics
 * @param max_size Maximum buffer size
 * @return Number of bytes written, -1 on error
 */
int32_t nimcp_metrics_get_stats(
    nimcp_metrics_collector_t collector,
    char* stats_json,
    uint32_t max_size
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Query metrics by name pattern
 *
 * @param collector Metrics collector handle
 * @param pattern Name pattern (supports wildcards)
 * @param results Array to store results
 * @param max_results Maximum number of results
 * @return Number of results found, -1 on error
 */
int32_t nimcp_metrics_query_by_name(
    nimcp_metrics_collector_t collector,
    const char* pattern,
    nimcp_metric_point_t* results,
    uint32_t max_results
);

/**
 * @brief Query metrics by time range
 *
 * @param collector Metrics collector handle
 * @param start_time_ms Start timestamp (Unix ms)
 * @param end_time_ms End timestamp (Unix ms)
 * @param results Array to store results
 * @param max_results Maximum number of results
 * @return Number of results found, -1 on error
 */
int32_t nimcp_metrics_query_by_time(
    nimcp_metrics_collector_t collector,
    uint64_t start_time_ms,
    uint64_t end_time_ms,
    nimcp_metric_point_t* results,
    uint32_t max_results
);

/**
 * @brief Query metrics by category
 *
 * @param collector Metrics collector handle
 * @param category Metric category
 * @param results Array to store results
 * @param max_results Maximum number of results
 * @return Number of results found, -1 on error
 */
int32_t nimcp_metrics_query_by_category(
    nimcp_metrics_collector_t collector,
    nimcp_metric_category_t category,
    nimcp_metric_point_t* results,
    uint32_t max_results
);

//=============================================================================
// Bio-Async Integration (Loose Coupling)
//=============================================================================

/**
 * @brief Register metrics module to receive brain probe broadcasts via bio-async
 *
 * WHAT: Registers a handler for BIO_MSG_BRAIN_PROBE_DATA messages
 * WHY:  Enables loose coupling - metrics module receives data without direct dependency on brain API
 * HOW:  Uses bio_router_register_handler() to subscribe to brain probe broadcasts
 *
 * After registration, the metrics module will automatically receive and process
 * brain probe data from any brain that calls nimcp_brain_broadcast_probe().
 * Supports multiple concurrent brains via unique brain_id field.
 *
 * @param collector Metrics collector handle to receive and record metrics
 * @return true on success or if bio-router not available (graceful degradation)
 */
bool nimcp_metrics_register_bio_async(nimcp_metrics_collector_t collector);

/**
 * @brief Process pending bio-async messages for metrics module
 *
 * WHAT: Checks for and processes any pending brain probe messages
 * WHY:  Allows explicit message processing when not using threaded mode
 * HOW:  Calls bio_router_poll() to receive and handle pending messages
 *
 * In threaded mode, this is called automatically. In poll mode, call this
 * periodically to process incoming brain probe broadcasts.
 */
void nimcp_metrics_process_bio_async(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_METRICS_H
