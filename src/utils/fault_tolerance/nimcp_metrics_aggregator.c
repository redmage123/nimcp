/**
 * @file nimcp_metrics_aggregator.c
 * @brief Metrics Aggregator Implementation
 */

#include "utils/fault_tolerance/nimcp_metrics_aggregator.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_metrics_aggregator"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metrics_aggregator)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

/* =============================================================================
 * Window Configuration
 * ============================================================================= */

/* Window capacities (max samples per window) */
static const uint32_t window_capacities[NIMCP_WINDOW_COUNT] = {
    3600,   /* 1s:  up to 3600 samples (limited by buffer) */
    3600,   /* 10s: up to 3600 samples */
    3600,   /* 1m:  up to 3600 samples */
    3600    /* 1h:  up to 3600 samples (limited by NIMCP_WINDOW_BUFFER_SIZE) */
};

/* Window durations in seconds */
static const uint32_t window_durations[NIMCP_WINDOW_COUNT] = {
    1,      /* 1s */
    10,     /* 10s */
    60,     /* 1m */
    3600    /* 1h */
};

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static void rolling_window_init(nimcp_rolling_window_t* window, uint32_t capacity) {
    memset(window, 0, sizeof(nimcp_rolling_window_t));
    window->capacity = capacity;
    window->window_start = time(NULL);
}

static void rolling_window_add(nimcp_rolling_window_t* window, double value, time_t timestamp) {
    /* Don't overwrite if we haven't reached capacity yet */
    if (window->count < window->capacity) {
        window->values[window->count] = value;
        window->count++;
    } else {
        /* Circular buffer behavior when at capacity */
        window->values[window->head] = value;
        window->head = (window->head + 1) % window->capacity;
    }
}

static void rolling_window_reset(nimcp_rolling_window_t* window) {
    window->head = 0;
    window->count = 0;
    window->window_start = time(NULL);
}

/* =============================================================================
 * Histogram Operations
 * ============================================================================= */

void nimcp_histogram_reset(nimcp_histogram_t* hist) {
    if (!hist) return;

    memset(hist->buckets, 0, sizeof(hist->buckets));
    hist->bucket_width = 0.0;
    hist->min_value = INFINITY;
    hist->max_value = -INFINITY;
    hist->total_count = 0;
}

void nimcp_histogram_add(nimcp_histogram_t* hist, double value) {
    if (!hist) return;

    /* Update min/max */
    if (value < hist->min_value) hist->min_value = value;
    if (value > hist->max_value) hist->max_value = value;

    hist->total_count++;

    /* Calculate range and bucket width */
    double range = hist->max_value - hist->min_value;

    /* Initialize or update bucket width based on current range */
    if (range > 0.0) {
        hist->bucket_width = range / NIMCP_HISTOGRAM_BUCKETS;
    } else {
        hist->bucket_width = 1.0;  /* Default when all values are the same */
    }

    /* Determine bucket */
    uint32_t bucket;
    if (range > 0.0) {
        bucket = (uint32_t)((value - hist->min_value) / hist->bucket_width);
        if (bucket >= NIMCP_HISTOGRAM_BUCKETS) {
            bucket = NIMCP_HISTOGRAM_BUCKETS - 1;
        }
    } else {
        bucket = 0;  /* All values are the same */
    }

    hist->buckets[bucket]++;
}

double nimcp_histogram_percentile(const nimcp_histogram_t* hist, double percentile) {
    if (!hist || hist->total_count == 0) {
        return 0.0;
    }

    if (percentile <= 0.0) return hist->min_value;
    if (percentile >= 1.0) return hist->max_value;

    /* Find the bucket containing the percentile */
    uint64_t target_count = (uint64_t)(hist->total_count * percentile);
    uint64_t cumulative_count = 0;

    for (uint32_t i = 0; i < NIMCP_HISTOGRAM_BUCKETS; i++) {
        cumulative_count += hist->buckets[i];
        if (cumulative_count >= target_count) {
            /* Percentile is in this bucket */
            /* Return middle of bucket */
            return hist->min_value + (i + 0.5) * hist->bucket_width;
        }
    }

    /* Fallback to max value */
    return hist->max_value;
}

/* =============================================================================
 * Core Aggregator Functions
 * ============================================================================= */

nimcp_metrics_aggregator_t* nimcp_metrics_aggregator_create(const char* metric_name) {
    if (!metric_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metric_name is NULL");

        return NULL;
    }

    nimcp_metrics_aggregator_t* agg =
        (nimcp_metrics_aggregator_t*)nimcp_calloc(1, sizeof(nimcp_metrics_aggregator_t));
    if (!agg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "agg is NULL");

        return NULL;
    }

    /* Set metric name */
    strncpy(agg->metric_name, metric_name, sizeof(agg->metric_name) - 1);
    agg->metric_name[sizeof(agg->metric_name) - 1] = '\0';

    /* Initialize windows */
    for (int i = 0; i < NIMCP_WINDOW_COUNT; i++) {
        rolling_window_init(&agg->windows[i], window_capacities[i]);
        nimcp_histogram_reset(&agg->histograms[i]);

        /* Initialize cached stats */
        agg->cached_stats[i].min = INFINITY;
        agg->cached_stats[i].max = -INFINITY;
        agg->cached_stats[i].avg = 0.0;
        agg->cached_stats[i].sum = 0.0;
        agg->cached_stats[i].p50 = 0.0;
        agg->cached_stats[i].p95 = 0.0;
        agg->cached_stats[i].p99 = 0.0;
        agg->cached_stats[i].count = 0;
    }

    /* Configuration */
    agg->auto_aggregate = true;
    agg->aggregate_interval = 1;  /* Aggregate every second by default */
    agg->last_aggregate_time = time(NULL);

    /* Statistics */
    agg->total_samples = 0;
    agg->aggregations_performed = 0;

    return agg;
}

void nimcp_metrics_aggregator_destroy(nimcp_metrics_aggregator_t* agg) {
    if (!agg) {
        return;
    }
    nimcp_free(agg);
}

bool nimcp_metrics_aggregator_add_sample(
    nimcp_metrics_aggregator_t* agg,
    double value,
    time_t timestamp
) {
    if (!agg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_aggregator_add_sample: agg is NULL");
        return false;
    }

    if (timestamp == 0) {
        timestamp = time(NULL);
    }

    /* Add to all windows */
    for (int i = 0; i < NIMCP_WINDOW_COUNT; i++) {
        nimcp_rolling_window_t* window = &agg->windows[i];

        /* Check if we need to rotate window */
        uint32_t window_duration = window_durations[i];
        if (timestamp - window->window_start >= window_duration) {
            rolling_window_reset(window);
        }

        /* Add sample to window */
        rolling_window_add(window, value, timestamp);

        /* Histogram will be rebuilt during aggregation for accuracy */
    }

    agg->total_samples++;

    /* Auto-aggregate if enabled and interval elapsed */
    if (agg->auto_aggregate) {
        if (agg->aggregate_interval == 0 ||
            (timestamp - agg->last_aggregate_time) >= agg->aggregate_interval) {
            nimcp_metrics_aggregator_aggregate(agg);
            agg->last_aggregate_time = timestamp;
        }
    }

    return true;
}

bool nimcp_metrics_aggregator_aggregate(nimcp_metrics_aggregator_t* agg) {
    if (!agg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_aggregator_aggregate: agg is NULL");
        return false;
    }

    time_t now = time(NULL);

    /* Aggregate each window */
    for (int i = 0; i < NIMCP_WINDOW_COUNT; i++) {
        nimcp_rolling_window_t* window = &agg->windows[i];
        nimcp_histogram_t* hist = &agg->histograms[i];
        nimcp_aggregated_metric_t* stats = &agg->cached_stats[i];

        stats->count = window->count;
        stats->window_start = window->window_start;
        stats->window_end = now;

        if (window->count == 0) {
            stats->min = 0.0;
            stats->max = 0.0;
            stats->avg = 0.0;
            stats->sum = 0.0;
            stats->p50 = 0.0;
            stats->p95 = 0.0;
            stats->p99 = 0.0;
            continue;
        }

        /* Calculate min/max/sum/avg */
        stats->min = INFINITY;
        stats->max = -INFINITY;
        stats->sum = 0.0;

        for (uint32_t j = 0; j < window->count; j++) {
            double val = window->values[j];
            if (val < stats->min) stats->min = val;
            if (val > stats->max) stats->max = val;
            stats->sum += val;
        }

        stats->avg = stats->sum / window->count;

        /* Rebuild histogram from current window values for accurate percentiles */
        nimcp_histogram_reset(hist);

        /* Pre-calculate range and bucket width to avoid issues with dynamic updates */
        double range = stats->max - stats->min;
        if (range > 0.0) {
            hist->bucket_width = range / NIMCP_HISTOGRAM_BUCKETS;
        } else {
            hist->bucket_width = 1.0;
        }
        hist->min_value = stats->min;
        hist->max_value = stats->max;

        /* Add all samples to histogram with fixed bucket width */
        for (uint32_t j = 0; j < window->count; j++) {
            double val = window->values[j];
            uint32_t bucket;
            if (range > 0.0) {
                bucket = (uint32_t)((val - hist->min_value) / hist->bucket_width);
                if (bucket >= NIMCP_HISTOGRAM_BUCKETS) {
                    bucket = NIMCP_HISTOGRAM_BUCKETS - 1;
                }
            } else {
                bucket = 0;
            }
            hist->buckets[bucket]++;
            hist->total_count++;
        }

        /* Calculate percentiles from histogram */
        stats->p50 = nimcp_histogram_percentile(hist, 0.50);
        stats->p95 = nimcp_histogram_percentile(hist, 0.95);
        stats->p99 = nimcp_histogram_percentile(hist, 0.99);
    }

    agg->aggregations_performed++;
    return true;
}

/* =============================================================================
 * Query Functions
 * ============================================================================= */

const nimcp_aggregated_metric_t* nimcp_metrics_aggregator_get_stats(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
) {
    if (!agg || window >= NIMCP_WINDOW_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_metrics_aggregator_get_stats: agg is NULL");
        return NULL;
    }
    return &agg->cached_stats[window];
}

double nimcp_metrics_aggregator_get_min(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
) {
    if (!agg || window >= NIMCP_WINDOW_COUNT) {
        return 0.0;
    }
    return agg->cached_stats[window].min;
}

double nimcp_metrics_aggregator_get_max(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
) {
    if (!agg || window >= NIMCP_WINDOW_COUNT) {
        return 0.0;
    }
    return agg->cached_stats[window].max;
}

double nimcp_metrics_aggregator_get_avg(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
) {
    if (!agg || window >= NIMCP_WINDOW_COUNT) {
        return 0.0;
    }
    return agg->cached_stats[window].avg;
}

double nimcp_metrics_aggregator_get_percentile(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window,
    double percentile
) {
    if (!agg || window >= NIMCP_WINDOW_COUNT) {
        return 0.0;
    }
    return nimcp_histogram_percentile(&agg->histograms[window], percentile);
}

uint64_t nimcp_metrics_aggregator_get_count(
    const nimcp_metrics_aggregator_t* agg,
    nimcp_time_window_t window
) {
    if (!agg || window >= NIMCP_WINDOW_COUNT) {
        return 0;
    }
    return agg->cached_stats[window].count;
}

/* =============================================================================
 * Configuration
 * ============================================================================= */

bool nimcp_metrics_aggregator_set_auto_aggregate(
    nimcp_metrics_aggregator_t* agg,
    bool enabled,
    uint32_t interval_seconds
) {
    if (!agg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_aggregator_set_auto_aggregate: agg is NULL");
        return false;
    }

    agg->auto_aggregate = enabled;
    agg->aggregate_interval = interval_seconds;
    return true;
}

void nimcp_metrics_aggregator_reset(nimcp_metrics_aggregator_t* agg) {
    if (!agg) {
        return;
    }

    /* Reset all windows */
    for (int i = 0; i < NIMCP_WINDOW_COUNT; i++) {
        rolling_window_reset(&agg->windows[i]);
        nimcp_histogram_reset(&agg->histograms[i]);

        /* Reset cached stats */
        agg->cached_stats[i].min = INFINITY;
        agg->cached_stats[i].max = -INFINITY;
        agg->cached_stats[i].avg = 0.0;
        agg->cached_stats[i].sum = 0.0;
        agg->cached_stats[i].p50 = 0.0;
        agg->cached_stats[i].p95 = 0.0;
        agg->cached_stats[i].p99 = 0.0;
        agg->cached_stats[i].count = 0;
    }

    /* Reset statistics */
    agg->total_samples = 0;
    agg->aggregations_performed = 0;
    agg->last_aggregate_time = time(NULL);
}

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

const char* nimcp_window_to_string(nimcp_time_window_t window) {
    switch (window) {
        case NIMCP_WINDOW_1S:  return "1s";
        case NIMCP_WINDOW_10S: return "10s";
        case NIMCP_WINDOW_1M:  return "1m";
        case NIMCP_WINDOW_1H:  return "1h";
        default:               return "UNKNOWN";
    }
}

uint32_t nimcp_window_duration(nimcp_time_window_t window) {
    if (window >= NIMCP_WINDOW_COUNT) {
        return 0;
    }
    return window_durations[window];
}

bool nimcp_metrics_aggregator_get_statistics(
    const nimcp_metrics_aggregator_t* agg,
    uint64_t* total_samples,
    uint64_t* aggregations
) {
    if (!agg || !total_samples || !aggregations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_aggregator_get_statistics: required parameter is NULL (agg, total_samples, aggregations)");
        return false;
    }

    *total_samples = agg->total_samples;
    *aggregations = agg->aggregations_performed;
    return true;
}
