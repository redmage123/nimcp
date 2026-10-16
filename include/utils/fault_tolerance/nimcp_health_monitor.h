/**
 * @file nimcp_health_monitor.h
 * @brief NIMCP Runtime Health Monitoring and Anomaly Detection System
 * @version 1.0.0
 * @date 2025-11-19
 *
 * Comprehensive runtime health monitoring with:
 * - Real-time metrics collection (memory, CPU, latency, errors)
 * - Baseline establishment for normal operation profiles
 * - Statistical anomaly detection (Z-score, moving averages, change points)
 * - Predictive failure detection using leading indicators
 * - Health scoring (0-100) with actionable thresholds
 * - Integration with NIMCP metrics and diagnostics systems
 */

#ifndef NIMCP_HEALTH_MONITOR_H
#define NIMCP_HEALTH_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HEALTH_MONITOR_MAX_METRICS 256          /**< Max tracked metrics */
#define HEALTH_MONITOR_MAX_OPERATIONS 64        /**< Max operation types */
#define HEALTH_MONITOR_MAX_ERROR_TYPES 32       /**< Max error types */
#define HEALTH_MONITOR_WINDOW_SIZE 100          /**< Moving average window */
#define HEALTH_MONITOR_BASELINE_SAMPLES 1000    /**< Samples for baseline */
#define HEALTH_MONITOR_ANOMALY_THRESHOLD 3.0    /**< Z-score threshold */
#define HEALTH_MONITOR_MAX_ANOMALIES 100        /**< Max detected anomalies */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Health status levels based on score
 */
typedef enum {
    HEALTH_EXCELLENT = 0,       /**< 90-100: Optimal operation */
    HEALTH_GOOD = 1,            /**< 70-89: Normal operation */
    HEALTH_FAIR = 2,            /**< 50-69: Minor issues detected */
    HEALTH_POOR = 3,            /**< 30-49: Significant degradation */
    HEALTH_CRITICAL = 4,        /**< 0-29: Immediate action required */
    HEALTH_UNKNOWN = 5          /**< Insufficient data */
} health_status_t;

/**
 * @brief Anomaly types
 */
typedef enum {
    ANOMALY_NONE = 0,
    ANOMALY_MEMORY_LEAK,        /**< Memory usage growing unbounded */
    ANOMALY_PERFORMANCE_DEGRADATION, /**< Latency increasing over time */
    ANOMALY_ERROR_SPIKE,        /**< Sudden increase in errors */
    ANOMALY_THROUGHPUT_DROP,    /**< Operations/sec decreasing */
    ANOMALY_CACHE_THRASHING,    /**< Low cache hit rate */
    ANOMALY_RESOURCE_EXHAUSTION,/**< Near resource limits */
    ANOMALY_NUMERICAL_INSTABILITY, /**< FPE or numerical errors */
    ANOMALY_THREAD_CONTENTION,  /**< High thread lock contention */
    ANOMALY_UNKNOWN             /**< Unknown anomaly pattern */
} anomaly_type_t;

/**
 * @brief Anomaly severity
 */
typedef enum {
    ANOMALY_SEVERITY_INFO = 0,
    ANOMALY_SEVERITY_WARNING = 1,
    ANOMALY_SEVERITY_ERROR = 2,
    ANOMALY_SEVERITY_CRITICAL = 3
} anomaly_severity_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Statistical metrics for a single measure
 */
typedef struct {
    double mean;                /**< Average value */
    double variance;            /**< Variance */
    double std_dev;             /**< Standard deviation */
    double min;                 /**< Minimum value */
    double max;                 /**< Maximum value */
    uint64_t count;             /**< Sample count */
    double moving_avg;          /**< Exponential moving average */
    double trend;               /**< Trend coefficient */
} metric_stats_t;

/**
 * @brief Operation performance metrics
 */
typedef struct {
    char name[64];              /**< Operation name */
    uint64_t count;             /**< Total operations */
    uint64_t total_duration_us; /**< Total duration (microseconds) */
    uint64_t min_duration_us;   /**< Minimum duration */
    uint64_t max_duration_us;   /**< Maximum duration */
    double avg_duration_us;     /**< Average duration */
    metric_stats_t stats;       /**< Statistical metrics */
    uint64_t last_recorded_us;  /**< Last recorded timestamp */
} operation_metric_t;

/**
 * @brief Memory usage metrics
 */
typedef struct {
    size_t current_bytes;       /**< Current memory usage */
    size_t peak_bytes;          /**< Peak memory usage */
    size_t baseline_bytes;      /**< Baseline (normal) usage */
    double growth_rate;         /**< Bytes/second growth */
    uint64_t allocation_count;  /**< Total allocations */
    uint64_t deallocation_count;/**< Total deallocations */
    metric_stats_t stats;       /**< Statistical metrics */
    uint64_t last_update_us;    /**< Last update timestamp */
} memory_metric_t;

/**
 * @brief Error tracking metrics
 */
typedef struct {
    char type[32];              /**< Error type */
    uint64_t count;             /**< Total occurrences */
    uint64_t count_last_window; /**< Count in last window */
    double rate_per_min;        /**< Errors per minute */
    uint64_t last_occurrence_us;/**< Last occurrence timestamp */
    metric_stats_t stats;       /**< Statistical metrics */
} error_metric_t;

/**
 * @brief Throughput metrics
 */
typedef struct {
    uint64_t total_operations;  /**< Total operations */
    double operations_per_sec;  /**< Current throughput */
    double avg_ops_per_sec;     /**< Average throughput */
    double peak_ops_per_sec;    /**< Peak throughput */
    metric_stats_t stats;       /**< Statistical metrics */
    uint64_t last_update_us;    /**< Last update timestamp */
} throughput_metric_t;

/**
 * @brief Cache performance metrics
 */
typedef struct {
    uint64_t hits;              /**< Cache hits */
    uint64_t misses;            /**< Cache misses */
    double hit_rate;            /**< Hit rate (0-1) */
    double avg_hit_rate;        /**< Average hit rate */
    metric_stats_t stats;       /**< Statistical metrics */
} cache_metric_t;

/**
 * @brief Thread contention metrics
 */
typedef struct {
    uint64_t lock_acquisitions; /**< Total lock acquisitions */
    uint64_t lock_contentions;  /**< Contentions (waits) */
    double contention_rate;     /**< Contention rate (0-1) */
    uint64_t avg_wait_time_us;  /**< Average wait time */
    metric_stats_t stats;       /**< Statistical metrics */
} thread_metric_t;

/**
 * @brief Detected anomaly
 */
typedef struct {
    anomaly_type_t type;        /**< Anomaly type */
    anomaly_severity_t severity;/**< Severity level */
    char description[256];      /**< Human-readable description */
    double confidence;          /**< Confidence (0-1) */
    uint64_t detected_at_us;    /**< Detection timestamp */
    double metric_value;        /**< Metric value at detection */
    double expected_value;      /**< Expected (baseline) value */
    double deviation;           /**< Deviation from baseline */
    char affected_component[64];/**< Affected component */
    bool resolved;              /**< Whether resolved */
} anomaly_t;

/**
 * @brief Complete health status snapshot
 */
typedef struct {
    health_status_t status;     /**< Overall health status */
    float score;                /**< Health score (0-100) */
    uint32_t num_anomalies;     /**< Active anomalies */
    uint64_t timestamp_us;      /**< Status timestamp */

    // Component scores (0-100)
    float memory_score;
    float performance_score;
    float error_score;
    float throughput_score;
    float cache_score;
    float thread_score;

    // Key indicators
    bool memory_leak_detected;
    bool performance_degradation;
    bool error_spike;
    bool resource_exhaustion;

    // Predictions
    bool failure_predicted;
    uint32_t time_to_failure_sec; /**< Predicted time to failure */
} health_status_snapshot_t;

/**
 * @brief Opaque health monitor handle
 */
typedef struct health_monitor_internal* health_monitor_t;

//=============================================================================
// Monitor Lifecycle API
//=============================================================================

/**
 * @brief Create health monitor for a brain instance
 *
 * WHAT: Creates and initializes a health monitoring system
 * WHY: Enable continuous health tracking and anomaly detection
 * HOW: Allocates monitor, initializes metrics, starts baseline collection
 *
 * @param brain_id Unique identifier for the brain instance
 * @return Health monitor handle, NULL on failure
 */
health_monitor_t health_monitor_create(const char* brain_id);

/**
 * @brief Destroy health monitor and free resources
 *
 * WHAT: Destroys monitor and releases all resources
 * WHY: Clean shutdown and memory cleanup
 * HOW: Stops monitoring thread, writes final report, frees memory
 *
 * @param monitor Health monitor handle
 */
void health_monitor_destroy(health_monitor_t monitor);

/**
 * @brief Start health monitoring
 *
 * WHAT: Begins active health monitoring with background thread
 * WHY: Enable continuous real-time monitoring
 * HOW: Spawns monitoring thread, initializes baseline collection
 *
 * @param monitor Health monitor handle
 * @return true on success, false on failure
 */
bool health_monitor_start(health_monitor_t monitor);

/**
 * @brief Stop health monitoring
 *
 * WHAT: Stops active monitoring and background thread
 * WHY: Graceful shutdown or pause monitoring
 * HOW: Signals thread to stop, waits for completion
 *
 * @param monitor Health monitor handle
 * @return true on success, false on failure
 */
bool health_monitor_stop(health_monitor_t monitor);

/**
 * @brief Check if monitor is running
 *
 * @param monitor Health monitor handle
 * @return true if running, false otherwise
 */
bool health_monitor_is_running(health_monitor_t monitor);

//=============================================================================
// Metric Recording API
//=============================================================================

/**
 * @brief Record operation performance
 *
 * WHAT: Records execution time for an operation
 * WHY: Track performance trends and detect degradation
 * HOW: Updates operation metrics, calculates statistics
 *
 * @param monitor Health monitor handle
 * @param operation Operation name (e.g., "inference", "learning")
 * @param duration_us Duration in microseconds
 */
void health_monitor_record_operation(
    health_monitor_t monitor,
    const char* operation,
    uint64_t duration_us
);

/**
 * @brief Record memory usage
 *
 * WHAT: Records current memory usage
 * WHY: Detect memory leaks and resource exhaustion
 * HOW: Updates memory metrics, calculates growth rate
 *
 * @param monitor Health monitor handle
 * @param bytes Current memory usage in bytes
 */
void health_monitor_record_memory(
    health_monitor_t monitor,
    size_t bytes
);

/**
 * @brief Record error occurrence
 *
 * WHAT: Records an error event
 * WHY: Track error rates and detect error spikes
 * HOW: Updates error metrics, calculates rates
 *
 * @param monitor Health monitor handle
 * @param error_type Error type (e.g., "FPE", "NULL_PTR")
 */
void health_monitor_record_error(
    health_monitor_t monitor,
    const char* error_type
);

/**
 * @brief Record cache hit/miss
 *
 * WHAT: Records cache access result
 * WHY: Detect cache thrashing and performance issues
 * HOW: Updates cache metrics, calculates hit rate
 *
 * @param monitor Health monitor handle
 * @param hit true for cache hit, false for miss
 */
void health_monitor_record_cache_access(
    health_monitor_t monitor,
    bool hit
);

/**
 * @brief Record thread lock contention
 *
 * WHAT: Records thread lock acquisition attempt
 * WHY: Detect thread contention and synchronization issues
 * HOW: Updates thread metrics, tracks wait times
 *
 * @param monitor Health monitor handle
 * @param contentious true if lock was contested
 * @param wait_time_us Wait time in microseconds
 */
void health_monitor_record_thread_event(
    health_monitor_t monitor,
    bool contentious,
    uint64_t wait_time_us
);

/**
 * @brief Record throughput sample
 *
 * WHAT: Records operations completed in time window
 * WHY: Track system throughput and detect degradation
 * HOW: Updates throughput metrics, calculates ops/sec
 *
 * @param monitor Health monitor handle
 * @param operations Number of operations completed
 * @param window_us Time window in microseconds
 */
void health_monitor_record_throughput(
    health_monitor_t monitor,
    uint64_t operations,
    uint64_t window_us
);

//=============================================================================
// Health Assessment API
//=============================================================================

/**
 * @brief Get current health status
 *
 * WHAT: Returns complete health status snapshot
 * WHY: Assess overall system health state
 * HOW: Calculates scores, analyzes metrics, generates snapshot
 *
 * @param monitor Health monitor handle
 * @param status Output parameter for health status
 * @return true on success, false on failure
 */
bool health_monitor_get_status(
    health_monitor_t monitor,
    health_status_snapshot_t* status
);

/**
 * @brief Get health score
 *
 * WHAT: Returns overall health score (0-100)
 * WHY: Quick health assessment
 * HOW: Weighted combination of component scores
 *
 * @param monitor Health monitor handle
 * @return Health score (0-100), -1 on error
 */
float health_monitor_get_score(health_monitor_t monitor);

/**
 * @brief Check if system is healthy
 *
 * WHAT: Boolean health check
 * WHY: Quick yes/no health status
 * HOW: Returns true if score >= threshold (70)
 *
 * @param monitor Health monitor handle
 * @return true if healthy, false otherwise
 */
bool health_monitor_is_healthy(health_monitor_t monitor);

/**
 * @brief Get health status enum
 *
 * @param monitor Health monitor handle
 * @return Health status level
 */
health_status_t health_monitor_get_status_level(health_monitor_t monitor);

//=============================================================================
// Anomaly Detection API
//=============================================================================

/**
 * @brief Detect current anomalies
 *
 * WHAT: Analyzes metrics and detects anomalies
 * WHY: Identify abnormal patterns and potential failures
 * HOW: Applies statistical algorithms (Z-score, change detection)
 *
 * @param monitor Health monitor handle
 * @param anomalies Output array for detected anomalies
 * @param max_anomalies Maximum anomalies to return
 * @return Number of anomalies detected, -1 on error
 */
int32_t health_monitor_detect_anomalies(
    health_monitor_t monitor,
    anomaly_t* anomalies,
    uint32_t max_anomalies
);

/**
 * @brief Predict potential failure
 *
 * WHAT: Predicts if failure is imminent
 * WHY: Enable proactive intervention before failure
 * HOW: Analyzes trends and leading indicators
 *
 * @param monitor Health monitor handle
 * @param time_to_failure_sec Output: estimated seconds to failure
 * @return true if failure predicted, false otherwise
 */
bool health_monitor_predict_failure(
    health_monitor_t monitor,
    uint32_t* time_to_failure_sec
);

/**
 * @brief Clear resolved anomalies
 *
 * @param monitor Health monitor handle
 * @return Number of anomalies cleared
 */
uint32_t health_monitor_clear_resolved_anomalies(health_monitor_t monitor);

/**
 * @brief Get anomaly count by type
 *
 * @param monitor Health monitor handle
 * @param type Anomaly type
 * @return Count of active anomalies of this type
 */
uint32_t health_monitor_get_anomaly_count(
    health_monitor_t monitor,
    anomaly_type_t type
);

//=============================================================================
// Baseline and Configuration API
//=============================================================================

/**
 * @brief Establish baseline from current metrics
 *
 * WHAT: Sets current metrics as baseline (normal operation)
 * WHY: Enable anomaly detection relative to normal state
 * HOW: Calculates statistics from current metric window
 *
 * @param monitor Health monitor handle
 * @return true on success, false on failure
 */
bool health_monitor_establish_baseline(health_monitor_t monitor);

/**
 * @brief Reset baseline to initial state
 *
 * @param monitor Health monitor handle
 * @return true on success
 */
bool health_monitor_reset_baseline(health_monitor_t monitor);

/**
 * @brief Set anomaly detection threshold
 *
 * @param monitor Health monitor handle
 * @param z_score_threshold Z-score threshold for anomalies
 * @return true on success
 */
bool health_monitor_set_anomaly_threshold(
    health_monitor_t monitor,
    double z_score_threshold
);

/**
 * @brief Set monitoring interval
 *
 * @param monitor Health monitor handle
 * @param interval_ms Monitoring interval in milliseconds
 * @return true on success
 */
bool health_monitor_set_interval(
    health_monitor_t monitor,
    uint32_t interval_ms
);

//=============================================================================
// Reporting and Export API
//=============================================================================

/**
 * @brief Generate health report
 *
 * WHAT: Generates comprehensive health report
 * WHY: Human-readable status and diagnostics
 * HOW: Formats metrics, anomalies, and recommendations
 *
 * @param monitor Health monitor handle
 * @param output Output file stream (stdout, file, etc.)
 */
void health_monitor_report(health_monitor_t monitor, FILE* output);

/**
 * @brief Export metrics to JSON
 *
 * @param monitor Health monitor handle
 * @param json_buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of bytes written, -1 on error
 */
int32_t health_monitor_export_json(
    health_monitor_t monitor,
    char* json_buffer,
    size_t buffer_size
);

/**
 * @brief Get operation statistics
 *
 * @param monitor Health monitor handle
 * @param operation Operation name
 * @param stats Output parameter for statistics
 * @return true on success
 */
bool health_monitor_get_operation_stats(
    health_monitor_t monitor,
    const char* operation,
    operation_metric_t* stats
);

/**
 * @brief Get memory statistics
 *
 * @param monitor Health monitor handle
 * @param stats Output parameter for statistics
 * @return true on success
 */
bool health_monitor_get_memory_stats(
    health_monitor_t monitor,
    memory_metric_t* stats
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert health status to string
 *
 * @param status Health status
 * @return String representation
 */
const char* health_status_to_string(health_status_t status);

/**
 * @brief Convert anomaly type to string
 *
 * @param type Anomaly type
 * @return String representation
 */
const char* anomaly_type_to_string(anomaly_type_t type);

/**
 * @brief Convert anomaly severity to string
 *
 * @param severity Anomaly severity
 * @return String representation
 */
const char* anomaly_severity_to_string(anomaly_severity_t severity);

/**
 * @brief Get current timestamp in microseconds
 *
 * @return Timestamp in microseconds since epoch
 */
uint64_t health_monitor_get_timestamp_us(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_HEALTH_MONITOR_H
