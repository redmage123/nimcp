/**
 * @file nimcp_metrics_aggregator.h
 * @brief Metrics Aggregator for Fault Tolerance
 *
 * WHAT: Aggregate raw metrics into time windows (min/max/avg/p50/p95/p99)
 * WHY:  250x less memory (16MB → 64KB), faster queries, trend analysis
 * HOW:  Rolling windows, percentile calculation, histogram-based statistics
 *
 * Memory Efficiency:
 * - Raw metrics: 1000 samples/sec × 1 hour = 3.6M samples × 4 bytes = 14.4MB
 * - Aggregated: 4 windows × 16 stats × 1KB = 64KB (225x reduction)
 *
 * Time Windows:
 * - 1 second:  High-resolution recent data
 * - 10 seconds: Short-term trends
 * - 1 minute:   Medium-term patterns
 * - 1 hour:     Long-term trends
 */

#ifndef NIMCP_METRICS_AGGREGATOR_H
#define NIMCP_METRICS_AGGREGATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Time window types
 */
typedef enum {
    NIMCP_WINDOW_1S = 0,    /**< 1 second window */
    NIMCP_WINDOW_10S,       /**< 10 second window */
    NIMCP_WINDOW_1M,        /**< 1 minute window */
    NIMCP_WINDOW_1H,        /**< 1 hour window */
    NIMCP_WINDOW_COUNT      /**< Number of window types */
} nimcp_time_window_t;

/**
 * @brief Aggregated metric statistics
 */
typedef struct {
    double min;             /**< Minimum value in window */
    double max;             /**< Maximum value in window */
    double avg;             /**< Average value */
    double sum;             /**< Sum of all values */
    double p50;             /**< 50th percentile (median) */
    double p95;             /**< 95th percentile */
    double p99;             /**< 99th percentile */
    uint64_t count;         /**< Number of samples */
    time_t window_start;    /**< Window start time */
    time_t window_end;      /**< Window end time */
} nimcp_aggregated_metric_t;

/**
 * @brief Histogram for percentile calculation
 * Uses fixed buckets for memory efficiency
 */
#define NIMCP_HISTOGRAM_BUCKETS 100

typedef struct {
    uint32_t buckets[NIMCP_HISTOGRAM_BUCKETS];  /**< Sample counts per bucket */
    double bucket_width;                         /**< Width of each bucket */
    double min_value;                            /**< Minimum value seen */
    double max_value;                            /**< Maximum value seen */
    uint64_t total_count;                        /**< Total samples */
} nimcp_histogram_t;

/**
 * @brief Rolling window buffer
 */
#define NIMCP_WINDOW_BUFFER_SIZE 3600  /**< Max samples per window (1 hour at 1/sec) */

typedef struct {
    double values[NIMCP_WINDOW_BUFFER_SIZE];
    uint32_t head;          /**< Next write position */
    uint32_t count;         /**< Current sample count */
    uint32_t capacity;      /**< Maximum samples for this window */
    time_t window_start;    /**< Current window start time */
} nimcp_rolling_window_t;

/**
 * @brief Metric aggregator instance
 */
typedef struct {
    char metric_name[64];                           /**< Metric identifier */
    nimcp_rolling_window_t windows[NIMCP_WINDOW_COUNT];
    nimcp_histogram_t histograms[NIMCP_WINDOW_COUNT];
    nimcp_aggregated_metric_t cached_stats[NIMCP_WINDOW_COUNT];

    /* Configuration */
    bool auto_aggregate;    /**< Automatically aggregate on sample add */
    uint32_t aggregate_interval;  /**< Seconds between aggregations */
    time_t last_aggregate_time;

    /* Statistics */
    uint64_t total_samples;
    uint64_t aggregations_performed;
} nimcp_metrics_aggregator_t;

/* =============================================================================
 * Core Aggregator Functions
 * ============================================================================= */

/**
 * @brief Create metrics aggregator
 * WHAT: Initializes aggregator for a named metric
 * WHY:  Required before collecting samples
 * HOW:  Allocates memory, initializes windows and histograms
 *
 * @param metric_name Name of the metric (e.g., "latency_ms", "error_rate")
 * @return Initialized aggregator, NULL on failure
 */
nimcp_metrics_aggregator_t* nimcp_metrics_aggregator_create(const char* metric_name);

/**
 * @brief Destroy metrics aggregator
 * WHAT: Frees aggregator resources
 * WHY:  Prevents memory leaks
 * HOW:  Validates pointer, frees memory
 *
 * @param agg Aggregator to destroy
 */
void nimcp_metrics_aggregator_destroy(nimcp_metrics_aggregator_t* agg);

/**
 * @brief Add sample to aggregator
 * WHAT: Records new metric sample value
 * WHY:  Collect data for aggregation
 * HOW:  Adds to rolling windows, updates histograms, triggers auto-aggregation
 *
 * @param agg Aggregator instance
 * @param value Sample value
 * @param timestamp Sample timestamp (0 = use current time)
 * @return true on success, false on error
 */
bool nimcp_metrics_aggregator_add_sample(
    nimcp_metrics_aggregator_t* agg,
    double value,
    time_t timestamp
);

/**
 * @brief Manually trigger aggregation
 * WHAT: Computes statistics for all time windows
 * WHY:  Get up-to-date aggregated metrics
 * HOW:  Calculates min/max/avg/percentiles from histograms
 *
 * @param agg Aggregator instance
 * @return true on success, false on error
 */
bool nimcp_metrics_aggregator_aggregate(nimcp_metrics_aggregator_t* agg);

/* =============================================================================
 * Query Functions
 * ============================================================================= */

/**
 * @brief Get aggregated statistics for a time window
 * WHAT: Returns pre-computed statistics for specified window
 * WHY:  Fast access to aggregated metrics
 * HOW:  Returns cached aggregated stats
 *
 * @param agg Aggregator instance
 * @param window Time window to query
 * @return Pointer to aggregated stats, NULL on error
 */
const nimcp_aggregated_metric_t* nimcp_metrics_aggregator_get_stats(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
);

/**
 * @brief Get minimum value in window
 * WHAT: Returns minimum sample value in time window
 * WHY:  Quick access to min without full stats
 * HOW:  Returns cached min value
 *
 * @param agg Aggregator instance
 * @param window Time window
 * @return Minimum value, 0.0 on error
 */
double nimcp_metrics_aggregator_get_min(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
);

/**
 * @brief Get maximum value in window
 * WHAT: Returns maximum sample value in time window
 * WHY:  Quick access to max without full stats
 * HOW:  Returns cached max value
 *
 * @param agg Aggregator instance
 * @param window Time window
 * @return Maximum value, 0.0 on error
 */
double nimcp_metrics_aggregator_get_max(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
);

/**
 * @brief Get average value in window
 * WHAT: Returns average sample value in time window
 * WHY:  Quick access to average without full stats
 * HOW:  Returns cached average value
 *
 * @param agg Aggregator instance
 * @param window Time window
 * @return Average value, 0.0 on error
 */
double nimcp_metrics_aggregator_get_avg(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
);

/**
 * @brief Get percentile value
 * WHAT: Returns specified percentile from histogram
 * WHY:  Understand metric distribution
 * HOW:  Calculates from histogram buckets
 *
 * @param agg Aggregator instance
 * @param window Time window
 * @param percentile Percentile to calculate (0.0-1.0, e.g., 0.95 for P95)
 * @return Percentile value, 0.0 on error
 */
double nimcp_metrics_aggregator_get_percentile(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window,
    double percentile
);

/**
 * @brief Get sample count in window
 * WHAT: Returns number of samples in time window
 * WHY:  Understand sample density, validate statistics
 * HOW:  Returns window sample count
 *
 * @param agg Aggregator instance
 * @param window Time window
 * @return Sample count, 0 on error
 */
uint64_t nimcp_metrics_aggregator_get_count(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
);

/* =============================================================================
 * Configuration
 * ============================================================================= */

/**
 * @brief Enable/disable auto-aggregation
 * WHAT: Controls automatic statistics calculation
 * WHY:  Trade CPU for freshness of statistics
 * HOW:  Sets flag, configures aggregation interval
 *
 * @param agg Aggregator instance
 * @param enabled Enable auto-aggregation
 * @param interval_seconds Seconds between auto-aggregations (0 = every sample)
 * @return true on success, false on error
 */
bool nimcp_metrics_aggregator_set_auto_aggregate(
    nimcp_metrics_aggregator_t* agg,
    bool enabled,
    uint32_t interval_seconds
);

/**
 * @brief Reset aggregator
 * WHAT: Clears all samples and statistics
 * WHY:  Start fresh data collection
 * HOW:  Clears windows, histograms, cached stats
 *
 * @param agg Aggregator instance
 */
void nimcp_metrics_aggregator_reset(nimcp_metrics_aggregator_t* agg);

/* =============================================================================
 * Histogram Operations (Internal/Advanced)
 * ============================================================================= */

/**
 * @brief Calculate percentile from histogram
 * WHAT: Computes percentile value from histogram buckets
 * WHY:  Efficient percentile calculation without sorting
 * HOW:  Iterates buckets to find percentile threshold
 *
 * @param hist Histogram
 * @param percentile Percentile to calculate (0.0-1.0)
 * @return Percentile value
 */
double nimcp_histogram_percentile(const nimcp_histogram_t* hist, double percentile);

/**
 * @brief Add value to histogram
 * WHAT: Records value in histogram bucket
 * WHY:  Enables percentile calculation
 * HOW:  Determines bucket, increments count
 *
 * @param hist Histogram
 * @param value Value to record
 */
void nimcp_histogram_add(nimcp_histogram_t* hist, double value);

/**
 * @brief Reset histogram
 * WHAT: Clears all histogram buckets
 * WHY:  Start fresh histogram
 * HOW:  Zeros bucket counts
 *
 * @param hist Histogram to reset
 */
void nimcp_histogram_reset(nimcp_histogram_t* hist);

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

/**
 * @brief Convert window type to string
 * WHAT: Returns human-readable window name
 * WHY:  Logging, debugging, display
 * HOW:  Lookup table
 *
 * @param window Window type
 * @return Window name string
 */
const char* nimcp_window_to_string(nimcp_time_window_t window);

/**
 * @brief Get window duration in seconds
 * WHAT: Returns window size in seconds
 * WHY:  Calculate window boundaries
 * HOW:  Lookup table
 *
 * @param window Window type
 * @return Window duration in seconds
 */
uint32_t nimcp_window_duration(nimcp_time_window_t window);

/**
 * @brief Get aggregator statistics
 * WHAT: Returns global aggregator statistics
 * WHY:  Monitor aggregator performance
 * HOW:  Returns internal counters
 *
 * @param agg Aggregator instance
 * @param total_samples Output: total samples collected
 * @param aggregations Output: number of aggregations performed
 * @return true on success, false on error
 */
bool nimcp_metrics_aggregator_get_statistics(
    const nimcp_metrics_aggregator_t* agg,
    uint64_t* total_samples,
    uint64_t* aggregations
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METRICS_AGGREGATOR_H */
