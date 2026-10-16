/**
 * @file nimcp_protocol_metrics.c
 * @brief Protocol metrics collection and dashboard implementation
 *
 * WHAT: Implements metrics collection, aggregation, and export
 * WHY:  Monitor protocol health and performance
 * HOW:  Hash table storage, time-series tracking, multiple export formats
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "async/nimcp_protocol_metrics.h"
#include "api/nimcp_api_exception.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_common.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <inttypes.h>  /* P3-60 fix: PRIu64 for portable uint64_t formatting */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protocol_metrics)

#define MODULE_NAME "protocol_metrics"

//=============================================================================
// Internal Constants
//=============================================================================

#define DEFAULT_MAX_METRICS 1024
#define DEFAULT_RETENTION_MS (3600 * 1000)  // 1 hour
#define DEFAULT_COLLECTION_INTERVAL_MS 1000  // 1 second
#define METRIC_HASH_SIZE 256

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Metric entry in hash table
 */
typedef struct metric_entry {
    metric_t metric;
    histogram_metric_t* histogram;     // For histogram types
    struct metric_entry* next;         // Hash collision chain
} metric_entry_t;

/**
 * @brief Protocol metrics implementation
 */
struct protocol_metrics_struct {
    dashboard_config_t config;
    metric_entry_t* hash_table[METRIC_HASH_SIZE];
    uint32_t metric_count;
    uint64_t creation_time_ms;
    uint64_t last_cleanup_ms;
    metrics_summary_t cached_summary;
    bool summary_valid;
    // P3 note: Using platform mutex (nimcp_platform_mutex_t) instead of nimcp_mutex_t.
    // This is acceptable because protocol_metrics operates at the middleware layer
    // and doesn't need recursive locking or advanced mutex attributes.
    nimcp_platform_mutex_t mutex;  /**< Thread safety for all public operations */
};

//=============================================================================
// Hash Function
//=============================================================================

/**
 * @brief Simple string hash function
 */
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % METRIC_HASH_SIZE;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Create metric key from name and labels
 */
static void make_metric_key(char* key, size_t key_size, const char* name,
                           const metric_label_t* labels, uint32_t label_count) {
    snprintf(key, key_size, "%s", name);

    for (uint32_t i = 0; i < label_count && i < MAX_METRIC_LABELS; i++) {
        size_t len = strlen(key);
        snprintf(key + len, key_size - len, "_%s=%s",
                labels[i].key, labels[i].value);
    }
}

/**
 * @brief Find metric entry by key
 */
static metric_entry_t* find_metric(protocol_metrics_t metrics, const char* key) {
    if (!metrics || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_metric: required parameter is NULL (metrics, key)");
        return NULL;
    }

    uint32_t hash = hash_string(key);
    metric_entry_t* entry = metrics->hash_table[hash];

    while (entry) {
        if (strcmp(entry->metric.name, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    // P2 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE.
    // Not finding a metric is normal (first recording of this metric name).
    return NULL;
}

/**
 * @brief Create new metric entry
 */
static metric_entry_t* create_metric_entry(const char* name, metric_type_t type,
                                          const metric_label_t* labels,
                                          uint32_t label_count) {
    metric_entry_t* entry = (metric_entry_t*)nimcp_calloc(1, sizeof(metric_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "create_metric_entry: failed to allocate metric entry");
        return NULL;
    }

    strncpy(entry->metric.name, name, sizeof(entry->metric.name) - 1);
    entry->metric.type = type;
    entry->metric.value = 0.0;
    entry->metric.timestamp_ms = nimcp_time_get_ms();
    entry->metric.sample_count = 0;
    entry->metric.label_count = label_count;

    for (uint32_t i = 0; i < label_count && i < MAX_METRIC_LABELS; i++) {
        entry->metric.labels[i] = labels[i];
    }

    // Create histogram if needed
    if (type == METRIC_TYPE_HISTOGRAM) {
        entry->histogram = (histogram_metric_t*)nimcp_calloc(1, sizeof(histogram_metric_t));
        if (entry->histogram) {
            strncpy(entry->histogram->name, name, sizeof(entry->histogram->name) - 1);
            entry->histogram->label_count = label_count;
            for (uint32_t i = 0; i < label_count && i < MAX_METRIC_LABELS; i++) {
                entry->histogram->labels[i] = labels[i];
            }

            // Default buckets (exponential)
            double bounds[] = {0.001, 0.01, 0.1, 1.0, 10.0, 100.0, 1000.0};
            entry->histogram->bucket_count = 7;
            for (uint32_t i = 0; i < 7; i++) {
                entry->histogram->buckets[i].upper_bound = bounds[i];
                entry->histogram->buckets[i].count = 0;
            }
        }
    }

    return entry;
}

//=============================================================================
// Lifecycle Implementation
//=============================================================================

dashboard_config_t protocol_metrics_default_config(void) {
    dashboard_config_t config;
    config.collection_interval_ms = DEFAULT_COLLECTION_INTERVAL_MS;
    config.retention_ms = DEFAULT_RETENTION_MS;
    config.default_export_format = EXPORT_FORMAT_JSON;
    config.max_metrics = DEFAULT_MAX_METRICS;
    config.enable_aggregation = true;
    config.enable_histograms = true;
    return config;
}

protocol_metrics_t protocol_metrics_create(const dashboard_config_t* config) {
    LOG_INFO("Creating protocol metrics dashboard");

    protocol_metrics_t metrics = (protocol_metrics_t)nimcp_calloc(1,
                                    sizeof(struct protocol_metrics_struct));
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "protocol_metrics_create: failed to allocate metrics structure");
        LOG_ERROR("Failed to allocate metrics structure");
        return NULL;
    }

    if (config) {
        metrics->config = *config;
    } else {
        metrics->config = protocol_metrics_default_config();
    }

    /* Initialize mutex for thread safety */
    if (nimcp_platform_mutex_init(&metrics->mutex, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "protocol_metrics_create: failed to initialize mutex");
        LOG_ERROR("Failed to initialize metrics mutex");
        nimcp_free(metrics);
        return NULL;
    }

    metrics->creation_time_ms = nimcp_time_get_ms();
    metrics->last_cleanup_ms = metrics->creation_time_ms;
    metrics->metric_count = 0;
    metrics->summary_valid = false;

    memset(metrics->hash_table, 0, sizeof(metrics->hash_table));

    LOG_INFO("Metrics dashboard created: max=%u, retention=%ums",
             metrics->config.max_metrics, metrics->config.retention_ms);

    return metrics;
}

void protocol_metrics_destroy(protocol_metrics_t metrics) {
    if (!metrics) return;

    LOG_INFO("Destroying metrics dashboard");

    nimcp_platform_mutex_lock(&metrics->mutex);

    // Free all entries
    for (uint32_t i = 0; i < METRIC_HASH_SIZE; i++) {
        metric_entry_t* entry = metrics->hash_table[i];
        while (entry) {
            metric_entry_t* next = entry->next;
            if (entry->histogram) {
                nimcp_free(entry->histogram);
            }
            nimcp_free(entry);
            entry = next;
        }
    }

    nimcp_platform_mutex_unlock(&metrics->mutex);
    nimcp_platform_mutex_destroy(&metrics->mutex);

    nimcp_free(metrics);
    LOG_DEBUG("Metrics dashboard destroyed");
}

//=============================================================================
// Recording Implementation
//=============================================================================

bool protocol_metrics_record(protocol_metrics_t metrics, const char* name,
                            metric_type_t type, double value,
                            const metric_label_t* labels, uint32_t label_count) {
    if (!metrics || !name) {
        LOG_ERROR("Invalid parameters for metric recording");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_record: required parameter is NULL (metrics, name)");
        return false;
    }

    if (label_count > MAX_METRIC_LABELS) {
        LOG_WARN("Too many labels (%u), truncating to %d", label_count, MAX_METRIC_LABELS);
        label_count = MAX_METRIC_LABELS;
    }

    char key[NIMCP_NAME_BUFFER_SIZE];
    make_metric_key(key, sizeof(key), name, labels, label_count);

    nimcp_platform_mutex_lock(&metrics->mutex);

    metric_entry_t* entry = find_metric(metrics, key);
    if (!entry) {
        if (metrics->metric_count >= metrics->config.max_metrics) {
            LOG_ERROR("Maximum metrics reached (%u)", metrics->config.max_metrics);
            nimcp_platform_mutex_unlock(&metrics->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "protocol_metrics_record: capacity exceeded");
            return false;
        }

        entry = create_metric_entry(key, type, labels, label_count);
        if (!entry) {
            LOG_ERROR("Failed to create metric entry");
            nimcp_platform_mutex_unlock(&metrics->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_record: entry is NULL");
            return false;
        }

        uint32_t hash = hash_string(key);
        entry->next = metrics->hash_table[hash];
        metrics->hash_table[hash] = entry;
        metrics->metric_count++;
        metrics->summary_valid = false;
    }

    entry->metric.value = value;
    entry->metric.timestamp_ms = nimcp_time_get_ms();
    /* Saturating increment to prevent overflow */
    if (entry->metric.sample_count < UINT64_MAX) {
        entry->metric.sample_count++;
    }

    nimcp_platform_mutex_unlock(&metrics->mutex);

    LOG_DEBUG("Recorded metric: %s = %.3f", key, value);

    return true;
}

bool protocol_metrics_increment(protocol_metrics_t metrics, const char* name,
                               double delta, const metric_label_t* labels,
                               uint32_t label_count) {
    if (!metrics || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_increment: required parameter is NULL (metrics, name)");
        return false;
    }

    char key[NIMCP_NAME_BUFFER_SIZE];
    make_metric_key(key, sizeof(key), name, labels, label_count);

    nimcp_platform_mutex_lock(&metrics->mutex);

    metric_entry_t* entry = find_metric(metrics, key);
    if (!entry) {
        nimcp_platform_mutex_unlock(&metrics->mutex);
        return protocol_metrics_record(metrics, key, METRIC_TYPE_COUNTER,
                                      delta, labels, label_count);
    }

    /* Saturating addition to prevent overflow */
    if (entry->metric.value <= DBL_MAX - fabs(delta)) {
        entry->metric.value += delta;
    }
    entry->metric.timestamp_ms = nimcp_time_get_ms();
    /* Saturating increment to prevent overflow */
    if (entry->metric.sample_count < UINT64_MAX) {
        entry->metric.sample_count++;
    }

    /* P2-59 fix: Capture values under lock for LOG statement, since entry->metric
     * may be modified by another thread after we release the mutex. */
    double log_value = entry->metric.value;

    nimcp_platform_mutex_unlock(&metrics->mutex);

    LOG_DEBUG("Incremented counter: %s += %.3f (total=%.3f)",
             key, delta, log_value);

    return true;
}

bool protocol_metrics_set_gauge(protocol_metrics_t metrics, const char* name,
                               double value, const metric_label_t* labels,
                               uint32_t label_count) {
    return protocol_metrics_record(metrics, name, METRIC_TYPE_GAUGE,
                                  value, labels, label_count);
}

bool protocol_metrics_observe(protocol_metrics_t metrics, const char* name,
                             double value, const metric_label_t* labels,
                             uint32_t label_count) {
    if (!metrics || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_observe: required parameter is NULL (metrics, name)");
        return false;
    }

    char key[NIMCP_NAME_BUFFER_SIZE];
    make_metric_key(key, sizeof(key), name, labels, label_count);

    nimcp_platform_mutex_lock(&metrics->mutex);

    metric_entry_t* entry = find_metric(metrics, key);
    if (!entry) {
        /* Check metric limit before creating new entry */
        if (metrics->metric_count >= metrics->config.max_metrics) {
            LOG_ERROR("Maximum metrics reached (%u)", metrics->config.max_metrics);
            nimcp_platform_mutex_unlock(&metrics->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "protocol_metrics_observe: capacity exceeded");
            return false;
        }

        entry = create_metric_entry(key, METRIC_TYPE_HISTOGRAM, labels, label_count);
        if (!entry) {
            nimcp_platform_mutex_unlock(&metrics->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_observe: entry is NULL");
            return false;
        }

        uint32_t hash = hash_string(key);
        entry->next = metrics->hash_table[hash];
        metrics->hash_table[hash] = entry;
        metrics->metric_count++;
    }

    if (entry->histogram) {
        /* Saturating additions to prevent overflow */
        if (entry->histogram->sum <= DBL_MAX - fabs(value)) {
            entry->histogram->sum += value;
        }
        if (entry->histogram->count < UINT64_MAX) {
            entry->histogram->count++;
        }

        for (uint32_t i = 0; i < entry->histogram->bucket_count; i++) {
            if (value <= entry->histogram->buckets[i].upper_bound) {
                if (entry->histogram->buckets[i].count < UINT64_MAX) {
                    entry->histogram->buckets[i].count++;
                }
            }
        }
    }

    entry->metric.timestamp_ms = nimcp_time_get_ms();
    /* Saturating increment to prevent overflow */
    if (entry->metric.sample_count < UINT64_MAX) {
        entry->metric.sample_count++;
    }

    nimcp_platform_mutex_unlock(&metrics->mutex);

    return true;
}

//=============================================================================
// Query Implementation
//=============================================================================

bool protocol_metrics_get(protocol_metrics_t metrics, const char* name,
                         metric_t* out_metric) {
    if (!metrics || !name || !out_metric) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_get: required parameter is NULL (metrics, name, out_metric)");
        return false;
    }

    nimcp_platform_mutex_lock(&metrics->mutex);

    metric_entry_t* entry = find_metric(metrics, name);
    if (!entry) {
        nimcp_platform_mutex_unlock(&metrics->mutex);
        // P2 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE.
        // Querying a non-existent metric is a normal "not found" case.
        return false;
    }

    *out_metric = entry->metric;

    nimcp_platform_mutex_unlock(&metrics->mutex);
    return true;
}

bool protocol_metrics_get_summary(protocol_metrics_t metrics,
                                 metrics_summary_t* out_summary) {
    if (!metrics || !out_summary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_get_summary: required parameter is NULL (metrics, out_summary)");
        return false;
    }

    nimcp_platform_mutex_lock(&metrics->mutex);

    if (metrics->summary_valid) {
        *out_summary = metrics->cached_summary;
        nimcp_platform_mutex_unlock(&metrics->mutex);
        return true;
    }

    memset(out_summary, 0, sizeof(metrics_summary_t));

    uint64_t now = nimcp_time_get_ms();
    out_summary->oldest_timestamp_ms = now;
    out_summary->newest_timestamp_ms = 0;

    for (uint32_t i = 0; i < METRIC_HASH_SIZE; i++) {
        metric_entry_t* entry = metrics->hash_table[i];
        while (entry) {
            out_summary->total_metrics++;
            out_summary->total_samples += entry->metric.sample_count;

            switch (entry->metric.type) {
                case METRIC_TYPE_COUNTER:
                    out_summary->counters++;
                    break;
                case METRIC_TYPE_GAUGE:
                    out_summary->gauges++;
                    break;
                case METRIC_TYPE_HISTOGRAM:
                    out_summary->histograms++;
                    break;
                case METRIC_TYPE_SUMMARY:
                    out_summary->summaries++;
                    break;
            }

            if (entry->metric.timestamp_ms < out_summary->oldest_timestamp_ms) {
                out_summary->oldest_timestamp_ms = entry->metric.timestamp_ms;
            }
            if (entry->metric.timestamp_ms > out_summary->newest_timestamp_ms) {
                out_summary->newest_timestamp_ms = entry->metric.timestamp_ms;
            }

            entry = entry->next;
        }
    }

    out_summary->memory_usage_bytes =
        out_summary->total_metrics * sizeof(metric_entry_t) +
        out_summary->histograms * sizeof(histogram_metric_t);

    metrics->cached_summary = *out_summary;
    metrics->summary_valid = true;

    nimcp_platform_mutex_unlock(&metrics->mutex);

    return true;
}

bool protocol_metrics_query(protocol_metrics_t metrics, const char* pattern,
                           metric_t* out_metrics, uint32_t max_metrics,
                           uint32_t* out_count) {
    if (!metrics || !out_metrics || !out_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_query: required parameter is NULL (metrics, out_metrics, out_count)");
        return false;
    }

    *out_count = 0;

    nimcp_platform_mutex_lock(&metrics->mutex);

    for (uint32_t i = 0; i < METRIC_HASH_SIZE && *out_count < max_metrics; i++) {
        metric_entry_t* entry = metrics->hash_table[i];
        while (entry && *out_count < max_metrics) {
            // Simple pattern matching (NULL = match all)
            if (!pattern || strstr(entry->metric.name, pattern)) {
                out_metrics[*out_count] = entry->metric;
                (*out_count)++;
            }
            entry = entry->next;
        }
    }

    nimcp_platform_mutex_unlock(&metrics->mutex);

    return true;
}

//=============================================================================
// Export Implementation
//=============================================================================

/**
 * @brief Helper to safely add snprintf result to offset
 * @note Caps offset at size to handle truncation correctly
 */
static inline size_t safe_snprintf_advance(size_t offset, size_t size, int written) {
    if (written < 0) {
        return offset;  /* Error, don't advance */
    }
    size_t new_offset = offset + (size_t)written;
    return (new_offset > size) ? size : new_offset;
}

static size_t export_json(protocol_metrics_t metrics, char* buffer, size_t size) {
    size_t offset = 0;
    int written;

    written = snprintf(buffer + offset, size - offset, "{\n  \"metrics\": [\n");
    offset = safe_snprintf_advance(offset, size, written);

    bool first = true;
    for (uint32_t i = 0; i < METRIC_HASH_SIZE && offset < size; i++) {
        metric_entry_t* entry = metrics->hash_table[i];
        while (entry && offset < size - 256) {
            if (!first) {
                written = snprintf(buffer + offset, size - offset, ",\n");
                offset = safe_snprintf_advance(offset, size, written);
            }
            first = false;

            /* P3-60 fix: Use PRIu64 instead of %llu for portable uint64_t */
            written = snprintf(buffer + offset, size - offset,
                "    {\n"
                "      \"name\": \"%s\",\n"
                "      \"type\": \"%s\",\n"
                "      \"value\": %.6f,\n"
                "      \"timestamp\": %" PRIu64 ",\n"
                "      \"samples\": %" PRIu64 "\n"
                "    }",
                entry->metric.name,
                entry->metric.type == METRIC_TYPE_COUNTER ? "counter" :
                entry->metric.type == METRIC_TYPE_GAUGE ? "gauge" :
                entry->metric.type == METRIC_TYPE_HISTOGRAM ? "histogram" : "summary",
                entry->metric.value,
                entry->metric.timestamp_ms,
                entry->metric.sample_count
            );
            offset = safe_snprintf_advance(offset, size, written);

            entry = entry->next;
        }
    }

    if (offset < size) {
        written = snprintf(buffer + offset, size - offset, "\n  ]\n}\n");
        offset = safe_snprintf_advance(offset, size, written);
    }

    return offset;
}

static size_t export_prometheus(protocol_metrics_t metrics, char* buffer, size_t size) {
    size_t offset = 0;
    int written;

    for (uint32_t i = 0; i < METRIC_HASH_SIZE && offset < size; i++) {
        metric_entry_t* entry = metrics->hash_table[i];
        while (entry && offset < size - 256) {
            const char* type_str =
                entry->metric.type == METRIC_TYPE_COUNTER ? "counter" :
                entry->metric.type == METRIC_TYPE_GAUGE ? "gauge" : "histogram";

            /* P3-60 fix: Use PRIu64 instead of %llu */
            written = snprintf(buffer + offset, size - offset,
                "# TYPE %s %s\n%s %.6f %" PRIu64 "\n",
                entry->metric.name, type_str,
                entry->metric.name, entry->metric.value,
                entry->metric.timestamp_ms
            );
            offset = safe_snprintf_advance(offset, size, written);

            entry = entry->next;
        }
    }

    return offset;
}

bool protocol_metrics_export(protocol_metrics_t metrics, export_format_t format,
                            char* out_buffer, size_t buffer_size, size_t* out_bytes) {
    if (!metrics || !out_buffer || !out_bytes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_export: required parameter is NULL (metrics, out_buffer, out_bytes)");
        return false;
    }

    nimcp_platform_mutex_lock(&metrics->mutex);

    size_t bytes = 0;

    switch (format) {
        case EXPORT_FORMAT_JSON:
            bytes = export_json(metrics, out_buffer, buffer_size);
            break;
        case EXPORT_FORMAT_PROMETHEUS:
            bytes = export_prometheus(metrics, out_buffer, buffer_size);
            break;
        case EXPORT_FORMAT_CSV:
            // Simple CSV export
            bytes = snprintf(out_buffer, buffer_size,
                           "name,type,value,timestamp,samples\n");
            break;
    }

    nimcp_platform_mutex_unlock(&metrics->mutex);

    *out_bytes = bytes;
    return bytes < buffer_size;
}

bool protocol_metrics_export_file(protocol_metrics_t metrics,
                                 export_format_t format, const char* filename) {
    if (!metrics || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_export_file: required parameter is NULL (metrics, filename)");
        return false;
    }

    char buffer[65536];
    size_t bytes = 0;

    if (!protocol_metrics_export(metrics, format, buffer, sizeof(buffer), &bytes)) {
        LOG_ERROR("Export buffer too small");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_metrics_export_file: protocol_metrics_export is NULL");
        return false;
    }

    FILE* f = fopen(filename, "w");
    if (!f) {
        LOG_ERROR("Failed to open file: %s", filename);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_export_file: f is NULL");
        return false;
    }

    size_t written = fwrite(buffer, 1, bytes, f);
    fclose(f);

    if (written != bytes) {
        LOG_ERROR("Failed to write all bytes to file: %s (wrote %zu of %zu)",
                  filename, written, bytes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_metrics_export_file: validation failed");
        return false;
    }

    LOG_INFO("Exported metrics to %s (%zu bytes)", filename, bytes);
    return true;
}

//=============================================================================
// Utility Implementation
//=============================================================================

bool protocol_metrics_reset(protocol_metrics_t metrics) {
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "protocol_metrics_reset: metrics is NULL");
        return false;
    }

    nimcp_platform_mutex_lock(&metrics->mutex);

    for (uint32_t i = 0; i < METRIC_HASH_SIZE; i++) {
        metric_entry_t* entry = metrics->hash_table[i];
        while (entry) {
            metric_entry_t* next = entry->next;
            if (entry->histogram) {
                nimcp_free(entry->histogram);
            }
            nimcp_free(entry);
            entry = next;
        }
        metrics->hash_table[i] = NULL;
    }

    metrics->metric_count = 0;
    metrics->summary_valid = false;

    nimcp_platform_mutex_unlock(&metrics->mutex);

    LOG_INFO("All metrics reset");
    return true;
}

uint32_t protocol_metrics_cleanup(protocol_metrics_t metrics) {
    if (!metrics) return 0;

    nimcp_platform_mutex_lock(&metrics->mutex);

    uint64_t now = nimcp_time_get_ms();
    uint64_t cutoff = now - metrics->config.retention_ms;
    uint32_t removed = 0;

    for (uint32_t i = 0; i < METRIC_HASH_SIZE; i++) {
        metric_entry_t** prev_next = &metrics->hash_table[i];
        metric_entry_t* entry = metrics->hash_table[i];

        while (entry) {
            if (entry->metric.timestamp_ms < cutoff) {
                *prev_next = entry->next;
                if (entry->histogram) {
                    nimcp_free(entry->histogram);
                }
                nimcp_free(entry);
                removed++;
                metrics->metric_count--;
                entry = *prev_next;
            } else {
                prev_next = &entry->next;
                entry = entry->next;
            }
        }
    }

    if (removed > 0) {
        LOG_INFO("Cleaned up %u expired metrics", removed);
        metrics->summary_valid = false;
    }

    metrics->last_cleanup_ms = now;

    nimcp_platform_mutex_unlock(&metrics->mutex);

    return removed;
}

uint32_t protocol_metrics_make_labels(metric_label_t* labels,
                                     uint32_t max_labels, ...) {
    if (!labels) return 0;

    va_list args;
    va_start(args, max_labels);

    uint32_t count = 0;
    while (count < max_labels) {
        const char* key = va_arg(args, const char*);
        if (!key) break;

        const char* value = va_arg(args, const char*);
        if (!value) break;

        strncpy(labels[count].key, key, sizeof(labels[count].key) - 1);
        strncpy(labels[count].value, value, sizeof(labels[count].value) - 1);
        count++;
    }

    va_end(args);
    return count;
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from the knowledge graph
 *
 * WHAT: Retrieves structural self-knowledge about the Protocol_Metrics module
 * WHY:  Enables runtime introspection and self-awareness capabilities
 * HOW:  Queries KG for Protocol_Metrics entity and logs observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge was found, 0 otherwise
 */
int protocol_metrics_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Protocol_Metrics");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Protocol_Metrics self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Protocol_Metrics");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Protocol_Metrics");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
