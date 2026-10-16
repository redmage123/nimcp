/**
 * @file nimcp_immune_metrics.c
 * @brief Immune System Metrics Implementation - Prometheus/JSON Export
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Implementation of metrics collection and export for self-healing system
 * WHY:  Enable production monitoring and alerting for crash recovery
 * HOW:  Thread-safe counters, histograms, Prometheus/JSON formatting
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_immune_metrics.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "cognitive/immune/nimcp_heal_patterns.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(immune_metrics, MESH_ADAPTER_CATEGORY_SECURITY)



/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "ImmuneMetrics"

/* Default histogram buckets for latency (microseconds) */
static const double DEFAULT_LATENCY_BUCKETS[] = {
    100.0,      /* 100us */
    500.0,      /* 500us */
    1000.0,     /* 1ms */
    5000.0,     /* 5ms */
    10000.0,    /* 10ms */
    50000.0,    /* 50ms */
    100000.0,   /* 100ms */
    500000.0    /* 500ms */
};
static const size_t N_DEFAULT_LATENCY_BUCKETS = 8;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void)
{
    return get_time_us() / 1000ULL;
}

/**
 * @brief Initialize histogram with default latency buckets
 */
static void init_latency_histogram(histogram_data_t* hist)
{
    if (hist == NULL) return;

    memset(hist, 0, sizeof(histogram_data_t));
    hist->n_buckets = N_DEFAULT_LATENCY_BUCKETS;

    for (size_t i = 0; i < N_DEFAULT_LATENCY_BUCKETS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && N_DEFAULT_LATENCY_BUCKETS > 256) {
            immune_metrics_heartbeat("immune_metri_loop",
                             (float)(i + 1) / (float)N_DEFAULT_LATENCY_BUCKETS);
        }

        hist->buckets[i].upper_bound = DEFAULT_LATENCY_BUCKETS[i];
        hist->buckets[i].count = 0;
    }
}

/**
 * @brief Add observation to histogram
 */
static void histogram_observe(histogram_data_t* hist, double value)
{
    if (hist == NULL) return;

    /* Update all buckets where value <= upper_bound */
    for (size_t i = 0; i < hist->n_buckets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->n_buckets > 256) {
            immune_metrics_heartbeat("immune_metri_loop",
                             (float)(i + 1) / (float)hist->n_buckets);
        }

        if (value <= hist->buckets[i].upper_bound) {
            hist->buckets[i].count++;
        }
    }

    hist->total_count++;
    hist->sum += value;
}

/**
 * @brief Write Prometheus metric header
 */
static int write_metric_header(
    char* buffer,
    size_t buffer_size,
    size_t* offset,
    const char* name,
    const char* type,
    const char* help)
{
    if (buffer == NULL || offset == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "write_metric_header: invalid parameter");
        return -1;
    }

    int written = snprintf(buffer + *offset, buffer_size - *offset,
        "# HELP %s %s\n# TYPE %s %s\n",
        name, help, name, type);

    if (written < 0 || (size_t)written >= buffer_size - *offset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_metric_header: capacity exceeded");
        return -1;
    }

    *offset += (size_t)written;
    return 0;
}

/**
 * @brief Write Prometheus counter metric
 */
static int write_counter(
    char* buffer,
    size_t buffer_size,
    size_t* offset,
    const char* name,
    const char* labels,
    uint64_t value)
{
    if (buffer == NULL || offset == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "write_counter: invalid parameter");
        return -1;
    }

    int written = 0;
    if (labels != NULL && labels[0] != '\0') {
        written = snprintf(buffer + *offset, buffer_size - *offset,
            "%s{%s} %lu\n", name, labels, (unsigned long)value);
    } else {
        written = snprintf(buffer + *offset, buffer_size - *offset,
            "%s %lu\n", name, (unsigned long)value);
    }

    if (written < 0 || (size_t)written >= buffer_size - *offset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_counter: capacity exceeded");
        return -1;
    }

    *offset += (size_t)written;
    return 0;
}

/**
 * @brief Write Prometheus gauge metric
 */
static int write_gauge(
    char* buffer,
    size_t buffer_size,
    size_t* offset,
    const char* name,
    const char* labels,
    double value)
{
    if (buffer == NULL || offset == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "write_gauge: invalid parameter");
        return -1;
    }

    int written = 0;
    if (labels != NULL && labels[0] != '\0') {
        written = snprintf(buffer + *offset, buffer_size - *offset,
            "%s{%s} %.6f\n", name, labels, value);
    } else {
        written = snprintf(buffer + *offset, buffer_size - *offset,
            "%s %.6f\n", name, value);
    }

    if (written < 0 || (size_t)written >= buffer_size - *offset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_gauge: capacity exceeded");
        return -1;
    }

    *offset += (size_t)written;
    return 0;
}

/**
 * @brief Write Prometheus histogram metric
 */
static int write_histogram(
    char* buffer,
    size_t buffer_size,
    size_t* offset,
    const char* name,
    const histogram_data_t* hist)
{
    if (buffer == NULL || offset == NULL || hist == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "write_histogram: invalid parameter");
        return -1;
    }

    int written = 0;

    /* Write buckets */
    for (size_t i = 0; i < hist->n_buckets; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && hist->n_buckets > 256) {
            immune_metrics_heartbeat("immune_metri_loop",
                             (float)(i + 1) / (float)hist->n_buckets);
        }

        if (hist->buckets[i].upper_bound == INFINITY) {
            written = snprintf(buffer + *offset, buffer_size - *offset,
                "%s_bucket{le=\"+Inf\"} %lu\n",
                name, (unsigned long)hist->buckets[i].count);
        } else {
            written = snprintf(buffer + *offset, buffer_size - *offset,
                "%s_bucket{le=\"%.0f\"} %lu\n",
                name, hist->buckets[i].upper_bound,
                (unsigned long)hist->buckets[i].count);
        }
        if (written < 0 || (size_t)written >= buffer_size - *offset) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_histogram: capacity exceeded");
            return -1;
        }
        *offset += (size_t)written;
    }

    /* Write +Inf bucket (total count) */
    written = snprintf(buffer + *offset, buffer_size - *offset,
        "%s_bucket{le=\"+Inf\"} %lu\n",
        name, (unsigned long)hist->total_count);
    if (written < 0 || (size_t)written >= buffer_size - *offset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_histogram: capacity exceeded");
        return -1;
    }
    *offset += (size_t)written;

    /* Write sum and count */
    written = snprintf(buffer + *offset, buffer_size - *offset,
        "%s_sum %.6f\n%s_count %lu\n",
        name, hist->sum, name, (unsigned long)hist->total_count);
    if (written < 0 || (size_t)written >= buffer_size - *offset) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_histogram: capacity exceeded");
        return -1;
    }
    *offset += (size_t)written;

    return 0;
}

/**
 * @brief Calculate success rate
 */
static float calculate_success_rate(uint64_t successes, uint64_t total)
{
    if (total == 0) return 0.0f;
    return (float)successes / (float)total;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int immune_metrics_default_config(immune_metrics_config_t* config)
{
    if (config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_default_config: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_default_config", 0.0f);


    memset(config, 0, sizeof(immune_metrics_config_t));

    config->enable_histogram = true;
    config->enable_http_server = false;
    config->http_port = IMMUNE_METRICS_DEFAULT_PORT;
    config->update_interval_ms = 1000;
    config->enable_alerting = true;

    /* Default alert thresholds */
    config->alert_config.failure_threshold = 5;
    config->alert_config.success_rate_threshold = 0.5f;
    config->alert_config.crash_rate_threshold = 10.0f;  /* crashes per minute */
    config->alert_config.memory_threshold_bytes = 100 * 1024 * 1024;  /* 100MB */
    config->alert_config.b_cell_min_count = 10;
    config->alert_config.pattern_effectiveness_min = 0.3f;
    config->alert_config.check_interval_ms = 10000;  /* 10 seconds */

    return 0;
}

immune_metrics_t* immune_metrics_create(const immune_metrics_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_create", 0.0f);


    immune_metrics_t* metrics = nimcp_calloc(1, sizeof(immune_metrics_t));
    if (metrics == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to allocate metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "immune_metrics_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        metrics->config = *config;
    } else {
        immune_metrics_default_config(&metrics->config);
    }

    /* Create mutex */
    metrics->mutex = nimcp_mutex_create(NULL);
    if (metrics->mutex == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to create mutex");
        nimcp_free(metrics);
        metrics = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "immune_metrics_create: validation failed");
        return NULL;
    }

    /* Initialize histogram */
    if (metrics->config.enable_histogram) {
        init_latency_histogram(&metrics->fix_latency);
    }

    metrics->start_time = get_time_ms();
    metrics->initialized = true;

    LOG_MODULE_INFO(LOG_TAG, "Immune metrics collector initialized");

    return metrics;
}

void immune_metrics_destroy(immune_metrics_t* metrics)
{
    if (metrics == NULL) return;

    /* Stop HTTP server if running */
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_destroy", 0.0f);


    if (metrics->http_running) {
        immune_metrics_stop_http_server(metrics);
    }

    nimcp_mutex_lock(metrics->mutex);
    metrics->initialized = false;
    nimcp_mutex_unlock(metrics->mutex);

    nimcp_mutex_destroy(metrics->mutex);
    nimcp_free(metrics);
    metrics = NULL;

    LOG_MODULE_INFO(LOG_TAG, "Immune metrics collector destroyed");
}

int immune_metrics_record_crash(
    immune_metrics_t* metrics,
    crash_signal_type_t signal_type)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_record_crash: invalid parameter");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_record_crash", 0.0f);


    if (signal_type < 0 || signal_type >= CRASH_SIGNAL_COUNT) {
        signal_type = CRASH_SIGNAL_OTHER;
    }

    nimcp_mutex_lock(metrics->mutex);

    metrics->crashes.count[signal_type]++;
    metrics->crashes.total++;
    metrics->crashes.last_timestamp = get_time_ms();

    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_record_fix(
    immune_metrics_t* metrics,
    bool success,
    bool pattern_based,
    bool lnn_based,
    float confidence)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_record_fix: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_record_fix", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    metrics->fixes.total_attempts++;

    if (success) {
        metrics->fixes.successes++;
        metrics->consecutive_failures = 0;

        if (pattern_based && lnn_based) {
            metrics->fixes.hybrid_fixes++;
        } else if (pattern_based) {
            metrics->fixes.pattern_fixes++;
        } else if (lnn_based) {
            metrics->fixes.lnn_fixes++;
        }
    } else {
        metrics->fixes.failures++;
        metrics->consecutive_failures++;
    }

    /* Update running average confidence */
    float n = (float)metrics->fixes.total_attempts;
    metrics->fixes.avg_confidence =
        (metrics->fixes.avg_confidence * (n - 1) + confidence) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Update success rate */
    metrics->fixes.success_rate = calculate_success_rate(
        metrics->fixes.successes, metrics->fixes.total_attempts);

    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_record_latency(
    immune_metrics_t* metrics,
    uint64_t latency_us)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_record_latency: invalid parameter");
        return -1;
    }
    if (!metrics->config.enable_histogram) return 0;

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_record_latency", 0.0f);


    nimcp_mutex_lock(metrics->mutex);
    histogram_observe(&metrics->fix_latency, (double)latency_us);
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_record_pattern_use(
    immune_metrics_t* metrics,
    int pattern_type)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_record_pattern_use: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_record_pattern_use", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    switch (pattern_type) {
        case FIX_PATTERN_NULL_CHECK:
            metrics->patterns.null_check_uses++;
            break;
        case FIX_PATTERN_BOUNDS_CHECK:
            metrics->patterns.bounds_check_uses++;
            break;
        case FIX_PATTERN_ZERO_CHECK:
            metrics->patterns.zero_check_uses++;
            break;
        case FIX_PATTERN_UAF_CHECK:
            metrics->patterns.uaf_check_uses++;
            break;
        case FIX_PATTERN_ALIGN_FIX:
            metrics->patterns.align_fix_uses++;
            break;
        case FIX_PATTERN_DOUBLE_FREE:
            metrics->patterns.double_free_uses++;
            break;
        case FIX_PATTERN_OVERFLOW_CHECK:
            metrics->patterns.overflow_check_uses++;
            break;
        case FIX_PATTERN_LNN_GENERATED:
            metrics->patterns.lnn_generated_uses++;
            break;
        case FIX_PATTERN_CUSTOM:
            metrics->patterns.custom_pattern_uses++;
            break;
        default:
            break;
    }

    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_update_b_cells(
    immune_metrics_t* metrics,
    const bcell_metrics_t* bcell_metrics)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_update_b_cells: invalid parameter");
        return -1;
    }
    if (bcell_metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_update_b_cells: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_update_b_cells", 0.0f);


    nimcp_mutex_lock(metrics->mutex);
    metrics->b_cells = *bcell_metrics;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_update_antibodies(
    immune_metrics_t* metrics,
    const antibody_metrics_t* antibody_metrics)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_update_antibodies: invalid parameter");
        return -1;
    }
    if (antibody_metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_update_antibodies: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_update_antibodies", 0.0f);


    nimcp_mutex_lock(metrics->mutex);
    metrics->antibodies = *antibody_metrics;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_update_memory(
    immune_metrics_t* metrics,
    const memory_metrics_t* memory_metrics)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_update_memory: invalid parameter");
        return -1;
    }
    if (memory_metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_update_memory: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_update_memory", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    metrics->memory = *memory_metrics;

    /* Track peak usage */
    if (memory_metrics->total_allocated > metrics->memory.peak_usage) {
        metrics->memory.peak_usage = memory_metrics->total_allocated;
    }

    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_export_prometheus(
    immune_metrics_t* metrics,
    char* buffer,
    size_t buffer_size)
{
    if (metrics == NULL || buffer == NULL || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_export_prometheus: invalid parameter");
        return -1;
    }
    if (!metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_export_prometheus: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_export_prometheus", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    size_t offset = 0;
    char label_buf[NIMCP_NAME_BUFFER_SIZE];

    /* ====== Crash Metrics ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_crashes_total",
            "counter",
            "Total crash events by signal type") != 0) {
        goto error;
    }

    for (int i = 0; i < CRASH_SIGNAL_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && CRASH_SIGNAL_COUNT > 256) {
            immune_metrics_heartbeat("immune_metri_loop",
                             (float)(i + 1) / (float)CRASH_SIGNAL_COUNT);
        }

        snprintf(label_buf, sizeof(label_buf), "signal=\"%s\"",
                 immune_metrics_signal_to_string((crash_signal_type_t)i));
        if (write_counter(buffer, buffer_size, &offset,
                "nimcp_immune_crashes_total", label_buf,
                metrics->crashes.count[i]) != 0) {
            goto error;
        }
    }

    /* ====== Fix Metrics ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_total",
            "counter",
            "Total fix attempts by result") != 0) {
        goto error;
    }

    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_total", "result=\"success\"",
            metrics->fixes.successes) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_total", "result=\"failure\"",
            metrics->fixes.failures) != 0) {
        goto error;
    }

    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_by_source",
            "counter",
            "Fixes by source (pattern, lnn, hybrid)") != 0) {
        goto error;
    }

    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_by_source", "source=\"pattern\"",
            metrics->fixes.pattern_fixes) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_by_source", "source=\"lnn\"",
            metrics->fixes.lnn_fixes) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_fixes_by_source", "source=\"hybrid\"",
            metrics->fixes.hybrid_fixes) != 0) {
        goto error;
    }

    /* Fix success rate gauge */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_fix_success_rate",
            "gauge",
            "Current fix success rate (0-1)") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_fix_success_rate", NULL,
            (double)metrics->fixes.success_rate) != 0) {
        goto error;
    }

    /* Fix confidence gauge */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_fix_avg_confidence",
            "gauge",
            "Average fix confidence score (0-1)") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_fix_avg_confidence", NULL,
            (double)metrics->fixes.avg_confidence) != 0) {
        goto error;
    }

    /* ====== Fix Latency Histogram ====== */
    if (metrics->config.enable_histogram && metrics->fix_latency.total_count > 0) {
        if (write_metric_header(buffer, buffer_size, &offset,
                "nimcp_immune_fix_latency_us",
                "histogram",
                "Fix generation latency in microseconds") != 0) {
            goto error;
        }
        if (write_histogram(buffer, buffer_size, &offset,
                "nimcp_immune_fix_latency_us", &metrics->fix_latency) != 0) {
            goto error;
        }
    }

    /* ====== Pattern Usage Metrics ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total",
            "counter",
            "Pattern usage counts by type") != 0) {
        goto error;
    }

    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"null_check\"",
            metrics->patterns.null_check_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"bounds_check\"",
            metrics->patterns.bounds_check_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"zero_check\"",
            metrics->patterns.zero_check_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"uaf_check\"",
            metrics->patterns.uaf_check_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"align_fix\"",
            metrics->patterns.align_fix_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"double_free\"",
            metrics->patterns.double_free_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"overflow_check\"",
            metrics->patterns.overflow_check_uses) != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_pattern_uses_total", "pattern=\"lnn_generated\"",
            metrics->patterns.lnn_generated_uses) != 0) {
        goto error;
    }

    /* ====== B-cell Metrics ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_total",
            "gauge",
            "Total B-cells created") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_total", NULL,
            (double)metrics->b_cells.total_created) != 0) {
        goto error;
    }

    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_current",
            "gauge",
            "Current B-cell counts by state") != 0) {
        goto error;
    }

    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_current", "state=\"naive\"",
            (double)metrics->b_cells.naive_count) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_current", "state=\"activated\"",
            (double)metrics->b_cells.activated_count) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_current", "state=\"plasma\"",
            (double)metrics->b_cells.plasma_count) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_bcells_current", "state=\"memory\"",
            (double)metrics->b_cells.memory_count) != 0) {
        goto error;
    }

    /* ====== Antibody Metrics ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_antibodies_produced",
            "counter",
            "Total antibodies produced") != 0) {
        goto error;
    }
    if (write_counter(buffer, buffer_size, &offset,
            "nimcp_immune_antibodies_produced", NULL,
            metrics->antibodies.total_produced) != 0) {
        goto error;
    }

    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_antibodies_active",
            "gauge",
            "Currently active antibodies by class") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_antibodies_active", "class=\"igm\"",
            (double)metrics->antibodies.igm_count) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_antibodies_active", "class=\"igg\"",
            (double)metrics->antibodies.igg_count) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_antibodies_active", "class=\"ige\"",
            (double)metrics->antibodies.ige_count) != 0) {
        goto error;
    }

    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_antibody_effectiveness",
            "gauge",
            "Average antibody effectiveness (0-1)") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_antibody_effectiveness", NULL,
            (double)metrics->antibodies.avg_effectiveness) != 0) {
        goto error;
    }

    /* ====== Memory Metrics ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_memory_bytes",
            "gauge",
            "Memory usage by component") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_memory_bytes", "component=\"total\"",
            (double)metrics->memory.total_allocated) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_memory_bytes", "component=\"training\"",
            (double)metrics->memory.training_samples) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_memory_bytes", "component=\"patterns\"",
            (double)metrics->memory.pattern_library) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_memory_bytes", "component=\"lnn\"",
            (double)metrics->memory.lnn_network) != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_memory_bytes", "component=\"peak\"",
            (double)metrics->memory.peak_usage) != 0) {
        goto error;
    }

    /* ====== Uptime ====== */
    if (write_metric_header(buffer, buffer_size, &offset,
            "nimcp_immune_uptime_ms",
            "gauge",
            "Immune system uptime in milliseconds") != 0) {
        goto error;
    }
    if (write_gauge(buffer, buffer_size, &offset,
            "nimcp_immune_uptime_ms", NULL,
            (double)(get_time_ms() - metrics->start_time)) != 0) {
        goto error;
    }

    nimcp_mutex_unlock(metrics->mutex);
    return (int)offset;

error:
    nimcp_mutex_unlock(metrics->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
    return -1;
}

int immune_metrics_export_json(
    immune_metrics_t* metrics,
    char* buffer,
    size_t buffer_size)
{
    if (metrics == NULL || buffer == NULL || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_export_json: invalid parameter");
        return -1;
    }
    if (!metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_export_json: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_export_json", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    int written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"crashes\": {\n"
        "    \"total\": %lu,\n"
        "    \"by_signal\": {\n"
        "      \"sigsegv\": %lu,\n"
        "      \"sigfpe\": %lu,\n"
        "      \"sigbus\": %lu,\n"
        "      \"sigabrt\": %lu,\n"
        "      \"sigill\": %lu,\n"
        "      \"sigtrap\": %lu,\n"
        "      \"other\": %lu\n"
        "    },\n"
        "    \"last_timestamp_ms\": %lu\n"
        "  },\n"
        "  \"fixes\": {\n"
        "    \"total_attempts\": %lu,\n"
        "    \"successes\": %lu,\n"
        "    \"failures\": %lu,\n"
        "    \"pattern_fixes\": %lu,\n"
        "    \"lnn_fixes\": %lu,\n"
        "    \"hybrid_fixes\": %lu,\n"
        "    \"success_rate\": %.4f,\n"
        "    \"avg_confidence\": %.4f\n"
        "  },\n"
        "  \"patterns\": {\n"
        "    \"null_check\": %lu,\n"
        "    \"bounds_check\": %lu,\n"
        "    \"zero_check\": %lu,\n"
        "    \"uaf_check\": %lu,\n"
        "    \"align_fix\": %lu,\n"
        "    \"double_free\": %lu,\n"
        "    \"overflow_check\": %lu,\n"
        "    \"lnn_generated\": %lu,\n"
        "    \"custom\": %lu\n"
        "  },\n"
        "  \"b_cells\": {\n"
        "    \"total_created\": %lu,\n"
        "    \"current_count\": %lu,\n"
        "    \"naive\": %lu,\n"
        "    \"activated\": %lu,\n"
        "    \"plasma\": %lu,\n"
        "    \"memory\": %lu,\n"
        "    \"avg_affinity\": %.4f\n"
        "  },\n"
        "  \"antibodies\": {\n"
        "    \"total_produced\": %lu,\n"
        "    \"current_active\": %lu,\n"
        "    \"igm\": %lu,\n"
        "    \"igg\": %lu,\n"
        "    \"ige\": %lu,\n"
        "    \"neutralizations\": %lu,\n"
        "    \"avg_effectiveness\": %.4f\n"
        "  },\n"
        "  \"memory\": {\n"
        "    \"total_allocated_bytes\": %zu,\n"
        "    \"training_samples_bytes\": %zu,\n"
        "    \"pattern_library_bytes\": %zu,\n"
        "    \"lnn_network_bytes\": %zu,\n"
        "    \"peak_usage_bytes\": %zu\n"
        "  },\n"
        "  \"latency_histogram\": {\n"
        "    \"count\": %lu,\n"
        "    \"sum_us\": %.2f\n"
        "  },\n"
        "  \"uptime_ms\": %lu\n"
        "}\n",
        /* crashes */
        (unsigned long)metrics->crashes.total,
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_SIGSEGV],
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_SIGFPE],
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_SIGBUS],
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_SIGABRT],
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_SIGILL],
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_SIGTRAP],
        (unsigned long)metrics->crashes.count[CRASH_SIGNAL_OTHER],
        (unsigned long)metrics->crashes.last_timestamp,
        /* fixes */
        (unsigned long)metrics->fixes.total_attempts,
        (unsigned long)metrics->fixes.successes,
        (unsigned long)metrics->fixes.failures,
        (unsigned long)metrics->fixes.pattern_fixes,
        (unsigned long)metrics->fixes.lnn_fixes,
        (unsigned long)metrics->fixes.hybrid_fixes,
        metrics->fixes.success_rate,
        metrics->fixes.avg_confidence,
        /* patterns */
        (unsigned long)metrics->patterns.null_check_uses,
        (unsigned long)metrics->patterns.bounds_check_uses,
        (unsigned long)metrics->patterns.zero_check_uses,
        (unsigned long)metrics->patterns.uaf_check_uses,
        (unsigned long)metrics->patterns.align_fix_uses,
        (unsigned long)metrics->patterns.double_free_uses,
        (unsigned long)metrics->patterns.overflow_check_uses,
        (unsigned long)metrics->patterns.lnn_generated_uses,
        (unsigned long)metrics->patterns.custom_pattern_uses,
        /* b_cells */
        (unsigned long)metrics->b_cells.total_created,
        (unsigned long)metrics->b_cells.current_count,
        (unsigned long)metrics->b_cells.naive_count,
        (unsigned long)metrics->b_cells.activated_count,
        (unsigned long)metrics->b_cells.plasma_count,
        (unsigned long)metrics->b_cells.memory_count,
        metrics->b_cells.avg_affinity,
        /* antibodies */
        (unsigned long)metrics->antibodies.total_produced,
        (unsigned long)metrics->antibodies.current_active,
        (unsigned long)metrics->antibodies.igm_count,
        (unsigned long)metrics->antibodies.igg_count,
        (unsigned long)metrics->antibodies.ige_count,
        (unsigned long)metrics->antibodies.neutralizations,
        metrics->antibodies.avg_effectiveness,
        /* memory */
        metrics->memory.total_allocated,
        metrics->memory.training_samples,
        metrics->memory.pattern_library,
        metrics->memory.lnn_network,
        metrics->memory.peak_usage,
        /* latency */
        (unsigned long)metrics->fix_latency.total_count,
        metrics->fix_latency.sum,
        /* uptime */
        (unsigned long)(get_time_ms() - metrics->start_time)
    );

    nimcp_mutex_unlock(metrics->mutex);

    if (written < 0 || (size_t)written >= buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "immune_metrics_export_json: capacity exceeded");
        return -1;
    }

    return written;
}

int immune_metrics_get_snapshot(
    immune_metrics_t* metrics,
    immune_metrics_snapshot_t* snapshot)
{
    if (metrics == NULL || snapshot == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_get_snapshot: invalid parameter");
        return -1;
    }
    if (!metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_get_snapshot: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_get_snapshot", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    memset(snapshot, 0, sizeof(immune_metrics_snapshot_t));

    snapshot->crashes = metrics->crashes;
    snapshot->fixes = metrics->fixes;
    snapshot->patterns = metrics->patterns;
    snapshot->b_cells = metrics->b_cells;
    snapshot->antibodies = metrics->antibodies;
    snapshot->memory = metrics->memory;
    snapshot->fix_latency = metrics->fix_latency;

    snapshot->uptime_ms = get_time_ms() - metrics->start_time;
    snapshot->last_update_time = get_time_ms();
    snapshot->engine_initialized = metrics->initialized;

    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

/* ============================================================================
 * HTTP Server Implementation (Stub - Full impl would require socket handling)
 * ============================================================================ */

int immune_metrics_start_http_server(immune_metrics_t* metrics)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_start_http_server: invalid parameter");
        return -1;
    }
    if (!metrics->config.enable_http_server) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_start_http_server: invalid parameter");
        return -1;
    }

    /* HTTP server implementation would require socket handling
     * This is a stub - full implementation would:
     * 1. Create listening socket on configured port
     * 2. Accept connections in a thread
     * 3. Serve Prometheus format on /metrics endpoint
     */

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_start_http_server", 0.0f);


    LOG_MODULE_WARN(LOG_TAG,
        "HTTP server not fully implemented - use export functions directly");

    metrics->http_running = false;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "immune_metrics_start_http_server: operation failed");
    return -1;  /* Not implemented */
}

int immune_metrics_stop_http_server(immune_metrics_t* metrics)
{
    if (metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_stop_http_server: invalid parameter");
        return -1;
    }
    if (!metrics->http_running) return 0;

    /* Stop HTTP server thread and close socket */
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_stop_http_server", 0.0f);


    metrics->http_running = false;

    return 0;
}

bool immune_metrics_http_is_running(immune_metrics_t* metrics)
{
    if (metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_http_is_running: invalid parameter");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_http_is_running", 0.0f);


    return metrics->http_running;
}

/* ============================================================================
 * Alerting Implementation
 * ============================================================================ */

int immune_metrics_set_alert_callback(
    immune_metrics_t* metrics,
    immune_alert_callback_t callback,
    void* user_data)
{
    if (metrics == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_set_alert_callback: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_set_alert_callback", 0.0f);


    nimcp_mutex_lock(metrics->mutex);
    metrics->alert_callback = callback;
    metrics->alert_user_data = user_data;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_configure_alerts(
    immune_metrics_t* metrics,
    const alert_config_t* config)
{
    if (metrics == NULL || config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_configure_alerts: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_configure_alerts", 0.0f);


    nimcp_mutex_lock(metrics->mutex);
    metrics->config.alert_config = *config;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int immune_metrics_check_alerts(immune_metrics_t* metrics)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_check_alerts: invalid parameter");
        return -1;
    }
    if (!metrics->config.enable_alerting) return 0;
    if (metrics->alert_callback == NULL) return 0;

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_check_alerts", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    int alerts_triggered = 0;
    alert_config_t* cfg = &metrics->config.alert_config;
    char message[NIMCP_ERROR_BUFFER_SIZE];

    /* Check consecutive failures */
    if (metrics->consecutive_failures >= cfg->failure_threshold) {
        snprintf(message, sizeof(message),
            "Consecutive fix failures: %u (threshold: %u)",
            metrics->consecutive_failures, cfg->failure_threshold);

        nimcp_mutex_unlock(metrics->mutex);
        metrics->alert_callback(
            ALERT_REPEATED_FAILURES,
            ALERT_SEVERITY_WARNING,
            message,
            metrics->alert_user_data);
        nimcp_mutex_lock(metrics->mutex);
        alerts_triggered++;
    }

    /* Check success rate */
    if (metrics->fixes.total_attempts > 10 &&
        metrics->fixes.success_rate < cfg->success_rate_threshold) {
        snprintf(message, sizeof(message),
            "Low fix success rate: %.2f%% (threshold: %.2f%%)",
            metrics->fixes.success_rate * 100.0f,
            cfg->success_rate_threshold * 100.0f);

        nimcp_mutex_unlock(metrics->mutex);
        metrics->alert_callback(
            ALERT_LOW_SUCCESS_RATE,
            ALERT_SEVERITY_WARNING,
            message,
            metrics->alert_user_data);
        nimcp_mutex_lock(metrics->mutex);
        alerts_triggered++;
    }

    /* Check memory usage */
    if (metrics->memory.total_allocated > cfg->memory_threshold_bytes) {
        snprintf(message, sizeof(message),
            "High memory usage: %zu bytes (threshold: %zu)",
            metrics->memory.total_allocated, cfg->memory_threshold_bytes);

        nimcp_mutex_unlock(metrics->mutex);
        metrics->alert_callback(
            ALERT_MEMORY_PRESSURE,
            ALERT_SEVERITY_WARNING,
            message,
            metrics->alert_user_data);
        nimcp_mutex_lock(metrics->mutex);
        alerts_triggered++;
    }

    /* Check B-cell pool */
    if (metrics->b_cells.current_count < cfg->b_cell_min_count &&
        metrics->b_cells.total_created > 0) {
        snprintf(message, sizeof(message),
            "Low B-cell count: %lu (minimum: %u)",
            (unsigned long)metrics->b_cells.current_count,
            cfg->b_cell_min_count);

        nimcp_mutex_unlock(metrics->mutex);
        metrics->alert_callback(
            ALERT_B_CELL_EXHAUSTION,
            ALERT_SEVERITY_CRITICAL,
            message,
            metrics->alert_user_data);
        nimcp_mutex_lock(metrics->mutex);
        alerts_triggered++;
    }

    /* Check antibody effectiveness */
    if (metrics->antibodies.total_produced > 10 &&
        metrics->antibodies.avg_effectiveness < cfg->pattern_effectiveness_min) {
        snprintf(message, sizeof(message),
            "Low antibody effectiveness: %.2f%% (threshold: %.2f%%)",
            metrics->antibodies.avg_effectiveness * 100.0f,
            cfg->pattern_effectiveness_min * 100.0f);

        nimcp_mutex_unlock(metrics->mutex);
        metrics->alert_callback(
            ALERT_PATTERN_DEGRADATION,
            ALERT_SEVERITY_WARNING,
            message,
            metrics->alert_user_data);
        nimcp_mutex_lock(metrics->mutex);
        alerts_triggered++;
    }

    metrics->last_alert_time = get_time_ms();

    nimcp_mutex_unlock(metrics->mutex);

    return alerts_triggered;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

int immune_metrics_reset(immune_metrics_t* metrics)
{
    if (metrics == NULL || !metrics->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "immune_metrics_reset: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_reset", 0.0f);


    nimcp_mutex_lock(metrics->mutex);

    memset(&metrics->crashes, 0, sizeof(crash_stats_t));
    memset(&metrics->fixes, 0, sizeof(fix_stats_t));
    memset(&metrics->patterns, 0, sizeof(pattern_stats_t));
    memset(&metrics->b_cells, 0, sizeof(bcell_metrics_t));
    memset(&metrics->antibodies, 0, sizeof(antibody_metrics_t));
    memset(&metrics->memory, 0, sizeof(memory_metrics_t));

    init_latency_histogram(&metrics->fix_latency);

    metrics->consecutive_failures = 0;
    metrics->start_time = get_time_ms();

    nimcp_mutex_unlock(metrics->mutex);

    LOG_MODULE_INFO(LOG_TAG, "Metrics reset");

    return 0;
}

const char* immune_metrics_signal_to_string(crash_signal_type_t signal_type)
{
    switch (signal_type) {
        case CRASH_SIGNAL_SIGSEGV: return "SIGSEGV";
        case CRASH_SIGNAL_SIGFPE:  return "SIGFPE";
        case CRASH_SIGNAL_SIGBUS:  return "SIGBUS";
        case CRASH_SIGNAL_SIGABRT: return "SIGABRT";
        case CRASH_SIGNAL_SIGILL:  return "SIGILL";
        case CRASH_SIGNAL_SIGTRAP: return "SIGTRAP";
        case CRASH_SIGNAL_OTHER:   return "OTHER";
        default:                   return "UNKNOWN";
    }
}

const char* immune_metrics_alert_to_string(alert_type_t alert_type)
{
    switch (alert_type) {
        case ALERT_REPEATED_FAILURES:    return "repeated_failures";
        case ALERT_RESOURCE_EXHAUSTION:  return "resource_exhaustion";
        case ALERT_HIGH_CRASH_RATE:      return "high_crash_rate";
        case ALERT_LOW_SUCCESS_RATE:     return "low_success_rate";
        case ALERT_PATTERN_DEGRADATION:  return "pattern_degradation";
        case ALERT_LNN_TRAINING_STALL:   return "lnn_training_stall";
        case ALERT_MEMORY_PRESSURE:      return "memory_pressure";
        case ALERT_B_CELL_EXHAUSTION:    return "b_cell_exhaustion";
        default:                         return "unknown";
    }
}

const char* immune_metrics_severity_to_string(alert_severity_t severity)
{
    switch (severity) {
        case ALERT_SEVERITY_INFO:      return "info";
        case ALERT_SEVERITY_WARNING:   return "warning";
        case ALERT_SEVERITY_CRITICAL:  return "critical";
        case ALERT_SEVERITY_EMERGENCY: return "emergency";
        default:                       return "unknown";
    }
}

uint64_t immune_metrics_get_uptime(immune_metrics_t* metrics)
{
    if (metrics == NULL || !metrics->initialized) return 0;
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_get_uptime", 0.0f);


    return get_time_ms() - metrics->start_time;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about immune metrics
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int immune_metrics_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    immune_metrics_heartbeat("immune_metri_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Immune_Metrics");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                immune_metrics_heartbeat("immune_metri_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Immune metrics self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Immune_Metrics");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Immune_Metrics");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void immune_metrics_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_immune_metrics_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int immune_metrics_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_metrics_training_begin: NULL argument");
        return -1;
    }
    immune_metrics_heartbeat_instance(NULL, "immune_metrics_training_begin", 0.0f);
    return 0;
}

int immune_metrics_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_metrics_training_end: NULL argument");
        return -1;
    }
    immune_metrics_heartbeat_instance(NULL, "immune_metrics_training_end", 1.0f);
    return 0;
}

int immune_metrics_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_metrics_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    immune_metrics_heartbeat_instance(NULL, "immune_metrics_training_step", progress);
    return 0;
}
