/**
 * @file nimcp_health_monitor.c
 * @brief NIMCP Runtime Health Monitoring Implementation
 * @version 1.0.0
 * @date 2025-11-19
 */

#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"

#define LOG_MODULE "utils_health_monitor"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(health_monitor)

#include "utils/thread/nimcp_thread.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Circular buffer for metric history
 */
typedef struct {
    double* values;
    uint32_t capacity;
    uint32_t size;
    uint32_t head;
} metric_history_t;

/**
 * @brief Internal health monitor structure
 */
struct health_monitor_internal {
    char brain_id[NIMCP_ID_BUFFER_SIZE];                          /**< Brain identifier */
    bool running;                               /**< Monitor running flag */
    nimcp_thread_t monitor_thread;              /**< Background thread */
    nimcp_mutex_t mutex;                        /**< Thread synchronization */

    // Configuration
    double anomaly_threshold;                   /**< Z-score threshold */
    uint32_t monitoring_interval_ms;            /**< Check interval */
    bool baseline_established;                  /**< Baseline ready */

    // Operation metrics
    operation_metric_t operations[HEALTH_MONITOR_MAX_OPERATIONS];
    uint32_t num_operations;

    // Memory metrics
    memory_metric_t memory;

    // Error metrics
    error_metric_t errors[HEALTH_MONITOR_MAX_ERROR_TYPES];
    uint32_t num_error_types;

    // Throughput metrics
    throughput_metric_t throughput;

    // Cache metrics
    cache_metric_t cache;

    // Thread metrics
    thread_metric_t threads;

    // Anomaly tracking
    anomaly_t anomalies[HEALTH_MONITOR_MAX_ANOMALIES];
    uint32_t num_anomalies;

    // Metric history for trend analysis
    metric_history_t memory_history;
    metric_history_t latency_history;
    metric_history_t error_history;

    // Health status
    health_status_snapshot_t last_status;
    uint64_t last_check_us;

    // Statistics
    uint64_t total_checks;
    uint64_t total_anomalies_detected;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current timestamp in microseconds
 */
uint64_t health_monitor_get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * @brief Create metric history buffer
 */
static metric_history_t* create_metric_history(uint32_t capacity) {
    metric_history_t* history = (metric_history_t*)nimcp_malloc(sizeof(metric_history_t));
    if (!history) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "history is NULL");

        return NULL;

    }

    history->values = (double*)nimcp_calloc(capacity, sizeof(double));
    if (!history->values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_metric_history: failed to allocate %u values", capacity);
        nimcp_free(history);
        return NULL;
    }

    history->capacity = capacity;
    history->size = 0;
    history->head = 0;

    return history;
}

/**
 * @brief Destroy metric history buffer (for heap-allocated histories)
 */
static void destroy_metric_history(metric_history_t* history) {
    if (history) {
        nimcp_free(history->values);
        nimcp_free(history);
    }
}

/**
 * @brief Free metric history contents (for embedded histories)
 */
static void free_metric_history_contents(metric_history_t* history) {
    if (history && history->values) {
        nimcp_free(history->values);
        history->values = NULL;
    }
}

/**
 * @brief Add value to metric history
 */
static void add_to_history(metric_history_t* history, double value) {
    if (!history || !history->values) return;
    if (history->capacity == 0) return;  // Defensive: prevent modulo by zero

    history->values[history->head] = value;
    history->head = (history->head + 1) % history->capacity;

    if (history->size < history->capacity) {
        history->size++;
    }
}

/**
 * @brief Calculate mean of metric history
 */
static double calculate_mean(const metric_history_t* history) {
    if (!history || !history->values || history->size == 0) return 0.0;

    double sum = 0.0;
    for (uint32_t i = 0; i < history->size; i++) {
        sum += history->values[i];
    }

    return sum / history->size;
}

/**
 * @brief Calculate standard deviation of metric history
 */
static double calculate_std_dev(const metric_history_t* history, double mean) {
    if (!history || !history->values || history->size < 2) return 0.0;

    double sum_sq_diff = 0.0;
    for (uint32_t i = 0; i < history->size; i++) {
        double diff = history->values[i] - mean;
        sum_sq_diff += diff * diff;
    }

    return sqrt(sum_sq_diff / (history->size - 1));
}

/**
 * @brief Calculate linear regression trend
 */
static double calculate_trend(const metric_history_t* history) {
    if (!history || !history->values || history->size < 2) return 0.0;

    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    uint32_t n = history->size;

    for (uint32_t i = 0; i < n; i++) {
        double x = (double)i;
        double y = history->values[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    double denominator = n * sum_xx - sum_x * sum_x;
    if (fabs(denominator) < 1e-10) return 0.0;

    return (n * sum_xy - sum_x * sum_y) / denominator;
}

/**
 * @brief Update metric statistics
 */
static void update_metric_stats(metric_stats_t* stats, double value, double alpha) {
    if (!stats) return;
    if (alpha < 0.0 || alpha > 1.0) alpha = 0.1;  // Defensive: clamp alpha to valid range

    if (stats->count == 0) {
        stats->mean = value;
        stats->variance = 0.0;
        stats->std_dev = 0.0;
        stats->min = value;
        stats->max = value;
        stats->moving_avg = value;
        stats->trend = 0.0;
    } else {
        // Update min/max
        if (value < stats->min) stats->min = value;
        if (value > stats->max) stats->max = value;

        // Update mean and variance (Welford's online algorithm)
        double delta = value - stats->mean;
        stats->mean += delta / (stats->count + 1);
        double delta2 = value - stats->mean;
        stats->variance += delta * delta2;
        stats->std_dev = sqrt(stats->variance / (stats->count + 1));

        // Update exponential moving average
        stats->moving_avg = alpha * value + (1.0 - alpha) * stats->moving_avg;
    }

    stats->count++;
}

/**
 * @brief Calculate Z-score for value
 */
static double calculate_z_score(double value, const metric_stats_t* stats) {
    if (!stats || stats->std_dev < 1e-10) return 0.0;
    return (value - stats->mean) / stats->std_dev;
}

/**
 * @brief Find or create operation metric
 */
static operation_metric_t* find_or_create_operation(
    health_monitor_t monitor,
    const char* operation
) {
    if (!monitor || !operation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_operation: required parameter is NULL (monitor, operation)");
        return NULL;
    }

    // Search for existing operation
    for (uint32_t i = 0; i < monitor->num_operations; i++) {
        if (strcmp(monitor->operations[i].name, operation) == 0) {
            return &monitor->operations[i];
        }
    }

    // Create new operation if space available
    if (monitor->num_operations >= HEALTH_MONITOR_MAX_OPERATIONS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_operation: capacity exceeded");
        return NULL;
    }

    operation_metric_t* op = &monitor->operations[monitor->num_operations++];
    memset(op, 0, sizeof(operation_metric_t));
    strncpy(op->name, operation, sizeof(op->name) - 1);
    op->min_duration_us = UINT64_MAX;

    return op;
}

/**
 * @brief Find or create error metric
 */
static error_metric_t* find_or_create_error(
    health_monitor_t monitor,
    const char* error_type
) {
    if (!monitor || !error_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_error: required parameter is NULL (monitor, error_type)");
        return NULL;
    }

    // Search for existing error type
    for (uint32_t i = 0; i < monitor->num_error_types; i++) {
        if (strcmp(monitor->errors[i].type, error_type) == 0) {
            return &monitor->errors[i];
        }
    }

    // Create new error type if space available
    if (monitor->num_error_types >= HEALTH_MONITOR_MAX_ERROR_TYPES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_error: capacity exceeded");
        return NULL;
    }

    error_metric_t* err = &monitor->errors[monitor->num_error_types++];
    memset(err, 0, sizeof(error_metric_t));
    strncpy(err->type, error_type, sizeof(err->type) - 1);

    return err;
}

//=============================================================================
// Anomaly Detection Algorithms
//=============================================================================

/**
 * @brief Detect memory leak anomaly
 */
static bool detect_memory_leak(health_monitor_t monitor, anomaly_t* anomaly) {
    if (!monitor->baseline_established) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_memory_leak: monitor->baseline_established is NULL");
        return false;
    }

    double trend = calculate_trend(&monitor->memory_history);
    double mean = calculate_mean(&monitor->memory_history);

    // Memory leak: positive trend and growing beyond baseline
    if (trend > 0 && mean > monitor->memory.baseline_bytes * 1.2) {
        if (anomaly) {
            anomaly->type = ANOMALY_MEMORY_LEAK;
            anomaly->severity = trend > 1000 ? ANOMALY_SEVERITY_CRITICAL : ANOMALY_SEVERITY_WARNING;
            snprintf(anomaly->description, sizeof(anomaly->description),
                    "Memory leak detected: %.2f bytes/sample growth rate", trend);
            anomaly->confidence = fmin(0.9, fabs(trend) / 1000.0);
            anomaly->detected_at_us = health_monitor_get_timestamp_us();
            anomaly->metric_value = mean;
            anomaly->expected_value = monitor->memory.baseline_bytes;
            anomaly->deviation = trend;
            strncpy(anomaly->affected_component, "memory", sizeof(anomaly->affected_component) - 1);
            anomaly->affected_component[sizeof(anomaly->affected_component) - 1] = '\0';
            anomaly->resolved = false;
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_memory_leak: operation failed");
    return false;
}

/**
 * @brief Detect performance degradation
 */
static bool detect_performance_degradation(
    health_monitor_t monitor,
    anomaly_t* anomaly
) {
    if (!monitor->baseline_established) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_performance_degradation: monitor->baseline_established is NULL");
        return false;
    }

    double trend = calculate_trend(&monitor->latency_history);
    double mean = calculate_mean(&monitor->latency_history);

    // Performance degradation: increasing latency trend
    if (trend > 0 && monitor->latency_history.size > 10) {
        double z_score = fabs(trend) / (mean + 1.0);
        if (z_score > 0.1) {  // 10% degradation
            if (anomaly) {
                anomaly->type = ANOMALY_PERFORMANCE_DEGRADATION;
                anomaly->severity = z_score > 0.5 ? ANOMALY_SEVERITY_ERROR : ANOMALY_SEVERITY_WARNING;
                snprintf(anomaly->description, sizeof(anomaly->description),
                        "Performance degradation: %.2f%% latency increase trend", z_score * 100);
                anomaly->confidence = fmin(0.9, z_score);
                anomaly->detected_at_us = health_monitor_get_timestamp_us();
                anomaly->metric_value = mean;
                anomaly->expected_value = mean - trend * monitor->latency_history.size / 2;
                anomaly->deviation = trend;
                strncpy(anomaly->affected_component, "performance", sizeof(anomaly->affected_component) - 1);
                anomaly->affected_component[sizeof(anomaly->affected_component) - 1] = '\0';
                anomaly->resolved = false;
            }
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_performance_degradation: operation failed");
    return false;
}

/**
 * @brief Detect error spike
 */
static bool detect_error_spike(health_monitor_t monitor, anomaly_t* anomaly) {
    if (!monitor->baseline_established || monitor->num_error_types == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_error_spike: monitor->baseline_established is NULL");
        return false;
    }

    double mean = calculate_mean(&monitor->error_history);

    // Count recent errors
    uint64_t recent_errors = 0;
    for (uint32_t i = 0; i < monitor->num_error_types; i++) {
        recent_errors += monitor->errors[i].count_last_window;
    }

    double z_score = calculate_z_score((double)recent_errors, &monitor->errors[0].stats);

    // Error spike: Z-score exceeds threshold
    if (fabs(z_score) > monitor->anomaly_threshold) {
        if (anomaly) {
            anomaly->type = ANOMALY_ERROR_SPIKE;
            anomaly->severity = z_score > 5.0 ? ANOMALY_SEVERITY_CRITICAL : ANOMALY_SEVERITY_ERROR;
            snprintf(anomaly->description, sizeof(anomaly->description),
                    "Error spike detected: %.1f sigma deviation (%.0f errors)", z_score, (double)recent_errors);
            anomaly->confidence = fmin(0.95, fabs(z_score) / 10.0);
            anomaly->detected_at_us = health_monitor_get_timestamp_us();
            anomaly->metric_value = (double)recent_errors;
            anomaly->expected_value = mean;
            anomaly->deviation = z_score;
            strncpy(anomaly->affected_component, "error_handling", sizeof(anomaly->affected_component) - 1);
            anomaly->affected_component[sizeof(anomaly->affected_component) - 1] = '\0';
            anomaly->resolved = false;
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_error_spike: operation failed");
    return false;
}

/**
 * @brief Detect cache thrashing
 */
static bool detect_cache_thrashing(health_monitor_t monitor, anomaly_t* anomaly) {
    if (!monitor->baseline_established) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_cache_thrashing: monitor->baseline_established is NULL");
        return false;
    }

    double hit_rate = monitor->cache.hit_rate;
    double avg_hit_rate = monitor->cache.avg_hit_rate;

    // Cache thrashing: hit rate significantly below average
    if (avg_hit_rate > 0 && hit_rate < avg_hit_rate * 0.5) {
        if (anomaly) {
            anomaly->type = ANOMALY_CACHE_THRASHING;
            anomaly->severity = hit_rate < 0.2 ? ANOMALY_SEVERITY_ERROR : ANOMALY_SEVERITY_WARNING;
            snprintf(anomaly->description, sizeof(anomaly->description),
                    "Cache thrashing: %.1f%% hit rate (avg: %.1f%%)",
                    hit_rate * 100, avg_hit_rate * 100);
            anomaly->confidence = 0.8;
            anomaly->detected_at_us = health_monitor_get_timestamp_us();
            anomaly->metric_value = hit_rate;
            anomaly->expected_value = avg_hit_rate;
            anomaly->deviation = (avg_hit_rate - hit_rate) / avg_hit_rate;
            strncpy(anomaly->affected_component, "cache", sizeof(anomaly->affected_component) - 1);
            anomaly->affected_component[sizeof(anomaly->affected_component) - 1] = '\0';
            anomaly->resolved = false;
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_cache_thrashing: operation failed");
    return false;
}

/**
 * @brief Detect throughput drop
 */
static bool detect_throughput_drop(health_monitor_t monitor, anomaly_t* anomaly) {
    if (!monitor->baseline_established) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "detect_throughput_drop: monitor->baseline_established is NULL");
        return false;
    }

    double current_ops = monitor->throughput.operations_per_sec;
    double avg_ops = monitor->throughput.avg_ops_per_sec;

    // Throughput drop: significantly below average
    if (avg_ops > 0 && current_ops < avg_ops * 0.6) {
        if (anomaly) {
            anomaly->type = ANOMALY_THROUGHPUT_DROP;
            anomaly->severity = current_ops < avg_ops * 0.3 ? ANOMALY_SEVERITY_CRITICAL : ANOMALY_SEVERITY_WARNING;
            snprintf(anomaly->description, sizeof(anomaly->description),
                    "Throughput drop: %.1f ops/sec (avg: %.1f ops/sec)",
                    current_ops, avg_ops);
            anomaly->confidence = 0.85;
            anomaly->detected_at_us = health_monitor_get_timestamp_us();
            anomaly->metric_value = current_ops;
            anomaly->expected_value = avg_ops;
            anomaly->deviation = (avg_ops - current_ops) / avg_ops;
            strncpy(anomaly->affected_component, "throughput", sizeof(anomaly->affected_component) - 1);
            anomaly->affected_component[sizeof(anomaly->affected_component) - 1] = '\0';
            anomaly->resolved = false;
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_throughput_drop: operation failed");
    return false;
}

/**
 * @brief Detect thread contention
 */
static bool detect_thread_contention(health_monitor_t monitor, anomaly_t* anomaly) {
    double contention_rate = monitor->threads.contention_rate;

    // High thread contention
    if (contention_rate > 0.3) {  // More than 30% contentious
        if (anomaly) {
            anomaly->type = ANOMALY_THREAD_CONTENTION;
            anomaly->severity = contention_rate > 0.6 ? ANOMALY_SEVERITY_ERROR : ANOMALY_SEVERITY_WARNING;
            snprintf(anomaly->description, sizeof(anomaly->description),
                    "High thread contention: %.1f%% of lock acquisitions contested",
                    contention_rate * 100);
            anomaly->confidence = 0.75;
            anomaly->detected_at_us = health_monitor_get_timestamp_us();
            anomaly->metric_value = contention_rate;
            anomaly->expected_value = 0.1;  // Expected < 10%
            anomaly->deviation = contention_rate;
            strncpy(anomaly->affected_component, "threading", sizeof(anomaly->affected_component) - 1);
            anomaly->affected_component[sizeof(anomaly->affected_component) - 1] = '\0';
            anomaly->resolved = false;
        }
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "detect_thread_contention: operation failed");
    return false;
}

//=============================================================================
// Health Scoring
//=============================================================================

/**
 * @brief Calculate memory health score
 */
static float calculate_memory_score(health_monitor_t monitor) {
    float score = 100.0F;

    // Penalize high memory usage
    if (monitor->memory.baseline_bytes > 0) {
        double usage_ratio = (double)monitor->memory.current_bytes / monitor->memory.baseline_bytes;
        if (usage_ratio > 2.0) score -= 30.0F;
        else if (usage_ratio > 1.5) score -= 15.0F;
    }

    // Penalize positive growth rate (memory leak indicator)
    if (monitor->memory.growth_rate > 1000) score -= 20.0F;
    else if (monitor->memory.growth_rate > 100) score -= 10.0F;

    // Penalize allocation/deallocation imbalance
    int64_t balance = (int64_t)monitor->memory.allocation_count -
                      (int64_t)monitor->memory.deallocation_count;
    if (balance > 1000) score -= 15.0F;

    return fmax(0.0F, score);
}

/**
 * @brief Calculate performance health score
 */
static float calculate_performance_score(health_monitor_t monitor) {
    float score = 100.0F;

    // Check latency trends
    double trend = calculate_trend(&monitor->latency_history);
    if (trend > 100) score -= 30.0F;  // Significant degradation
    else if (trend > 10) score -= 15.0F;

    // Check operation statistics
    for (uint32_t i = 0; i < monitor->num_operations; i++) {
        operation_metric_t* op = &monitor->operations[i];
        if (op->count > 10) {
            // Penalize high variance
            double cv = op->stats.std_dev / (op->stats.mean + 1.0);  // Coefficient of variation
            if (cv > 1.0) score -= 5.0F;
        }
    }

    return fmax(0.0F, score);
}

/**
 * @brief Calculate error health score
 */
static float calculate_error_score(health_monitor_t monitor) {
    float score = 100.0F;

    // Count recent errors
    uint64_t total_errors = 0;
    for (uint32_t i = 0; i < monitor->num_error_types; i++) {
        total_errors += monitor->errors[i].count_last_window;
    }

    // Penalize based on error count
    if (total_errors > 100) score = 0.0F;
    else if (total_errors > 50) score -= 50.0F;
    else if (total_errors > 10) score -= 25.0F;
    else if (total_errors > 0) score -= 10.0F;

    return fmax(0.0F, score);
}

/**
 * @brief Calculate throughput health score
 */
static float calculate_throughput_score(health_monitor_t monitor) {
    float score = 100.0F;

    if (monitor->throughput.avg_ops_per_sec > 0) {
        double ratio = monitor->throughput.operations_per_sec /
                      monitor->throughput.avg_ops_per_sec;

        if (ratio < 0.3) score -= 50.0F;
        else if (ratio < 0.6) score -= 25.0F;
        else if (ratio < 0.8) score -= 10.0F;
    }

    return fmax(0.0F, score);
}

/**
 * @brief Calculate cache health score
 */
static float calculate_cache_score(health_monitor_t monitor) {
    float score = 100.0F;

    double hit_rate = monitor->cache.hit_rate;
    if (hit_rate < 0.3) score -= 40.0F;
    else if (hit_rate < 0.5) score -= 25.0F;
    else if (hit_rate < 0.7) score -= 10.0F;

    return fmax(0.0F, score);
}

/**
 * @brief Calculate thread health score
 */
static float calculate_thread_score(health_monitor_t monitor) {
    float score = 100.0F;

    double contention_rate = monitor->threads.contention_rate;
    if (contention_rate > 0.6) score -= 40.0F;
    else if (contention_rate > 0.4) score -= 25.0F;
    else if (contention_rate > 0.2) score -= 10.0F;

    return fmax(0.0F, score);
}

/**
 * @brief Calculate overall health score
 */
static float calculate_health_score(health_monitor_t monitor) {
    // Weighted combination of component scores
    float memory_score = calculate_memory_score(monitor);
    float perf_score = calculate_performance_score(monitor);
    float error_score = calculate_error_score(monitor);
    float throughput_score = calculate_throughput_score(monitor);
    float cache_score = calculate_cache_score(monitor);
    float thread_score = calculate_thread_score(monitor);

    // Weights (sum to 1.0)
    const float weights[] = {0.25F, 0.25F, 0.20F, 0.15F, 0.10F, 0.05F};

    float overall = weights[0] * memory_score +
                   weights[1] * perf_score +
                   weights[2] * error_score +
                   weights[3] * throughput_score +
                   weights[4] * cache_score +
                   weights[5] * thread_score;

    return fmax(0.0F, fmin(100.0F, overall));
}

//=============================================================================
// Background Monitoring Thread
//=============================================================================

/**
 * @brief Background monitoring thread function
 */
static void* monitoring_thread_func(void* arg) {
    health_monitor_t monitor = (health_monitor_t)arg;

    NIMCP_LOGGING_INFO("Health monitor started for brain '%s'", monitor->brain_id);

    while (monitor->running) {
        nimcp_mutex_lock(&monitor->mutex);

        // Perform health check
        monitor->total_checks++;

        // Detect anomalies
        anomaly_t temp_anomaly;
        if (detect_memory_leak(monitor, &temp_anomaly)) {
            if (monitor->num_anomalies < HEALTH_MONITOR_MAX_ANOMALIES) {
                monitor->anomalies[monitor->num_anomalies++] = temp_anomaly;
                monitor->total_anomalies_detected++;
            }
        }

        if (detect_performance_degradation(monitor, &temp_anomaly)) {
            if (monitor->num_anomalies < HEALTH_MONITOR_MAX_ANOMALIES) {
                monitor->anomalies[monitor->num_anomalies++] = temp_anomaly;
                monitor->total_anomalies_detected++;
            }
        }

        if (detect_error_spike(monitor, &temp_anomaly)) {
            if (monitor->num_anomalies < HEALTH_MONITOR_MAX_ANOMALIES) {
                monitor->anomalies[monitor->num_anomalies++] = temp_anomaly;
                monitor->total_anomalies_detected++;
            }
        }

        if (detect_cache_thrashing(monitor, &temp_anomaly)) {
            if (monitor->num_anomalies < HEALTH_MONITOR_MAX_ANOMALIES) {
                monitor->anomalies[monitor->num_anomalies++] = temp_anomaly;
                monitor->total_anomalies_detected++;
            }
        }

        if (detect_throughput_drop(monitor, &temp_anomaly)) {
            if (monitor->num_anomalies < HEALTH_MONITOR_MAX_ANOMALIES) {
                monitor->anomalies[monitor->num_anomalies++] = temp_anomaly;
                monitor->total_anomalies_detected++;
            }
        }

        if (detect_thread_contention(monitor, &temp_anomaly)) {
            if (monitor->num_anomalies < HEALTH_MONITOR_MAX_ANOMALIES) {
                monitor->anomalies[monitor->num_anomalies++] = temp_anomaly;
                monitor->total_anomalies_detected++;
            }
        }

        // Calculate health scores
        monitor->last_status.memory_score = calculate_memory_score(monitor);
        monitor->last_status.performance_score = calculate_performance_score(monitor);
        monitor->last_status.error_score = calculate_error_score(monitor);
        monitor->last_status.throughput_score = calculate_throughput_score(monitor);
        monitor->last_status.cache_score = calculate_cache_score(monitor);
        monitor->last_status.thread_score = calculate_thread_score(monitor);
        monitor->last_status.score = calculate_health_score(monitor);

        // Determine status level
        if (monitor->last_status.score >= 90.0F) {
            monitor->last_status.status = HEALTH_EXCELLENT;
        } else if (monitor->last_status.score >= 70.0F) {
            monitor->last_status.status = HEALTH_GOOD;
        } else if (monitor->last_status.score >= 50.0F) {
            monitor->last_status.status = HEALTH_FAIR;
        } else if (monitor->last_status.score >= 30.0F) {
            monitor->last_status.status = HEALTH_POOR;
        } else {
            monitor->last_status.status = HEALTH_CRITICAL;
        }

        monitor->last_status.num_anomalies = monitor->num_anomalies;
        monitor->last_status.timestamp_us = health_monitor_get_timestamp_us();

        nimcp_mutex_unlock(&monitor->mutex);

        // Sleep for monitoring interval
        usleep(monitor->monitoring_interval_ms * 1000);
    }

    NIMCP_LOGGING_INFO("Health monitor stopped for brain '%s'", monitor->brain_id);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "monitoring_thread_func: operation failed");
    return NULL;
}

//=============================================================================
// Public API Implementation
//=============================================================================

health_monitor_t health_monitor_create(const char* brain_id) {
    if (!brain_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_create: brain_id is NULL");
        NIMCP_LOGGING_ERROR("Cannot create health monitor: NULL brain_id");
        return NULL;
    }

    health_monitor_t monitor = (health_monitor_t)nimcp_calloc(1, sizeof(struct health_monitor_internal));
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "health_monitor_create: failed to allocate monitor");
        NIMCP_LOGGING_ERROR("Failed to allocate health monitor");
        return NULL;
    }

    // Initialize basic fields
    strncpy(monitor->brain_id, brain_id, sizeof(monitor->brain_id) - 1);
    monitor->running = false;
    monitor->anomaly_threshold = HEALTH_MONITOR_ANOMALY_THRESHOLD;
    monitor->monitoring_interval_ms = 1000;  // 1 second default
    monitor->baseline_established = false;

    // Initialize mutex
    if (nimcp_mutex_init(&monitor->mutex, NULL) != NIMCP_SUCCESS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "health_monitor_create: failed to initialize mutex");
        NIMCP_LOGGING_ERROR("Failed to initialize monitor mutex");
        nimcp_free(monitor);
        return NULL;
    }

    // Initialize metric histories
    metric_history_t* mem_hist = create_metric_history(HEALTH_MONITOR_WINDOW_SIZE);
    metric_history_t* lat_hist = create_metric_history(HEALTH_MONITOR_WINDOW_SIZE);
    metric_history_t* err_hist = create_metric_history(HEALTH_MONITOR_WINDOW_SIZE);

    if (!mem_hist || !lat_hist || !err_hist) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "health_monitor_create: failed to allocate metric histories");
        NIMCP_LOGGING_ERROR("Failed to allocate metric histories");
        if (mem_hist) nimcp_free(mem_hist);
        if (lat_hist) nimcp_free(lat_hist);
        if (err_hist) nimcp_free(err_hist);
        nimcp_mutex_destroy(&monitor->mutex);
        nimcp_free(monitor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_create: validation failed");
        return NULL;
    }

    monitor->memory_history = *mem_hist;
    monitor->latency_history = *lat_hist;
    monitor->error_history = *err_hist;

    // Free the wrapper structures (we copied the contents)
    nimcp_free(mem_hist);
    nimcp_free(lat_hist);
    nimcp_free(err_hist);

    // Initialize last status
    monitor->last_status.status = HEALTH_UNKNOWN;
    monitor->last_status.score = 0.0F;

    NIMCP_LOGGING_INFO("Health monitor created for brain '%s'", brain_id);

    return monitor;
}

void health_monitor_destroy(health_monitor_t monitor) {
    if (!monitor) return;

    // Stop monitoring if running
    if (monitor->running) {
        health_monitor_stop(monitor);
    }

    // Generate final report
    NIMCP_LOGGING_INFO("Final health report for brain '%s':", monitor->brain_id);
    health_monitor_report(monitor, stdout);

    // Free metric history contents (not the structures themselves - they're embedded)
    free_metric_history_contents(&monitor->memory_history);
    free_metric_history_contents(&monitor->latency_history);
    free_metric_history_contents(&monitor->error_history);

    // Destroy mutex
    nimcp_mutex_destroy(&monitor->mutex);

    // Free monitor
    nimcp_free(monitor);

    NIMCP_LOGGING_INFO("Health monitor destroyed");
}

bool health_monitor_start(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_start: monitor is NULL");
        return false;
    }

    if (monitor->running) {
        NIMCP_LOGGING_WARN("Health monitor already running");
        return true;
    }

    monitor->running = true;

    // Create monitoring thread
    if (nimcp_thread_create(&monitor->monitor_thread, monitoring_thread_func, monitor,
                            NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "health_monitor_start: failed to create monitoring thread");
        NIMCP_LOGGING_ERROR("Failed to create monitoring thread");
        monitor->running = false;
        return false;
    }

    NIMCP_LOGGING_INFO("Health monitoring started");
    return true;
}

bool health_monitor_stop(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_stop: monitor is NULL");
        return false;
    }

    if (!monitor->running) {
        return true;
    }

    monitor->running = false;

    // Wait for thread to finish
    nimcp_thread_join(monitor->monitor_thread, NULL);

    NIMCP_LOGGING_INFO("Health monitoring stopped");
    return true;
}

bool health_monitor_is_running(health_monitor_t monitor) {
    return monitor ? monitor->running : false;
}

void health_monitor_record_operation(
    health_monitor_t monitor,
    const char* operation,
    uint64_t duration_us
) {
    if (!monitor || !operation) return;

    nimcp_mutex_lock(&monitor->mutex);

    operation_metric_t* op = find_or_create_operation(monitor, operation);
    if (op) {
        op->count++;
        op->total_duration_us += duration_us;
        op->avg_duration_us = (double)op->total_duration_us / op->count;
        op->last_recorded_us = health_monitor_get_timestamp_us();

        if (duration_us < op->min_duration_us) op->min_duration_us = duration_us;
        if (duration_us > op->max_duration_us) op->max_duration_us = duration_us;

        // Update statistics with EMA alpha = 0.1
        update_metric_stats(&op->stats, (double)duration_us, 0.1);

        // Add to latency history
        add_to_history(&monitor->latency_history, (double)duration_us);
    }

    nimcp_mutex_unlock(&monitor->mutex);
}

void health_monitor_record_memory(health_monitor_t monitor, size_t bytes) {
    if (!monitor) return;

    nimcp_mutex_lock(&monitor->mutex);

    uint64_t now = health_monitor_get_timestamp_us();
    size_t prev_bytes = monitor->memory.current_bytes;
    uint64_t prev_time = monitor->memory.last_update_us;

    monitor->memory.current_bytes = bytes;
    monitor->memory.last_update_us = now;

    if (bytes > monitor->memory.peak_bytes) {
        monitor->memory.peak_bytes = bytes;
    }

    // Calculate growth rate (bytes per second)
    if (prev_time > 0) {
        double time_diff_sec = (double)(now - prev_time) / 1000000.0;
        if (time_diff_sec > 0) {
            monitor->memory.growth_rate = ((double)bytes - (double)prev_bytes) / time_diff_sec;
        }
    }

    // Update statistics
    update_metric_stats(&monitor->memory.stats, (double)bytes, 0.1);

    // Add to history
    add_to_history(&monitor->memory_history, (double)bytes);

    nimcp_mutex_unlock(&monitor->mutex);
}

void health_monitor_record_error(
    health_monitor_t monitor,
    const char* error_type
) {
    if (!monitor || !error_type) return;

    nimcp_mutex_lock(&monitor->mutex);

    error_metric_t* err = find_or_create_error(monitor, error_type);
    if (err) {
        err->count++;
        err->count_last_window++;
        err->last_occurrence_us = health_monitor_get_timestamp_us();

        // Calculate error rate (errors per minute)
        // Simple moving window approach
        err->rate_per_min = (double)err->count_last_window;

        // Update statistics
        update_metric_stats(&err->stats, 1.0, 0.1);

        // Add to error history
        add_to_history(&monitor->error_history, (double)err->count_last_window);
    }

    nimcp_mutex_unlock(&monitor->mutex);
}

void health_monitor_record_cache_access(health_monitor_t monitor, bool hit) {
    if (!monitor) return;

    nimcp_mutex_lock(&monitor->mutex);

    if (hit) {
        monitor->cache.hits++;
    } else {
        monitor->cache.misses++;
    }

    uint64_t total = monitor->cache.hits + monitor->cache.misses;
    if (total > 0) {
        monitor->cache.hit_rate = (double)monitor->cache.hits / total;

        // Update average hit rate with EMA
        if (monitor->cache.avg_hit_rate == 0.0) {
            monitor->cache.avg_hit_rate = monitor->cache.hit_rate;
        } else {
            monitor->cache.avg_hit_rate = 0.1 * monitor->cache.hit_rate +
                                         0.9 * monitor->cache.avg_hit_rate;
        }

        update_metric_stats(&monitor->cache.stats, monitor->cache.hit_rate, 0.1);
    }

    nimcp_mutex_unlock(&monitor->mutex);
}

void health_monitor_record_thread_event(
    health_monitor_t monitor,
    bool contentious,
    uint64_t wait_time_us
) {
    if (!monitor) return;

    nimcp_mutex_lock(&monitor->mutex);

    monitor->threads.lock_acquisitions++;
    if (contentious) {
        monitor->threads.lock_contentions++;
        monitor->threads.avg_wait_time_us =
            (monitor->threads.avg_wait_time_us * (monitor->threads.lock_contentions - 1) +
             wait_time_us) / monitor->threads.lock_contentions;
    }

    if (monitor->threads.lock_acquisitions > 0) {
        monitor->threads.contention_rate =
            (double)monitor->threads.lock_contentions / monitor->threads.lock_acquisitions;

        update_metric_stats(&monitor->threads.stats, monitor->threads.contention_rate, 0.1);
    }

    nimcp_mutex_unlock(&monitor->mutex);
}

void health_monitor_record_throughput(
    health_monitor_t monitor,
    uint64_t operations,
    uint64_t window_us
) {
    if (!monitor || window_us == 0) return;

    nimcp_mutex_lock(&monitor->mutex);

    monitor->throughput.total_operations += operations;
    monitor->throughput.operations_per_sec = (double)operations / ((double)window_us / 1000000.0);

    // Update average with EMA
    if (monitor->throughput.avg_ops_per_sec == 0.0) {
        monitor->throughput.avg_ops_per_sec = monitor->throughput.operations_per_sec;
    } else {
        monitor->throughput.avg_ops_per_sec = 0.1 * monitor->throughput.operations_per_sec +
                                              0.9 * monitor->throughput.avg_ops_per_sec;
    }

    if (monitor->throughput.operations_per_sec > monitor->throughput.peak_ops_per_sec) {
        monitor->throughput.peak_ops_per_sec = monitor->throughput.operations_per_sec;
    }

    update_metric_stats(&monitor->throughput.stats, monitor->throughput.operations_per_sec, 0.1);
    monitor->throughput.last_update_us = health_monitor_get_timestamp_us();

    nimcp_mutex_unlock(&monitor->mutex);
}

bool health_monitor_get_status(
    health_monitor_t monitor,
    health_status_snapshot_t* status
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_status: monitor is NULL");
        return false;
    }
    if (!status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_status: status is NULL");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);

    // Compute scores on-demand if not running in background
    if (!monitor->running) {
        monitor->last_status.memory_score = calculate_memory_score(monitor);
        monitor->last_status.performance_score = calculate_performance_score(monitor);
        monitor->last_status.error_score = calculate_error_score(monitor);
        monitor->last_status.throughput_score = calculate_throughput_score(monitor);
        monitor->last_status.cache_score = calculate_cache_score(monitor);
        monitor->last_status.thread_score = calculate_thread_score(monitor);
        monitor->last_status.score = calculate_health_score(monitor);

        // Determine status level
        if (monitor->last_status.score >= 90.0F) {
            monitor->last_status.status = HEALTH_EXCELLENT;
        } else if (monitor->last_status.score >= 70.0F) {
            monitor->last_status.status = HEALTH_GOOD;
        } else if (monitor->last_status.score >= 50.0F) {
            monitor->last_status.status = HEALTH_FAIR;
        } else if (monitor->last_status.score >= 30.0F) {
            monitor->last_status.status = HEALTH_POOR;
        } else {
            monitor->last_status.status = HEALTH_CRITICAL;
        }
    }

    *status = monitor->last_status;
    nimcp_mutex_unlock(&monitor->mutex);

    return true;
}

float health_monitor_get_score(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_score: monitor is NULL");
        return -1.0F;
    }

    nimcp_mutex_lock(&monitor->mutex);
    float score = monitor->last_status.score;
    nimcp_mutex_unlock(&monitor->mutex);

    return score;
}

bool health_monitor_is_healthy(health_monitor_t monitor) {
    return health_monitor_get_score(monitor) >= 70.0F;
}

health_status_t health_monitor_get_status_level(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_status_level: monitor is NULL");
        return HEALTH_UNKNOWN;
    }

    nimcp_mutex_lock(&monitor->mutex);
    health_status_t status = monitor->last_status.status;
    nimcp_mutex_unlock(&monitor->mutex);

    return status;
}

int32_t health_monitor_detect_anomalies(
    health_monitor_t monitor,
    anomaly_t* anomalies,
    uint32_t max_anomalies
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_detect_anomalies: monitor is NULL");
        return -1;
    }
    if (!anomalies) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_detect_anomalies: anomalies is NULL");
        return -1;
    }
    if (max_anomalies == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_monitor_detect_anomalies: max_anomalies is 0");
        return -1;
    }

    nimcp_mutex_lock(&monitor->mutex);

    uint32_t count = monitor->num_anomalies < max_anomalies ?
                     monitor->num_anomalies : max_anomalies;

    for (uint32_t i = 0; i < count; i++) {
        anomalies[i] = monitor->anomalies[i];
    }

    nimcp_mutex_unlock(&monitor->mutex);

    return (int32_t)count;
}

bool health_monitor_predict_failure(
    health_monitor_t monitor,
    uint32_t* time_to_failure_sec
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_predict_failure: monitor is NULL");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);

    bool failure_predicted = false;
    uint32_t ttf = UINT32_MAX;

    // Predict based on memory leak
    double mem_trend = calculate_trend(&monitor->memory_history);
    if (mem_trend > 1000) {  // Growing at 1KB per sample
        // Estimate time until memory exhaustion (assuming 1GB limit)
        size_t remaining = 1024 * 1024 * 1024 - monitor->memory.current_bytes;
        if (remaining > 0 && mem_trend > 0) {
            uint32_t samples_to_exhaustion = (uint32_t)(remaining / mem_trend);
            uint32_t seconds = samples_to_exhaustion; // Assuming 1 sample/sec
            if (seconds < ttf) ttf = seconds;
            failure_predicted = true;
        }
    }

    // Predict based on error rate
    if (monitor->num_error_types > 0) {
        uint64_t total_errors = 0;
        for (uint32_t i = 0; i < monitor->num_error_types; i++) {
            total_errors += monitor->errors[i].count_last_window;
        }
        if (total_errors > 50) {  // High error rate
            ttf = fmin(ttf, 300);  // Predict failure within 5 minutes
            failure_predicted = true;
        }
    }

    // Predict based on throughput degradation
    if (monitor->throughput.avg_ops_per_sec > 0 &&
        monitor->throughput.operations_per_sec < monitor->throughput.avg_ops_per_sec * 0.1) {
        ttf = fmin(ttf, 600);  // Predict failure within 10 minutes
        failure_predicted = true;
    }

    monitor->last_status.failure_predicted = failure_predicted;
    monitor->last_status.time_to_failure_sec = ttf;

    if (time_to_failure_sec) {
        *time_to_failure_sec = ttf;
    }

    nimcp_mutex_unlock(&monitor->mutex);

    return failure_predicted;
}

uint32_t health_monitor_clear_resolved_anomalies(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_clear_resolved_anomalies: monitor is NULL");
        return 0;
    }

    nimcp_mutex_lock(&monitor->mutex);

    uint32_t cleared = 0;
    uint32_t new_count = 0;

    // Compact array, removing resolved anomalies
    for (uint32_t i = 0; i < monitor->num_anomalies; i++) {
        if (!monitor->anomalies[i].resolved) {
            if (new_count != i) {
                monitor->anomalies[new_count] = monitor->anomalies[i];
            }
            new_count++;
        } else {
            cleared++;
        }
    }

    monitor->num_anomalies = new_count;

    nimcp_mutex_unlock(&monitor->mutex);

    return cleared;
}

uint32_t health_monitor_get_anomaly_count(
    health_monitor_t monitor,
    anomaly_type_t type
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_anomaly_count: monitor is NULL");
        return 0;
    }

    nimcp_mutex_lock(&monitor->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < monitor->num_anomalies; i++) {
        if (monitor->anomalies[i].type == type && !monitor->anomalies[i].resolved) {
            count++;
        }
    }

    nimcp_mutex_unlock(&monitor->mutex);

    return count;
}

bool health_monitor_establish_baseline(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_establish_baseline: monitor is NULL");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);

    // Set memory baseline
    monitor->memory.baseline_bytes = monitor->memory.current_bytes;

    // Mark baseline as established
    monitor->baseline_established = true;

    nimcp_mutex_unlock(&monitor->mutex);

    NIMCP_LOGGING_INFO("Baseline established for brain '%s'", monitor->brain_id);
    return true;
}

bool health_monitor_reset_baseline(health_monitor_t monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_reset_baseline: monitor is NULL");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);
    monitor->baseline_established = false;
    monitor->memory.baseline_bytes = 0;
    nimcp_mutex_unlock(&monitor->mutex);

    return true;
}

bool health_monitor_set_anomaly_threshold(
    health_monitor_t monitor,
    double z_score_threshold
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_set_anomaly_threshold: monitor is NULL");
        return false;
    }
    if (z_score_threshold < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_monitor_set_anomaly_threshold: z_score_threshold is negative");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);
    monitor->anomaly_threshold = z_score_threshold;
    nimcp_mutex_unlock(&monitor->mutex);

    return true;
}

bool health_monitor_set_interval(health_monitor_t monitor, uint32_t interval_ms) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_set_interval: monitor is NULL");
        return false;
    }
    if (interval_ms == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_monitor_set_interval: interval_ms is 0");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);
    monitor->monitoring_interval_ms = interval_ms;
    nimcp_mutex_unlock(&monitor->mutex);

    return true;
}

void health_monitor_report(health_monitor_t monitor, FILE* output) {
    if (!monitor || !output) return;

    if (nimcp_mutex_lock(&monitor->mutex) != NIMCP_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to acquire mutex for health report");
        return;
    }

    fprintf(output, "\n");
    fprintf(output, "=== NIMCP Health Monitor Report ===\n");
    fprintf(output, "Brain ID: %s\n", monitor->brain_id);
    fprintf(output, "Status: %s (Score: %.1f/100)\n",
            health_status_to_string(monitor->last_status.status),
            monitor->last_status.score);
    fprintf(output, "Running: %s\n", monitor->running ? "Yes" : "No");
    fprintf(output, "Baseline Established: %s\n", monitor->baseline_established ? "Yes" : "No");
    fprintf(output, "\n");

    // Component scores
    fprintf(output, "--- Component Scores ---\n");
    fprintf(output, "Memory:      %.1f/100\n", monitor->last_status.memory_score);
    fprintf(output, "Performance: %.1f/100\n", monitor->last_status.performance_score);
    fprintf(output, "Errors:      %.1f/100\n", monitor->last_status.error_score);
    fprintf(output, "Throughput:  %.1f/100\n", monitor->last_status.throughput_score);
    fprintf(output, "Cache:       %.1f/100\n", monitor->last_status.cache_score);
    fprintf(output, "Threading:   %.1f/100\n", monitor->last_status.thread_score);
    fprintf(output, "\n");

    // Memory metrics
    fprintf(output, "--- Memory Metrics ---\n");
    fprintf(output, "Current: %zu bytes (%.2f MB)\n",
            monitor->memory.current_bytes,
            (double)monitor->memory.current_bytes / (1024 * 1024));
    fprintf(output, "Peak: %zu bytes (%.2f MB)\n",
            monitor->memory.peak_bytes,
            (double)monitor->memory.peak_bytes / (1024 * 1024));
    fprintf(output, "Baseline: %zu bytes (%.2f MB)\n",
            monitor->memory.baseline_bytes,
            (double)monitor->memory.baseline_bytes / (1024 * 1024));
    fprintf(output, "Growth Rate: %.2f bytes/sec\n", monitor->memory.growth_rate);
    fprintf(output, "Allocations: %lu, Deallocations: %lu\n",
            monitor->memory.allocation_count,
            monitor->memory.deallocation_count);
    fprintf(output, "\n");

    // Operation metrics
    fprintf(output, "--- Operation Metrics ---\n");
    for (uint32_t i = 0; i < monitor->num_operations; i++) {
        operation_metric_t* op = &monitor->operations[i];
        fprintf(output, "%s: count=%lu, avg=%.2fus, min=%luus, max=%luus\n",
                op->name, op->count, op->avg_duration_us,
                op->min_duration_us, op->max_duration_us);
    }
    fprintf(output, "\n");

    // Error metrics
    fprintf(output, "--- Error Metrics ---\n");
    for (uint32_t i = 0; i < monitor->num_error_types; i++) {
        error_metric_t* err = &monitor->errors[i];
        fprintf(output, "%s: count=%lu, rate=%.2f/min\n",
                err->type, err->count, err->rate_per_min);
    }
    fprintf(output, "\n");

    // Throughput metrics
    fprintf(output, "--- Throughput Metrics ---\n");
    fprintf(output, "Total Operations: %lu\n", monitor->throughput.total_operations);
    fprintf(output, "Current: %.2f ops/sec\n", monitor->throughput.operations_per_sec);
    fprintf(output, "Average: %.2f ops/sec\n", monitor->throughput.avg_ops_per_sec);
    fprintf(output, "Peak: %.2f ops/sec\n", monitor->throughput.peak_ops_per_sec);
    fprintf(output, "\n");

    // Cache metrics
    fprintf(output, "--- Cache Metrics ---\n");
    fprintf(output, "Hits: %lu, Misses: %lu\n", monitor->cache.hits, monitor->cache.misses);
    fprintf(output, "Hit Rate: %.2f%% (avg: %.2f%%)\n",
            monitor->cache.hit_rate * 100, monitor->cache.avg_hit_rate * 100);
    fprintf(output, "\n");

    // Thread metrics
    fprintf(output, "--- Thread Metrics ---\n");
    fprintf(output, "Lock Acquisitions: %lu\n", monitor->threads.lock_acquisitions);
    fprintf(output, "Lock Contentions: %lu (%.2f%%)\n",
            monitor->threads.lock_contentions,
            monitor->threads.contention_rate * 100);
    fprintf(output, "Avg Wait Time: %lu us\n", monitor->threads.avg_wait_time_us);
    fprintf(output, "\n");

    // Anomalies
    fprintf(output, "--- Active Anomalies (%u) ---\n", monitor->num_anomalies);
    for (uint32_t i = 0; i < monitor->num_anomalies; i++) {
        anomaly_t* a = &monitor->anomalies[i];
        if (!a->resolved) {
            fprintf(output, "[%s] %s: %s (confidence: %.2f)\n",
                    anomaly_severity_to_string(a->severity),
                    anomaly_type_to_string(a->type),
                    a->description,
                    a->confidence);
        }
    }
    fprintf(output, "\n");

    // Predictions
    fprintf(output, "--- Predictions ---\n");
    if (monitor->last_status.failure_predicted) {
        fprintf(output, "FAILURE PREDICTED in %u seconds!\n",
                monitor->last_status.time_to_failure_sec);
    } else {
        fprintf(output, "No imminent failure predicted\n");
    }
    fprintf(output, "\n");

    // Statistics
    fprintf(output, "--- Statistics ---\n");
    fprintf(output, "Total Checks: %lu\n", monitor->total_checks);
    fprintf(output, "Total Anomalies Detected: %lu\n", monitor->total_anomalies_detected);
    fprintf(output, "\n");

    fprintf(output, "=====================================\n");

    nimcp_mutex_unlock(&monitor->mutex);
}

int32_t health_monitor_export_json(
    health_monitor_t monitor,
    char* json_buffer,
    size_t buffer_size
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_export_json: monitor is NULL");
        return -1;
    }
    if (!json_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_export_json: json_buffer is NULL");
        return -1;
    }
    if (buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_monitor_export_json: buffer_size is 0");
        return -1;
    }

    nimcp_mutex_lock(&monitor->mutex);

    int written = snprintf(json_buffer, buffer_size,
        "{"
        "\"brain_id\":\"%s\","
        "\"status\":\"%s\","
        "\"score\":%.1f,"
        "\"running\":%s,"
        "\"baseline_established\":%s,"
        "\"component_scores\":{"
            "\"memory\":%.1f,"
            "\"performance\":%.1f,"
            "\"errors\":%.1f,"
            "\"throughput\":%.1f,"
            "\"cache\":%.1f,"
            "\"threading\":%.1f"
        "},"
        "\"anomalies\":%u,"
        "\"failure_predicted\":%s,"
        "\"time_to_failure_sec\":%u"
        "}",
        monitor->brain_id,
        health_status_to_string(monitor->last_status.status),
        monitor->last_status.score,
        monitor->running ? "true" : "false",
        monitor->baseline_established ? "true" : "false",
        monitor->last_status.memory_score,
        monitor->last_status.performance_score,
        monitor->last_status.error_score,
        monitor->last_status.throughput_score,
        monitor->last_status.cache_score,
        monitor->last_status.thread_score,
        monitor->num_anomalies,
        monitor->last_status.failure_predicted ? "true" : "false",
        monitor->last_status.time_to_failure_sec
    );

    nimcp_mutex_unlock(&monitor->mutex);

    return written < (int32_t)buffer_size ? written : -1;
}

bool health_monitor_get_operation_stats(
    health_monitor_t monitor,
    const char* operation,
    operation_metric_t* stats
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_operation_stats: monitor is NULL");
        return false;
    }
    if (!operation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_operation_stats: operation is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_operation_stats: stats is NULL");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);

    for (uint32_t i = 0; i < monitor->num_operations; i++) {
        if (strcmp(monitor->operations[i].name, operation) == 0) {
            *stats = monitor->operations[i];
            nimcp_mutex_unlock(&monitor->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&monitor->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "health_monitor_get_operation_stats: validation failed");
    return false;
}

bool health_monitor_get_memory_stats(
    health_monitor_t monitor,
    memory_metric_t* stats
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_memory_stats: monitor is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "health_monitor_get_memory_stats: stats is NULL");
        return false;
    }

    nimcp_mutex_lock(&monitor->mutex);
    *stats = monitor->memory;
    nimcp_mutex_unlock(&monitor->mutex);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* health_status_to_string(health_status_t status) {
    switch (status) {
        case HEALTH_EXCELLENT: return "EXCELLENT";
        case HEALTH_GOOD: return "GOOD";
        case HEALTH_FAIR: return "FAIR";
        case HEALTH_POOR: return "POOR";
        case HEALTH_CRITICAL: return "CRITICAL";
        case HEALTH_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

const char* anomaly_type_to_string(anomaly_type_t type) {
    switch (type) {
        case ANOMALY_NONE: return "NONE";
        case ANOMALY_MEMORY_LEAK: return "MEMORY_LEAK";
        case ANOMALY_PERFORMANCE_DEGRADATION: return "PERFORMANCE_DEGRADATION";
        case ANOMALY_ERROR_SPIKE: return "ERROR_SPIKE";
        case ANOMALY_THROUGHPUT_DROP: return "THROUGHPUT_DROP";
        case ANOMALY_CACHE_THRASHING: return "CACHE_THRASHING";
        case ANOMALY_RESOURCE_EXHAUSTION: return "RESOURCE_EXHAUSTION";
        case ANOMALY_NUMERICAL_INSTABILITY: return "NUMERICAL_INSTABILITY";
        case ANOMALY_THREAD_CONTENTION: return "THREAD_CONTENTION";
        case ANOMALY_UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

const char* anomaly_severity_to_string(anomaly_severity_t severity) {
    switch (severity) {
        case ANOMALY_SEVERITY_INFO: return "INFO";
        case ANOMALY_SEVERITY_WARNING: return "WARNING";
        case ANOMALY_SEVERITY_ERROR: return "ERROR";
        case ANOMALY_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "INVALID";
    }
}
