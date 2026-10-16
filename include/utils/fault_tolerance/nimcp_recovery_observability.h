/**
 * @file nimcp_recovery_observability.h
 * @brief Recovery Metrics and Observability System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive metrics collection for fault tolerance operations
 * WHY:  Enable debugging, optimization, and SLA tracking
 * HOW:  MTTR tracking, success rates, latency histograms, distributed tracing
 *
 * BIOLOGICAL BASIS:
 * - Interoception (brain's awareness of internal body states)
 * - Hypothalamus monitoring (temperature, hunger, thirst, fatigue)
 * - Autonomic feedback loops (heart rate variability)
 * - Cytokine signaling (immune system status reporting)
 *
 * KEY METRICS:
 * - MTTR (Mean Time To Recovery)
 * - MTBF (Mean Time Between Failures)
 * - Recovery success rate
 * - Latency percentiles (P50, P95, P99)
 * - Resource utilization during recovery
 * - Cascading failure metrics
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RECOVERY_OBSERVABILITY_H
#define NIMCP_RECOVERY_OBSERVABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define RO_MAX_SPANS 256                    /**< Max trace spans */
#define RO_MAX_METRICS 128                  /**< Max metric types */
#define RO_MAX_LABELS 8                     /**< Max labels per metric */
#define RO_MAX_EVENTS 1024                  /**< Max events in buffer */
#define RO_MAX_EXPORTERS 4                  /**< Max metric exporters */
#define RO_HISTOGRAM_BUCKETS 16             /**< Histogram bucket count */
#define RO_TRACE_ID_SIZE 16                 /**< Trace ID size (128-bit) */
#define RO_SPAN_ID_SIZE 8                   /**< Span ID size (64-bit) */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Metric types
 */
typedef enum {
    RO_METRIC_COUNTER = 0,      /**< Monotonically increasing */
    RO_METRIC_GAUGE,            /**< Point-in-time value */
    RO_METRIC_HISTOGRAM,        /**< Distribution of values */
    RO_METRIC_SUMMARY           /**< Quantile summary */
} ro_metric_type_t;

/**
 * @brief Span status
 */
typedef enum {
    RO_SPAN_UNSET = 0,          /**< Status not set */
    RO_SPAN_OK,                 /**< Operation succeeded */
    RO_SPAN_ERROR               /**< Operation failed */
} ro_span_status_t;

/**
 * @brief Event severity levels
 */
typedef enum {
    RO_SEVERITY_TRACE = 0,      /**< Detailed trace info */
    RO_SEVERITY_DEBUG,          /**< Debug information */
    RO_SEVERITY_INFO,           /**< Informational */
    RO_SEVERITY_WARN,           /**< Warning condition */
    RO_SEVERITY_ERROR,          /**< Error condition */
    RO_SEVERITY_FATAL           /**< Fatal error */
} ro_severity_t;

/**
 * @brief Export formats
 */
typedef enum {
    RO_EXPORT_JSON = 0,         /**< JSON format */
    RO_EXPORT_PROMETHEUS,       /**< Prometheus format */
    RO_EXPORT_OPENTELEMETRY,    /**< OpenTelemetry format */
    RO_EXPORT_CUSTOM            /**< Custom exporter */
} ro_export_format_t;

/**
 * @brief Recovery event types for observability
 */
typedef enum {
    RO_EVENT_FAILURE_DETECTED = 0,
    RO_EVENT_RECOVERY_STARTED,
    RO_EVENT_RECOVERY_SUCCESS,
    RO_EVENT_RECOVERY_FAILED,
    RO_EVENT_CHECKPOINT_CREATED,
    RO_EVENT_CHECKPOINT_RESTORED,
    RO_EVENT_ESCALATION,
    RO_EVENT_DEGRADATION,
    RO_EVENT_RECOVERY_TIMEOUT,
    RO_EVENT_RESOURCE_EXHAUSTED
} ro_event_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Metric label (key-value pair)
 */
typedef struct {
    char key[32];               /**< Label key */
    char value[64];             /**< Label value */
} ro_label_t;

/**
 * @brief Counter metric
 */
typedef struct {
    char name[64];              /**< Metric name */
    uint64_t value;             /**< Current count */
    ro_label_t labels[RO_MAX_LABELS]; /**< Labels */
    uint32_t label_count;       /**< Number of labels */
} ro_counter_t;

/**
 * @brief Gauge metric
 */
typedef struct {
    char name[64];              /**< Metric name */
    double value;               /**< Current value */
    ro_label_t labels[RO_MAX_LABELS]; /**< Labels */
    uint32_t label_count;       /**< Number of labels */
} ro_gauge_t;

/**
 * @brief Histogram bucket
 */
typedef struct {
    double upper_bound;         /**< Bucket upper bound */
    uint64_t count;             /**< Count in bucket */
} ro_histogram_bucket_t;

/**
 * @brief Histogram metric
 */
typedef struct {
    char name[64];              /**< Metric name */
    ro_histogram_bucket_t buckets[RO_HISTOGRAM_BUCKETS]; /**< Buckets */
    uint32_t bucket_count;      /**< Number of buckets */
    uint64_t sample_count;      /**< Total samples */
    double sample_sum;          /**< Sum of all samples */
    double min;                 /**< Minimum value */
    double max;                 /**< Maximum value */
    ro_label_t labels[RO_MAX_LABELS]; /**< Labels */
    uint32_t label_count;       /**< Number of labels */
} ro_histogram_t;

/**
 * @brief Trace span
 */
typedef struct {
    uint8_t trace_id[RO_TRACE_ID_SIZE]; /**< Trace identifier */
    uint8_t span_id[RO_SPAN_ID_SIZE];   /**< Span identifier */
    uint8_t parent_span_id[RO_SPAN_ID_SIZE]; /**< Parent span */
    char name[64];                      /**< Span name */
    uint64_t start_time_ns;             /**< Start timestamp */
    uint64_t end_time_ns;               /**< End timestamp */
    ro_span_status_t status;            /**< Span status */
    char status_message[128];           /**< Status message */
    ro_label_t attributes[RO_MAX_LABELS]; /**< Span attributes */
    uint32_t attribute_count;           /**< Number of attributes */
    bool is_recording;                  /**< Currently recording */
} ro_span_t;

/**
 * @brief Recovery event
 */
typedef struct {
    ro_event_type_t type;       /**< Event type */
    ro_severity_t severity;     /**< Severity level */
    uint64_t timestamp_ns;      /**< Event timestamp */
    uint32_t node_id;           /**< Source node */
    uint32_t fault_type;        /**< Fault type */
    uint32_t recovery_id;       /**< Recovery operation ID */
    uint64_t duration_ms;       /**< Duration (if applicable) */
    bool success;               /**< Success flag */
    char message[256];          /**< Event message */
    ro_label_t labels[RO_MAX_LABELS]; /**< Event labels */
    uint32_t label_count;       /**< Number of labels */
} ro_event_t;

/**
 * @brief MTTR (Mean Time To Recovery) statistics
 */
typedef struct {
    uint64_t total_recoveries;      /**< Total recovery count */
    uint64_t successful_recoveries; /**< Successful recoveries */
    uint64_t failed_recoveries;     /**< Failed recoveries */
    double mttr_ms;                 /**< Mean time to recovery */
    double mttr_p50_ms;             /**< 50th percentile */
    double mttr_p95_ms;             /**< 95th percentile */
    double mttr_p99_ms;             /**< 99th percentile */
    double min_recovery_ms;         /**< Minimum recovery time */
    double max_recovery_ms;         /**< Maximum recovery time */
    double success_rate;            /**< Success rate (0-1) */
} ro_mttr_stats_t;

/**
 * @brief MTBF (Mean Time Between Failures) statistics
 */
typedef struct {
    uint64_t total_failures;        /**< Total failure count */
    double mtbf_ms;                 /**< Mean time between failures */
    double min_tbf_ms;              /**< Minimum TBF */
    double max_tbf_ms;              /**< Maximum TBF */
    uint64_t uptime_ms;             /**< Total uptime */
    double availability;            /**< Availability ratio (0-1) */
} ro_mtbf_stats_t;

/**
 * @brief Recovery operation context
 */
typedef struct {
    uint32_t recovery_id;           /**< Operation ID */
    uint32_t fault_type;            /**< Fault being recovered */
    uint64_t start_time_ns;         /**< Start timestamp */
    uint64_t end_time_ns;           /**< End timestamp (0 if ongoing) */
    uint32_t attempt_count;         /**< Recovery attempts */
    bool is_complete;               /**< Recovery complete */
    bool success;                   /**< Recovery succeeded */
    float resource_usage[8];        /**< Resource usage during recovery */
    ro_span_t root_span;            /**< Root trace span */
} ro_recovery_context_t;

/**
 * @brief Exporter callback
 */
typedef bool (*ro_export_callback_t)(
    const void* data,
    size_t data_size,
    ro_export_format_t format,
    void* user_data
);

/**
 * @brief Configuration for observability
 */
typedef struct {
    bool enable_tracing;            /**< Enable distributed tracing */
    bool enable_metrics;            /**< Enable metrics collection */
    bool enable_events;             /**< Enable event logging */
    uint32_t event_buffer_size;     /**< Event buffer size */
    uint32_t metrics_interval_ms;   /**< Metrics collection interval */
    uint32_t trace_sample_rate;     /**< Trace sampling (1 in N) */
    ro_export_format_t export_format; /**< Default export format */
    uint64_t retention_ms;          /**< Data retention period */
} ro_config_t;

/**
 * @brief Opaque observability handle
 */
typedef struct ro_context ro_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create observability context
 *
 * WHAT: Initialize observability system
 * WHY:  Required before any metrics/tracing
 * HOW:  Allocate context, initialize buffers
 *
 * @param config Configuration
 * @return RO context or NULL on failure
 */
ro_context_t* ro_create(const ro_config_t* config);

/**
 * @brief Destroy observability context
 *
 * @param ctx RO context
 */
void ro_destroy(ro_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
ro_config_t ro_default_config(void);

/**
 * @brief Start observability collection
 *
 * @param ctx RO context
 * @return true on success
 */
bool ro_start(ro_context_t* ctx);

/**
 * @brief Stop observability collection
 *
 * @param ctx RO context
 * @return true on success
 */
bool ro_stop(ro_context_t* ctx);

//=============================================================================
// Counter Operations
//=============================================================================

/**
 * @brief Create counter metric
 *
 * @param ctx RO context
 * @param name Counter name
 * @param labels Labels array
 * @param label_count Number of labels
 * @return Counter handle, NULL on failure
 */
ro_counter_t* ro_create_counter(
    ro_context_t* ctx,
    const char* name,
    const ro_label_t* labels,
    uint32_t label_count
);

/**
 * @brief Increment counter
 *
 * @param counter Counter to increment
 * @param delta Amount to add (must be positive)
 * @return New value
 */
uint64_t ro_counter_inc(ro_counter_t* counter, uint64_t delta);

/**
 * @brief Get counter value
 *
 * @param counter Counter
 * @return Current value
 */
uint64_t ro_counter_get(const ro_counter_t* counter);

//=============================================================================
// Gauge Operations
//=============================================================================

/**
 * @brief Create gauge metric
 *
 * @param ctx RO context
 * @param name Gauge name
 * @param labels Labels array
 * @param label_count Number of labels
 * @return Gauge handle, NULL on failure
 */
ro_gauge_t* ro_create_gauge(
    ro_context_t* ctx,
    const char* name,
    const ro_label_t* labels,
    uint32_t label_count
);

/**
 * @brief Set gauge value
 *
 * @param gauge Gauge to set
 * @param value New value
 */
void ro_gauge_set(ro_gauge_t* gauge, double value);

/**
 * @brief Increment gauge
 *
 * @param gauge Gauge to increment
 * @param delta Amount to add
 */
void ro_gauge_inc(ro_gauge_t* gauge, double delta);

/**
 * @brief Decrement gauge
 *
 * @param gauge Gauge to decrement
 * @param delta Amount to subtract
 */
void ro_gauge_dec(ro_gauge_t* gauge, double delta);

/**
 * @brief Get gauge value
 *
 * @param gauge Gauge
 * @return Current value
 */
double ro_gauge_get(const ro_gauge_t* gauge);

//=============================================================================
// Histogram Operations
//=============================================================================

/**
 * @brief Create histogram metric
 *
 * @param ctx RO context
 * @param name Histogram name
 * @param buckets Bucket upper bounds
 * @param bucket_count Number of buckets
 * @param labels Labels array
 * @param label_count Number of labels
 * @return Histogram handle, NULL on failure
 */
ro_histogram_t* ro_create_histogram(
    ro_context_t* ctx,
    const char* name,
    const double* buckets,
    uint32_t bucket_count,
    const ro_label_t* labels,
    uint32_t label_count
);

/**
 * @brief Record histogram observation
 *
 * @param hist Histogram
 * @param value Value to record
 */
void ro_histogram_observe(ro_histogram_t* hist, double value);

/**
 * @brief Get histogram percentile
 *
 * @param hist Histogram
 * @param percentile Percentile (0-1)
 * @return Percentile value
 */
double ro_histogram_percentile(const ro_histogram_t* hist, double percentile);

/**
 * @brief Get histogram mean
 *
 * @param hist Histogram
 * @return Mean value
 */
double ro_histogram_mean(const ro_histogram_t* hist);

//=============================================================================
// Tracing Operations
//=============================================================================

/**
 * @brief Start new trace
 *
 * @param ctx RO context
 * @param name Trace name
 * @return Root span
 */
ro_span_t* ro_start_trace(ro_context_t* ctx, const char* name);

/**
 * @brief Start child span
 *
 * @param ctx RO context
 * @param parent Parent span
 * @param name Span name
 * @return Child span
 */
ro_span_t* ro_start_span(ro_context_t* ctx, const ro_span_t* parent, const char* name);

/**
 * @brief End span
 *
 * @param span Span to end
 * @param status Span status
 * @param message Status message (optional)
 */
void ro_end_span(ro_span_t* span, ro_span_status_t status, const char* message);

/**
 * @brief Add span attribute
 *
 * @param span Span
 * @param key Attribute key
 * @param value Attribute value
 */
void ro_span_set_attribute(ro_span_t* span, const char* key, const char* value);

/**
 * @brief Add span event
 *
 * @param span Span
 * @param name Event name
 */
void ro_span_add_event(ro_span_t* span, const char* name);

/**
 * @brief Get span duration
 *
 * @param span Span
 * @return Duration in nanoseconds
 */
uint64_t ro_span_duration_ns(const ro_span_t* span);

//=============================================================================
// Recovery-Specific Metrics
//=============================================================================

/**
 * @brief Start recovery observation
 *
 * @param ctx RO context
 * @param fault_type Fault type
 * @return Recovery context
 */
ro_recovery_context_t* ro_start_recovery(ro_context_t* ctx, uint32_t fault_type);

/**
 * @brief End recovery observation
 *
 * @param ctx RO context
 * @param recovery Recovery context
 * @param success Recovery succeeded
 */
void ro_end_recovery(ro_context_t* ctx, ro_recovery_context_t* recovery, bool success);

/**
 * @brief Record recovery attempt
 *
 * @param recovery Recovery context
 * @param strategy Strategy used
 * @param success Attempt succeeded
 */
void ro_record_recovery_attempt(ro_recovery_context_t* recovery, const char* strategy, bool success);

/**
 * @brief Get MTTR statistics
 *
 * @param ctx RO context
 * @param stats Output statistics
 * @return true on success
 */
bool ro_get_mttr_stats(ro_context_t* ctx, ro_mttr_stats_t* stats);

/**
 * @brief Get MTBF statistics
 *
 * @param ctx RO context
 * @param stats Output statistics
 * @return true on success
 */
bool ro_get_mtbf_stats(ro_context_t* ctx, ro_mtbf_stats_t* stats);

/**
 * @brief Record failure event
 *
 * @param ctx RO context
 * @param node_id Node that failed
 * @param fault_type Type of fault
 */
void ro_record_failure(ro_context_t* ctx, uint32_t node_id, uint32_t fault_type);

//=============================================================================
// Event Logging
//=============================================================================

/**
 * @brief Log recovery event
 *
 * @param ctx RO context
 * @param event Event to log
 * @return true on success
 */
bool ro_log_event(ro_context_t* ctx, const ro_event_t* event);

/**
 * @brief Get recent events
 *
 * @param ctx RO context
 * @param events Output array
 * @param max_events Array capacity
 * @param since_timestamp Only events after this time
 * @return Number of events
 */
uint32_t ro_get_events(
    ro_context_t* ctx,
    ro_event_t* events,
    uint32_t max_events,
    uint64_t since_timestamp
);

/**
 * @brief Get events by type
 *
 * @param ctx RO context
 * @param type Event type to filter
 * @param events Output array
 * @param max_events Array capacity
 * @return Number of events
 */
uint32_t ro_get_events_by_type(
    ro_context_t* ctx,
    ro_event_type_t type,
    ro_event_t* events,
    uint32_t max_events
);

//=============================================================================
// Export Operations
//=============================================================================

/**
 * @brief Export metrics to buffer
 *
 * @param ctx RO context
 * @param format Export format
 * @param buffer Output buffer
 * @param buffer_size Buffer capacity
 * @return Bytes written
 */
size_t ro_export_metrics(
    ro_context_t* ctx,
    ro_export_format_t format,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Export traces to buffer
 *
 * @param ctx RO context
 * @param format Export format
 * @param buffer Output buffer
 * @param buffer_size Buffer capacity
 * @return Bytes written
 */
size_t ro_export_traces(
    ro_context_t* ctx,
    ro_export_format_t format,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Register export callback
 *
 * @param ctx RO context
 * @param callback Export callback
 * @param format Export format
 * @param user_data User data
 * @return true on success
 */
bool ro_register_exporter(
    ro_context_t* ctx,
    ro_export_callback_t callback,
    ro_export_format_t format,
    void* user_data
);

/**
 * @brief Flush to exporters
 *
 * @param ctx RO context
 * @return true on success
 */
bool ro_flush(ro_context_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Generate trace ID
 *
 * @param trace_id Output trace ID buffer
 */
void ro_generate_trace_id(uint8_t* trace_id);

/**
 * @brief Generate span ID
 *
 * @param span_id Output span ID buffer
 */
void ro_generate_span_id(uint8_t* span_id);

/**
 * @brief Get current timestamp
 *
 * @return Nanoseconds since epoch
 */
uint64_t ro_timestamp_ns(void);

/**
 * @brief Reset all metrics
 *
 * @param ctx RO context
 */
void ro_reset_metrics(ro_context_t* ctx);

//=============================================================================
// String Conversion
//=============================================================================

const char* ro_metric_type_to_string(ro_metric_type_t type);
const char* ro_span_status_to_string(ro_span_status_t status);
const char* ro_severity_to_string(ro_severity_t severity);
const char* ro_event_type_to_string(ro_event_type_t type);
const char* ro_export_format_to_string(ro_export_format_t format);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RECOVERY_OBSERVABILITY_H
