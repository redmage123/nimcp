/**
 * @file nimcp_kg_observability.h
 * @brief Observability Dashboard for Brain Knowledge Graph
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Prometheus metrics, health checks, distributed tracing, and alerting
 * WHY:  Production-grade observability for monitoring brain KG state and performance
 * HOW:  Integrates with standard observability tools (Prometheus, OTLP, Alertmanager)
 *
 * OBSERVABILITY COMPONENTS:
 * ```
 * +============================================================================+
 * |                     BRAIN KG OBSERVABILITY DASHBOARD                       |
 * +============================================================================+
 * |                                                                            |
 * |   METRICS (Prometheus)                                                     |
 * |   ---------------------                                                    |
 * |   - Counter:    Monotonically increasing values (requests, errors)        |
 * |   - Gauge:      Values that go up/down (active connections, memory)       |
 * |   - Histogram:  Distribution of values (latencies, sizes)                 |
 * |   - Summary:    Quantiles over sliding time window                        |
 * |                                                                            |
 * |   HEALTH CHECKS                                                            |
 * |   --------------                                                           |
 * |   - Liveness:   Is the service running? (/healthz)                        |
 * |   - Readiness:  Is the service ready to accept traffic? (/readyz)         |
 * |   - Component:  Individual component health status                        |
 * |                                                                            |
 * |   DISTRIBUTED TRACING (OpenTelemetry)                                      |
 * |   ------------------------------------                                     |
 * |   - Trace ID:   128-bit unique identifier for request flow                |
 * |   - Span:       Individual operation within a trace                       |
 * |   - Tags:       Key-value metadata on spans                               |
 * |                                                                            |
 * |   ALERTING (Alertmanager)                                                  |
 * |   ------------------------                                                 |
 * |   - Severity:   critical, warning, info                                   |
 * |   - Routing:    Alert routing to appropriate channels                     |
 * |                                                                            |
 * +============================================================================+
 * ```
 *
 * THREAD SAFETY: All operations are thread-safe
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_OBSERVABILITY_H
#define NIMCP_KG_OBSERVABILITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum metric name length */
#define KG_OBS_MAX_NAME_LEN         64

/** Maximum help text length */
#define KG_OBS_MAX_HELP_LEN         256

/** Maximum component name length */
#define KG_OBS_MAX_COMPONENT_LEN    64

/** Maximum message/description length */
#define KG_OBS_MAX_MESSAGE_LEN      256

/** Maximum path length */
#define KG_OBS_MAX_PATH_LEN         64

/** Maximum URL length */
#define KG_OBS_MAX_URL_LEN          256

/** Trace ID length (128-bit as hex + null) */
#define KG_OBS_TRACE_ID_LEN         33

/** Span ID length (64-bit as hex + null) */
#define KG_OBS_SPAN_ID_LEN          17

/** Operation name length */
#define KG_OBS_MAX_OPERATION_LEN    64

/** Maximum label count per metric */
#define KG_OBS_MAX_LABELS           16

/** Maximum registered metrics */
#define KG_OBS_MAX_METRICS          256

/** Maximum health check components */
#define KG_OBS_MAX_HEALTH_CHECKS    64

/** Default Prometheus port */
#define KG_OBS_DEFAULT_PROMETHEUS_PORT  9090

/** Default health check port */
#define KG_OBS_DEFAULT_HEALTH_PORT      8080

/** Default trace sample rate */
#define KG_OBS_DEFAULT_TRACE_SAMPLE_RATE  0.1f

/* ============================================================================
 * Metric Type Enumeration
 * ============================================================================ */

/**
 * @brief Prometheus metric types
 *
 * WHAT: Standard Prometheus metric type identifiers
 * WHY:  Enable proper metric exposition and aggregation
 * HOW:  Maps to Prometheus exposition format TYPE declaration
 */
typedef enum {
    KG_METRIC_COUNTER = 0,      /**< Monotonically increasing (e.g., requests, errors) */
    KG_METRIC_GAUGE,            /**< Can go up or down (e.g., active connections) */
    KG_METRIC_HISTOGRAM,        /**< Distribution of values (e.g., latencies) */
    KG_METRIC_SUMMARY           /**< Quantiles over time (e.g., p50, p95, p99) */
} kg_metric_type_t;

/* ============================================================================
 * Metric Definition
 * ============================================================================ */

/**
 * @brief Metric definition structure
 *
 * WHAT: Complete definition of a Prometheus metric
 * WHY:  Enable registration and exposition of metrics
 * HOW:  Contains name, help text, type, and label schema
 */
typedef struct {
    char name[KG_OBS_MAX_NAME_LEN];      /**< Metric name (e.g., "kg_nodes_total") */
    char help[KG_OBS_MAX_HELP_LEN];      /**< Description/help text */
    kg_metric_type_t type;               /**< Metric type */
    char** label_names;                  /**< Label names array (NULL-terminated) */
    uint32_t label_count;                /**< Number of labels */
} kg_metric_def_t;

/* ============================================================================
 * Health Check Result
 * ============================================================================ */

/**
 * @brief Health check result structure
 *
 * WHAT: Result of a component health check
 * WHY:  Enable liveness/readiness probes and component monitoring
 * HOW:  Contains health status, latency, and diagnostic message
 */
typedef struct {
    char component[KG_OBS_MAX_COMPONENT_LEN];  /**< Component name */
    bool healthy;                              /**< Health status (true = healthy) */
    char message[KG_OBS_MAX_MESSAGE_LEN];      /**< Status message or error description */
    uint64_t latency_ms;                       /**< Check latency in milliseconds */
    uint64_t last_check;                       /**< Last check timestamp (ms since epoch) */
} kg_health_result_t;

/**
 * @brief Health check callback function type
 *
 * WHAT: User-defined function to check component health
 * WHY:  Allow custom health check logic per component
 * HOW:  Called periodically or on-demand, returns health status
 *
 * @param component Component name being checked
 * @param user_data User-provided context
 * @param result Output: health check result
 * @return 0 on success, -1 on error executing check
 */
typedef int (*kg_health_check_fn)(
    const char* component,
    void* user_data,
    kg_health_result_t* result
);

/* ============================================================================
 * Tracing Span
 * ============================================================================ */

/**
 * @brief Distributed tracing span structure
 *
 * WHAT: Represents a single operation in a distributed trace
 * WHY:  Enable request flow tracking across components
 * HOW:  Contains trace context, timing, and metadata
 *
 * TRACE CONTEXT:
 * - trace_id:       128-bit unique identifier for entire request flow
 * - span_id:        64-bit unique identifier for this operation
 * - parent_span_id: Links to parent span (empty for root spans)
 */
typedef struct {
    char trace_id[KG_OBS_TRACE_ID_LEN];        /**< 128-bit trace ID as hex string */
    char span_id[KG_OBS_SPAN_ID_LEN];          /**< 64-bit span ID as hex string */
    char parent_span_id[KG_OBS_SPAN_ID_LEN];   /**< Parent span ID (empty if root) */
    char operation[KG_OBS_MAX_OPERATION_LEN];  /**< Operation name */
    uint64_t start_time_ns;                    /**< Start timestamp (nanoseconds) */
    uint64_t duration_ns;                      /**< Duration (set on span end) */
    char** tags;                               /**< Key-value tags array */
    uint32_t tag_count;                        /**< Number of tags */
} kg_trace_span_t;

/* ============================================================================
 * Observability Configuration
 * ============================================================================ */

/**
 * @brief Observability configuration structure
 *
 * WHAT: Configuration for all observability features
 * WHY:  Enable flexible deployment configuration
 * HOW:  Controls Prometheus, health endpoints, tracing, and alerting
 */
typedef struct {
    /* Prometheus metrics */
    bool enable_prometheus;                    /**< Enable Prometheus metrics endpoint */
    uint16_t prometheus_port;                  /**< Prometheus port (default: 9090) */
    char prometheus_path[KG_OBS_MAX_PATH_LEN]; /**< Metrics path (default: /metrics) */

    /* Health endpoints */
    bool enable_health_endpoints;              /**< Enable health check endpoints */
    uint16_t health_port;                      /**< Health check port (default: 8080) */
    char liveness_path[KG_OBS_MAX_PATH_LEN];   /**< Liveness path (default: /healthz) */
    char readiness_path[KG_OBS_MAX_PATH_LEN];  /**< Readiness path (default: /readyz) */

    /* Distributed tracing */
    bool enable_tracing;                       /**< Enable distributed tracing */
    char otlp_endpoint[KG_OBS_MAX_URL_LEN];    /**< OpenTelemetry collector endpoint */
    float trace_sample_rate;                   /**< Sample rate 0.0-1.0 (default: 0.1) */

    /* Alerting */
    bool enable_alerting;                      /**< Enable alerting */
    char alertmanager_url[KG_OBS_MAX_URL_LEN]; /**< Alertmanager endpoint URL */
} kg_observability_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Observability handle (opaque)
 */
typedef struct kg_observability kg_observability_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default observability configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Simplify common deployment scenarios
 * HOW:  Sets default ports, paths, and sampling rates
 *
 * Default values:
 * - prometheus_port: 9090
 * - prometheus_path: "/metrics"
 * - health_port: 8080
 * - liveness_path: "/healthz"
 * - readiness_path: "/readyz"
 * - trace_sample_rate: 0.1
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int kg_observability_default_config(kg_observability_config_t* config);

/**
 * @brief Create observability instance
 *
 * WHAT: Initialize observability subsystem
 * WHY:  Enable metrics, health checks, tracing, and alerting
 * HOW:  Start configured endpoints and initialize collectors
 *
 * @param config Configuration (NULL for defaults)
 * @return Observability handle or NULL on error
 */
kg_observability_t* kg_observability_create(const kg_observability_config_t* config);

/**
 * @brief Destroy observability instance
 *
 * WHAT: Clean up observability resources
 * WHY:  Proper resource cleanup on shutdown
 * HOW:  Stop endpoints, flush pending data, free memory
 *
 * @param obs Observability handle (NULL safe)
 */
void kg_observability_destroy(kg_observability_t* obs);

/**
 * @brief Start observability endpoints
 *
 * WHAT: Start HTTP endpoints for metrics and health
 * WHY:  Begin serving observability data
 * HOW:  Bind ports and start listener threads
 *
 * @param obs Observability handle
 * @return 0 on success, -1 on error
 */
int kg_observability_start(kg_observability_t* obs);

/**
 * @brief Stop observability endpoints
 *
 * WHAT: Stop HTTP endpoints gracefully
 * WHY:  Prepare for shutdown
 * HOW:  Stop listeners, drain connections
 *
 * @param obs Observability handle
 * @return 0 on success, -1 on error
 */
int kg_observability_stop(kg_observability_t* obs);

/* ============================================================================
 * Metrics API
 * ============================================================================ */

/**
 * @brief Register a new metric
 *
 * WHAT: Define a new metric for collection
 * WHY:  Metrics must be registered before use
 * HOW:  Stores metric definition for exposition
 *
 * @param obs Observability handle
 * @param def Metric definition
 * @return 0 on success, -1 on error, -2 if metric already exists
 */
int kg_obs_register_metric(kg_observability_t* obs, const kg_metric_def_t* def);

/**
 * @brief Increment a counter metric
 *
 * WHAT: Add value to a counter metric
 * WHY:  Track cumulative counts (requests, errors)
 * HOW:  Atomically increments counter by value
 *
 * @param obs Observability handle
 * @param name Metric name
 * @param value Value to add (must be >= 0)
 * @param labels Label values (NULL if no labels)
 * @return 0 on success, -1 on error
 */
int kg_obs_counter_inc(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
);

/**
 * @brief Set a gauge metric value
 *
 * WHAT: Set absolute value of a gauge metric
 * WHY:  Track current state (connections, memory usage)
 * HOW:  Atomically sets gauge to specified value
 *
 * @param obs Observability handle
 * @param name Metric name
 * @param value New gauge value
 * @param labels Label values (NULL if no labels)
 * @return 0 on success, -1 on error
 */
int kg_obs_gauge_set(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
);

/**
 * @brief Increment a gauge metric
 *
 * WHAT: Add value to a gauge metric
 * WHY:  Track relative changes
 * HOW:  Atomically adds value to gauge
 *
 * @param obs Observability handle
 * @param name Metric name
 * @param value Value to add (can be negative)
 * @param labels Label values (NULL if no labels)
 * @return 0 on success, -1 on error
 */
int kg_obs_gauge_inc(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
);

/**
 * @brief Observe a histogram value
 *
 * WHAT: Record an observation in a histogram
 * WHY:  Track value distributions (latencies, sizes)
 * HOW:  Updates bucket counts and sum
 *
 * @param obs Observability handle
 * @param name Metric name
 * @param value Observed value
 * @param labels Label values (NULL if no labels)
 * @return 0 on success, -1 on error
 */
int kg_obs_histogram_observe(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
);

/**
 * @brief Observe a summary value
 *
 * WHAT: Record an observation in a summary
 * WHY:  Track quantiles over sliding window
 * HOW:  Updates streaming quantile estimator
 *
 * @param obs Observability handle
 * @param name Metric name
 * @param value Observed value
 * @param labels Label values (NULL if no labels)
 * @return 0 on success, -1 on error
 */
int kg_obs_summary_observe(
    kg_observability_t* obs,
    const char* name,
    double value,
    const char** labels
);

/**
 * @brief Get current metric value
 *
 * WHAT: Retrieve current value of a metric
 * WHY:  Allow programmatic access to metrics
 * HOW:  Returns current counter/gauge value
 *
 * @param obs Observability handle
 * @param name Metric name
 * @param labels Label values (NULL if no labels)
 * @param value Output: current value
 * @return 0 on success, -1 on error
 */
int kg_obs_get_metric(
    const kg_observability_t* obs,
    const char* name,
    const char** labels,
    double* value
);

/**
 * @brief Unregister a metric
 *
 * WHAT: Remove a metric from collection
 * WHY:  Clean up dynamic metrics
 * HOW:  Removes metric definition and data
 *
 * @param obs Observability handle
 * @param name Metric name to remove
 * @return 0 on success, -1 on error
 */
int kg_obs_unregister_metric(kg_observability_t* obs, const char* name);

/* ============================================================================
 * Health Check API
 * ============================================================================ */

/**
 * @brief Register a health check component
 *
 * WHAT: Register a component for health monitoring
 * WHY:  Enable liveness/readiness probing of components
 * HOW:  Stores check function for periodic/on-demand invocation
 *
 * @param obs Observability handle
 * @param component Component name
 * @param check_fn Health check callback function
 * @param user_data User context passed to callback
 * @return 0 on success, -1 on error
 */
int kg_obs_register_health_check(
    kg_observability_t* obs,
    const char* component,
    kg_health_check_fn check_fn,
    void* user_data
);

/**
 * @brief Unregister a health check component
 *
 * WHAT: Remove a component from health monitoring
 * WHY:  Clean up when components are removed
 * HOW:  Removes check function registration
 *
 * @param obs Observability handle
 * @param component Component name to remove
 * @return 0 on success, -1 if not found
 */
int kg_obs_unregister_health_check(
    kg_observability_t* obs,
    const char* component
);

/**
 * @brief Get health status of all components
 *
 * WHAT: Execute all health checks and return results
 * WHY:  Aggregate health status for readiness
 * HOW:  Invokes all registered check functions
 *
 * @param obs Observability handle
 * @param results Output array (caller allocated)
 * @param count Input: array capacity, Output: number of results
 * @return 0 on success, -1 on error
 */
int kg_obs_get_health(
    const kg_observability_t* obs,
    kg_health_result_t* results,
    uint32_t* count
);

/**
 * @brief Get health status of a specific component
 *
 * WHAT: Execute health check for one component
 * WHY:  Check individual component health
 * HOW:  Invokes registered check function
 *
 * @param obs Observability handle
 * @param component Component name
 * @param result Output health result
 * @return 0 on success, -1 if not found
 */
int kg_obs_get_component_health(
    const kg_observability_t* obs,
    const char* component,
    kg_health_result_t* result
);

/**
 * @brief Check if system is live (liveness probe)
 *
 * WHAT: Quick check if system is alive
 * WHY:  Kubernetes liveness probe support
 * HOW:  Returns true if basic health checks pass
 *
 * @param obs Observability handle
 * @return true if live, false otherwise
 */
bool kg_obs_is_live(const kg_observability_t* obs);

/**
 * @brief Check if system is ready (readiness probe)
 *
 * WHAT: Check if system is ready to accept traffic
 * WHY:  Kubernetes readiness probe support
 * HOW:  Returns true if all components are healthy
 *
 * @param obs Observability handle
 * @return true if ready, false otherwise
 */
bool kg_obs_is_ready(const kg_observability_t* obs);

/* ============================================================================
 * Tracing API
 * ============================================================================ */

/**
 * @brief Start a new trace span
 *
 * WHAT: Begin a new tracing span for an operation
 * WHY:  Track operation timing and context
 * HOW:  Creates span with trace context, starts timer
 *
 * @param obs Observability handle
 * @param operation Operation name
 * @param parent Parent span (NULL for root span)
 * @return New span or NULL on error (caller must free)
 */
kg_trace_span_t* kg_obs_start_span(
    kg_observability_t* obs,
    const char* operation,
    const kg_trace_span_t* parent
);

/**
 * @brief End a trace span
 *
 * WHAT: Complete a tracing span
 * WHY:  Record duration and export span
 * HOW:  Stops timer, sends to collector
 *
 * @param obs Observability handle
 * @param span Span to end
 * @return 0 on success, -1 on error
 */
int kg_obs_end_span(kg_observability_t* obs, kg_trace_span_t* span);

/**
 * @brief Add a tag to a trace span
 *
 * WHAT: Add key-value metadata to a span
 * WHY:  Attach context information
 * HOW:  Appends tag to span's tag array
 *
 * @param span Span to modify
 * @param key Tag key
 * @param value Tag value
 * @return 0 on success, -1 on error
 */
int kg_obs_add_span_tag(kg_trace_span_t* span, const char* key, const char* value);

/**
 * @brief Add an event/log to a trace span
 *
 * WHAT: Record an event within a span
 * WHY:  Capture significant events during operation
 * HOW:  Adds timestamped event to span
 *
 * @param obs Observability handle
 * @param span Span to add event to
 * @param event_name Event name
 * @return 0 on success, -1 on error
 */
int kg_obs_add_span_event(
    kg_observability_t* obs,
    kg_trace_span_t* span,
    const char* event_name
);

/**
 * @brief Set span status
 *
 * WHAT: Mark span as OK or error
 * WHY:  Indicate operation success/failure
 * HOW:  Sets status code and optional message
 *
 * @param span Span to modify
 * @param is_error True if operation failed
 * @param message Error message (NULL for OK status)
 * @return 0 on success, -1 on error
 */
int kg_obs_set_span_status(
    kg_trace_span_t* span,
    bool is_error,
    const char* message
);

/**
 * @brief Free a trace span
 *
 * WHAT: Release span resources
 * WHY:  Clean up after span is ended
 * HOW:  Frees tags and span structure
 *
 * @param span Span to free (NULL safe)
 */
void kg_obs_free_span(kg_trace_span_t* span);

/**
 * @brief Extract trace context from span
 *
 * WHAT: Get trace/span IDs for propagation
 * WHY:  Enable distributed trace context propagation
 * HOW:  Returns trace_id and span_id strings
 *
 * @param span Source span
 * @param trace_id Output: trace ID buffer (size KG_OBS_TRACE_ID_LEN)
 * @param span_id Output: span ID buffer (size KG_OBS_SPAN_ID_LEN)
 * @return 0 on success, -1 on error
 */
int kg_obs_extract_trace_context(
    const kg_trace_span_t* span,
    char* trace_id,
    char* span_id
);

/**
 * @brief Create span from propagated trace context
 *
 * WHAT: Create child span from external trace context
 * WHY:  Continue trace from incoming request
 * HOW:  Creates span with provided trace/parent IDs
 *
 * @param obs Observability handle
 * @param operation Operation name
 * @param trace_id Propagated trace ID
 * @param parent_span_id Propagated parent span ID
 * @return New span or NULL on error
 */
kg_trace_span_t* kg_obs_create_span_from_context(
    kg_observability_t* obs,
    const char* operation,
    const char* trace_id,
    const char* parent_span_id
);

/* ============================================================================
 * Alerting API
 * ============================================================================ */

/**
 * @brief Send an alert
 *
 * WHAT: Send alert to Alertmanager
 * WHY:  Notify operators of issues
 * HOW:  Posts alert to configured Alertmanager endpoint
 *
 * Alert severities:
 * - "critical": Immediate action required
 * - "warning":  Investigation needed
 * - "info":     Informational notification
 *
 * @param obs Observability handle
 * @param alert_name Alert name/identifier
 * @param severity Alert severity ("critical", "warning", "info")
 * @param message Alert message/description
 * @return 0 on success, -1 on error
 */
int kg_obs_send_alert(
    kg_observability_t* obs,
    const char* alert_name,
    const char* severity,
    const char* message
);

/**
 * @brief Send alert with labels
 *
 * WHAT: Send alert with additional context labels
 * WHY:  Enable alert routing and grouping
 * HOW:  Posts alert with labels to Alertmanager
 *
 * @param obs Observability handle
 * @param alert_name Alert name/identifier
 * @param severity Alert severity
 * @param message Alert message/description
 * @param label_keys Label key array
 * @param label_values Label value array
 * @param label_count Number of labels
 * @return 0 on success, -1 on error
 */
int kg_obs_send_alert_with_labels(
    kg_observability_t* obs,
    const char* alert_name,
    const char* severity,
    const char* message,
    const char** label_keys,
    const char** label_values,
    uint32_t label_count
);

/**
 * @brief Resolve an alert
 *
 * WHAT: Mark an alert as resolved
 * WHY:  Clear alert when issue is fixed
 * HOW:  Sends resolution to Alertmanager
 *
 * @param obs Observability handle
 * @param alert_name Alert name to resolve
 * @return 0 on success, -1 on error
 */
int kg_obs_resolve_alert(kg_observability_t* obs, const char* alert_name);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert metric type to string
 *
 * @param type Metric type
 * @return String representation (e.g., "counter", "gauge")
 */
const char* kg_metric_type_to_string(kg_metric_type_t type);

/**
 * @brief Parse metric type from string
 *
 * @param str String to parse
 * @return Metric type, or -1 if invalid
 */
int kg_metric_type_from_string(const char* str);

/**
 * @brief Get current timestamp in nanoseconds
 *
 * WHAT: High-resolution timestamp for tracing
 * WHY:  Accurate span timing
 * HOW:  Uses monotonic clock
 *
 * @return Nanoseconds since unspecified epoch
 */
uint64_t kg_obs_timestamp_ns(void);

/**
 * @brief Get current timestamp in milliseconds
 *
 * WHAT: Millisecond timestamp for health checks
 * WHY:  Latency measurement
 * HOW:  Uses monotonic clock
 *
 * @return Milliseconds since unspecified epoch
 */
uint64_t kg_obs_timestamp_ms(void);

/**
 * @brief Generate a new trace ID
 *
 * WHAT: Generate random 128-bit trace ID
 * WHY:  Create new trace roots
 * HOW:  Cryptographically secure random
 *
 * @param trace_id Output buffer (size KG_OBS_TRACE_ID_LEN)
 * @return 0 on success, -1 on error
 */
int kg_obs_generate_trace_id(char* trace_id);

/**
 * @brief Generate a new span ID
 *
 * WHAT: Generate random 64-bit span ID
 * WHY:  Create unique span identifiers
 * HOW:  Cryptographically secure random
 *
 * @param span_id Output buffer (size KG_OBS_SPAN_ID_LEN)
 * @return 0 on success, -1 on error
 */
int kg_obs_generate_span_id(char* span_id);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_OBSERVABILITY_H */
