/**
 * @file nimcp_metrics.c
 * @brief NIMCP Metrics Collection and Export Implementation
 * @version 2.6.1
 * @date 2025-11-04
 */

#include "utils/metrics/nimcp_metrics.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metrics)

//=============================================================================
// KG-Driven Wiring Infrastructure
//=============================================================================

/* Forward declaration for handler */
static nimcp_error_t metrics_handle_brain_probe(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/**
 * Handler map for metrics module.
 * Handles brain probe data messages for metrics collection.
 */
DEFINE_HANDLER_MAP_BEGIN(metrics)
    HANDLER_MAP_ENTRY(BIO_MSG_BRAIN_PROBE_DATA, metrics_handle_brain_probe)
DEFINE_HANDLER_MAP_END()

// Note: DEFINE_HANDLER_CALLBACK moved to after struct definition below

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct metric_buffer_entry {
    nimcp_metric_point_t point;
    struct metric_buffer_entry* next;
} metric_buffer_entry_t;

typedef struct nimcp_metrics_collector_internal {
    nimcp_metrics_config_t config;

    // Buffer management
    metric_buffer_entry_t* buffer_head;
    metric_buffer_entry_t* buffer_tail;
    uint32_t buffer_count;

    // File handles
    FILE* stream_file;
    char current_filename[NIMCP_METRICS_MAX_PATH];

    // Statistics
    uint64_t total_metrics_recorded;
    uint64_t total_metrics_flushed;
    uint64_t last_flush_time_ms;

    // Thread safety (future)
    bool is_initialized;
} nimcp_metrics_collector_internal_t;

/**
 * Wiring callback for KG-driven handler registration.
 */
DEFINE_HANDLER_CALLBACK(metrics, nimcp_metrics_collector_internal_t, collector)

//=============================================================================
// Utility Functions
//=============================================================================

static uint64_t get_timestamp_ms(void) {
    return nimcp_time_get_ms();
}

static bool create_directory_recursive(const char* path) {
    char tmp[NIMCP_METRICS_MAX_PATH];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);

    return true;
}

static const char* metric_type_to_string(nimcp_metric_type_t type) {
    switch (type) {
        case NIMCP_METRIC_TYPE_COUNTER: return "counter";
        case NIMCP_METRIC_TYPE_GAUGE: return "gauge";
        case NIMCP_METRIC_TYPE_HISTOGRAM: return "histogram";
        case NIMCP_METRIC_TYPE_TIMER: return "timer";
        case NIMCP_METRIC_TYPE_EVENT: return "event";
        default: return "unknown";
    }
}

static const char* metric_category_to_string(nimcp_metric_category_t category) {
    switch (category) {
        case NIMCP_METRIC_CATEGORY_PERFORMANCE: return "performance";
        case NIMCP_METRIC_CATEGORY_MEMORY: return "memory";
        case NIMCP_METRIC_CATEGORY_NETWORK: return "network";
        case NIMCP_METRIC_CATEGORY_LEARNING: return "learning";
        case NIMCP_METRIC_CATEGORY_INFERENCE: return "inference";
        case NIMCP_METRIC_CATEGORY_SYSTEM: return "system";
        case NIMCP_METRIC_CATEGORY_CUSTOM: return "custom";
        default: return "unknown";
    }
}

//=============================================================================
// Core API Implementation
//=============================================================================

void nimcp_metrics_get_default_config(nimcp_metrics_config_t* config) {
    if (!config) return;

    strncpy(config->output_directory, NIMCP_METRICS_DEFAULT_DIR,
            NIMCP_METRICS_MAX_PATH - 1);
    config->format = NIMCP_METRICS_FORMAT_CSV;
    config->flush_interval_ms = 5000;  // 5 seconds
    config->buffer_size = NIMCP_METRICS_BUFFER_SIZE;
    config->enable_streaming = true;
    config->enable_compression = false;
    config->include_timestamps = true;
    config->include_hostname = true;
}

nimcp_metrics_collector_t nimcp_metrics_create(void) {
    nimcp_metrics_config_t config;
    nimcp_metrics_get_default_config(&config);
    return nimcp_metrics_create_with_config(&config);
}

nimcp_metrics_collector_t nimcp_metrics_create_with_config(
    const nimcp_metrics_config_t* config
) {
    if (!config) {
        NIMCP_LOGGING_ERROR("nimcp_metrics_create_with_config: NULL config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    nimcp_metrics_collector_internal_t* collector =
        nimcp_calloc(1, sizeof(nimcp_metrics_collector_internal_t));

    if (!collector) {
        NIMCP_LOGGING_ERROR("Failed to allocate metrics collector");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collector is NULL");


        return NULL;
    }

    // Copy configuration
    memcpy(&collector->config, config, sizeof(nimcp_metrics_config_t));

    // Create output directory
    if (!create_directory_recursive(collector->config.output_directory)) {
        NIMCP_LOGGING_WARN("Failed to create metrics directory: %s",
                      collector->config.output_directory);
    }

    // Initialize buffer
    collector->buffer_head = NULL;
    collector->buffer_tail = NULL;
    collector->buffer_count = 0;

    // Initialize statistics
    collector->total_metrics_recorded = 0;
    collector->total_metrics_flushed = 0;
    collector->last_flush_time_ms = get_timestamp_ms();

    // Open streaming file if enabled
    if (collector->config.enable_streaming) {
        char timestamp[32];
        uint64_t ts = get_timestamp_ms();
        snprintf(timestamp, sizeof(timestamp), "%lu", ts);

        snprintf(collector->current_filename, NIMCP_METRICS_MAX_PATH,
                 "%.450s/metrics_%s.csv",
                 collector->config.output_directory, timestamp);

        collector->stream_file = fopen(collector->current_filename, "w");
        if (collector->stream_file) {
            // Write CSV header
            fprintf(collector->stream_file,
                    "timestamp_ms,name,type,category,value,labels\n");
            fflush(collector->stream_file);
        } else {
            NIMCP_LOGGING_WARN("Failed to open streaming file: %s",
                          collector->current_filename);
        }
    }

    collector->is_initialized = true;

    NIMCP_LOGGING_INFO("Created metrics collector (dir=%s, format=%d)",
                  collector->config.output_directory,
                  collector->config.format);

    return (nimcp_metrics_collector_t)collector;
}

void nimcp_metrics_destroy(nimcp_metrics_collector_t collector) {
    if (!collector) return;

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    // Flush pending metrics
    nimcp_metrics_flush(collector);

    // Close streaming file
    if (internal->stream_file) {
        fclose(internal->stream_file);
        internal->stream_file = NULL;
    }

    // Free buffer
    metric_buffer_entry_t* current = internal->buffer_head;
    while (current) {
        metric_buffer_entry_t* next = current->next;
        nimcp_free(current);
        current = next;
    }

    NIMCP_LOGGING_INFO("Destroyed metrics collector (recorded=%lu, flushed=%lu)",
                  internal->total_metrics_recorded,
                  internal->total_metrics_flushed);

    nimcp_free(internal);
}

bool nimcp_metrics_set_directory(
    nimcp_metrics_collector_t collector,
    const char* directory
) {
    if (!collector || !directory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_set_directory: required parameter is NULL (collector, directory)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    strncpy(internal->config.output_directory, directory,
            NIMCP_METRICS_MAX_PATH - 1);

    create_directory_recursive(directory);

    NIMCP_LOGGING_DEBUG("Set metrics directory: %s", directory);
    return true;
}

bool nimcp_metrics_set_format(
    nimcp_metrics_collector_t collector,
    nimcp_metrics_format_t format
) {
    if (!collector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_set_format: collector is NULL");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    internal->config.format = format;

    NIMCP_LOGGING_DEBUG("Set metrics format: %d", format);
    return true;
}

//=============================================================================
// Metrics Recording Implementation
//=============================================================================

static bool add_to_buffer(
    nimcp_metrics_collector_internal_t* internal,
    const nimcp_metric_point_t* point
) {
    metric_buffer_entry_t* entry =
        nimcp_calloc(1, sizeof(metric_buffer_entry_t));

    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "add_to_buffer: entry is NULL");
        return false;
    }

    memcpy(&entry->point, point, sizeof(nimcp_metric_point_t));
    entry->next = NULL;

    // Add to tail of linked list
    if (internal->buffer_tail) {
        internal->buffer_tail->next = entry;
    } else {
        internal->buffer_head = entry;
    }
    internal->buffer_tail = entry;
    internal->buffer_count++;

    internal->total_metrics_recorded++;

    // Auto-flush if buffer is full
    if (internal->buffer_count >= internal->config.buffer_size) {
        nimcp_metrics_flush((nimcp_metrics_collector_t)internal);
    }

    // Stream to file if enabled
    if (internal->stream_file && internal->config.enable_streaming) {
        fprintf(internal->stream_file, "%lu,%s,%s,%s,%.6f,%s\n",
                point->timestamp_ms,
                point->name,
                metric_type_to_string(point->type),
                metric_category_to_string(point->category),
                point->value,
                point->labels[0] ? point->labels : "{}");
        fflush(internal->stream_file);
    }

    return true;
}

bool nimcp_metrics_record_counter(
    nimcp_metrics_collector_t collector,
    const char* name,
    uint64_t value,
    nimcp_metric_category_t category
) {
    if (!collector || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_record_counter: required parameter is NULL (collector, name)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    nimcp_metric_point_t point = {0};
    strncpy(point.name, name, NIMCP_METRICS_MAX_NAME - 1);
    point.type = NIMCP_METRIC_TYPE_COUNTER;
    point.category = category;
    point.value = (double)value;
    point.timestamp_ms = get_timestamp_ms();
    point.labels[0] = '\0';

    return add_to_buffer(internal, &point);
}

bool nimcp_metrics_record_gauge(
    nimcp_metrics_collector_t collector,
    const char* name,
    double value,
    nimcp_metric_category_t category
) {
    if (!collector || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_record_gauge: required parameter is NULL (collector, name)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    nimcp_metric_point_t point = {0};
    strncpy(point.name, name, NIMCP_METRICS_MAX_NAME - 1);
    point.type = NIMCP_METRIC_TYPE_GAUGE;
    point.category = category;
    point.value = value;
    point.timestamp_ms = get_timestamp_ms();
    point.labels[0] = '\0';

    return add_to_buffer(internal, &point);
}

bool nimcp_metrics_record_timer(
    nimcp_metrics_collector_t collector,
    const char* name,
    double duration_ms,
    nimcp_metric_category_t category
) {
    if (!collector || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_record_timer: required parameter is NULL (collector, name)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    nimcp_metric_point_t point = {0};
    strncpy(point.name, name, NIMCP_METRICS_MAX_NAME - 1);
    point.type = NIMCP_METRIC_TYPE_TIMER;
    point.category = category;
    point.value = duration_ms;
    point.timestamp_ms = get_timestamp_ms();
    point.labels[0] = '\0';

    return add_to_buffer(internal, &point);
}

bool nimcp_metrics_record_event(
    nimcp_metrics_collector_t collector,
    const char* name,
    const char* labels,
    nimcp_metric_category_t category
) {
    if (!collector || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_record_event: required parameter is NULL (collector, name)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    nimcp_metric_point_t point = {0};
    strncpy(point.name, name, NIMCP_METRICS_MAX_NAME - 1);
    point.type = NIMCP_METRIC_TYPE_EVENT;
    point.category = category;
    point.value = 1.0;
    point.timestamp_ms = get_timestamp_ms();

    if (labels) {
        strncpy(point.labels, labels, sizeof(point.labels) - 1);
    } else {
        point.labels[0] = '\0';
    }

    return add_to_buffer(internal, &point);
}

bool nimcp_metrics_record_point(
    nimcp_metrics_collector_t collector,
    const nimcp_metric_point_t* point
) {
    if (!collector || !point) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_record_point: required parameter is NULL (collector, point)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    return add_to_buffer(internal, point);
}

//=============================================================================
// Hierarchical Brain Metrics Implementation
//=============================================================================

bool nimcp_metrics_record_hierarchical(
    nimcp_metrics_collector_t collector,
    const nimcp_hierarchical_metrics_t* metrics
) {
    if (!collector || !metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_record_hierarchical: required parameter is NULL (collector, metrics)");
        return false;
    }

    // Record system metrics
    nimcp_metrics_record_gauge(collector, "hierarchical.num_regions",
                                metrics->num_regions,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    nimcp_metrics_record_gauge(collector, "hierarchical.num_layers",
                                metrics->num_layers,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    // Record performance metrics
    nimcp_metrics_record_counter(collector, "hierarchical.forward_passes",
                                  metrics->total_forward_passes,
                                  NIMCP_METRIC_CATEGORY_PERFORMANCE);

    nimcp_metrics_record_counter(collector, "hierarchical.learning_updates",
                                  metrics->total_learning_updates,
                                  NIMCP_METRIC_CATEGORY_LEARNING);

    nimcp_metrics_record_gauge(collector, "hierarchical.avg_forward_time_ms",
                                metrics->avg_forward_time_ms,
                                NIMCP_METRIC_CATEGORY_PERFORMANCE);

    // Record memory metrics
    nimcp_metrics_record_gauge(collector, "hierarchical.total_memory_bytes",
                                (double)metrics->total_memory_bytes,
                                NIMCP_METRIC_CATEGORY_MEMORY);

    nimcp_metrics_record_gauge(collector, "hierarchical.active_memory_bytes",
                                (double)metrics->active_memory_bytes,
                                NIMCP_METRIC_CATEGORY_MEMORY);

    // Record learning metrics
    nimcp_metrics_record_gauge(collector, "hierarchical.avg_learning_rate",
                                metrics->avg_learning_rate,
                                NIMCP_METRIC_CATEGORY_LEARNING);

    nimcp_metrics_record_gauge(collector, "hierarchical.avg_error",
                                metrics->avg_error,
                                NIMCP_METRIC_CATEGORY_LEARNING);

    nimcp_metrics_record_gauge(collector, "hierarchical.avg_accuracy",
                                metrics->avg_accuracy,
                                NIMCP_METRIC_CATEGORY_LEARNING);

    // Record neuromodulation metrics
    nimcp_metrics_record_gauge(collector, "hierarchical.dopamine",
                                metrics->dopamine_level,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    nimcp_metrics_record_gauge(collector, "hierarchical.acetylcholine",
                                metrics->acetylcholine_level,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    nimcp_metrics_record_gauge(collector, "hierarchical.serotonin",
                                metrics->serotonin_level,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    // Record region metrics
    nimcp_metrics_record_gauge(collector, "hierarchical.active_regions",
                                metrics->active_regions,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    return true;
}

uint64_t nimcp_metrics_timer_start(
    nimcp_metrics_collector_t collector,
    const char* timer_name
) {
    if (!collector || !timer_name) return 0;
    return get_timestamp_ms();
}

bool nimcp_metrics_timer_stop(
    nimcp_metrics_collector_t collector,
    const char* timer_name,
    uint64_t start_time,
    nimcp_metric_category_t category
) {
    if (!collector || !timer_name || start_time == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_timer_stop: required parameter is NULL (collector, timer_name)");
        return false;
    }

    uint64_t end_time = get_timestamp_ms();
    double duration_ms = (double)(end_time - start_time);

    return nimcp_metrics_record_timer(collector, timer_name, duration_ms, category);
}

//=============================================================================
// Export and Streaming Implementation
//=============================================================================

int32_t nimcp_metrics_flush(nimcp_metrics_collector_t collector) {
    if (!collector) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collector is NULL");

        return -1;

    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    if (internal->buffer_count == 0) {
        return 0;  // Nothing to flush
    }

    uint32_t flushed = internal->buffer_count;

    // Clear buffer
    metric_buffer_entry_t* current = internal->buffer_head;
    while (current) {
        metric_buffer_entry_t* next = current->next;
        nimcp_free(current);
        current = next;
    }

    internal->buffer_head = NULL;
    internal->buffer_tail = NULL;
    internal->buffer_count = 0;
    internal->total_metrics_flushed += flushed;
    internal->last_flush_time_ms = get_timestamp_ms();

    NIMCP_LOGGING_DEBUG("Flushed %u metrics", flushed);

    return (int32_t)flushed;
}

bool nimcp_metrics_export_tableau_csv(
    nimcp_metrics_collector_t collector,
    const char* filename
) {
    if (!collector || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_export_tableau_csv: required parameter is NULL (collector, filename)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    char filepath[NIMCP_METRICS_MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%.450s/%.60s",
             internal->config.output_directory, filename);

    FILE* file = fopen(filepath, "w");
    if (!file) {
        NIMCP_LOGGING_ERROR("Failed to open file for export: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_export_tableau_csv: file is NULL");
        return false;
    }

    // Write Tableau-compatible CSV header
    fprintf(file, "Timestamp,Metric Name,Type,Category,Value,Labels\n");

    // Write all buffered metrics
    metric_buffer_entry_t* current = internal->buffer_head;
    while (current) {
        const nimcp_metric_point_t* point = &current->point;
        fprintf(file, "%lu,%s,%s,%s,%.6f,%s\n",
                point->timestamp_ms,
                point->name,
                metric_type_to_string(point->type),
                metric_category_to_string(point->category),
                point->value,
                point->labels[0] ? point->labels : "");
        current = current->next;
    }

    fclose(file);

    NIMCP_LOGGING_INFO("Exported metrics to Tableau CSV: %s", filepath);
    return true;
}

bool nimcp_metrics_export_powerbi_json(
    nimcp_metrics_collector_t collector,
    const char* filename
) {
    if (!collector || !filename) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_export_powerbi_json: required parameter is NULL (collector, filename)");
        return false;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    char filepath[NIMCP_METRICS_MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%.450s/%.60s",
             internal->config.output_directory, filename);

    FILE* file = fopen(filepath, "w");
    if (!file) {
        NIMCP_LOGGING_ERROR("Failed to open file for export: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_export_powerbi_json: file is NULL");
        return false;
    }

    // Write PowerBI-compatible JSON array
    fprintf(file, "[\n");

    metric_buffer_entry_t* current = internal->buffer_head;
    bool first = true;

    while (current) {
        const nimcp_metric_point_t* point = &current->point;

        if (!first) {
            fprintf(file, ",\n");
        }
        first = false;

        fprintf(file, "  {\n");
        fprintf(file, "    \"timestamp\": %lu,\n", point->timestamp_ms);
        fprintf(file, "    \"name\": \"%s\",\n", point->name);
        fprintf(file, "    \"type\": \"%s\",\n",
                metric_type_to_string(point->type));
        fprintf(file, "    \"category\": \"%s\",\n",
                metric_category_to_string(point->category));
        fprintf(file, "    \"value\": %.6f", point->value);

        if (point->labels[0]) {
            fprintf(file, ",\n    \"labels\": %s\n", point->labels);
        } else {
            fprintf(file, "\n");
        }

        fprintf(file, "  }");

        current = current->next;
    }

    fprintf(file, "\n]\n");
    fclose(file);

    NIMCP_LOGGING_INFO("Exported metrics to PowerBI JSON: %s", filepath);
    return true;
}

int32_t nimcp_metrics_get_stats(
    nimcp_metrics_collector_t collector,
    char* stats_json,
    uint32_t max_size
) {
    if (!collector || !stats_json || max_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_get_stats: required parameter is NULL (collector, stats_json)");
        return -1;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    int written = snprintf(stats_json, max_size,
        "{\n"
        "  \"total_recorded\": %lu,\n"
        "  \"total_flushed\": %lu,\n"
        "  \"buffer_count\": %u,\n"
        "  \"buffer_size\": %u,\n"
        "  \"last_flush_time_ms\": %lu,\n"
        "  \"output_directory\": \"%s\",\n"
        "  \"format\": %d,\n"
        "  \"streaming_enabled\": %s\n"
        "}",
        internal->total_metrics_recorded,
        internal->total_metrics_flushed,
        internal->buffer_count,
        internal->config.buffer_size,
        internal->last_flush_time_ms,
        internal->config.output_directory,
        internal->config.format,
        internal->config.enable_streaming ? "true" : "false"
    );

    return (written > 0) ? written : -1;
}

//=============================================================================
// Query Implementation (Basic)
//=============================================================================

int32_t nimcp_metrics_query_by_category(
    nimcp_metrics_collector_t collector,
    nimcp_metric_category_t category,
    nimcp_metric_point_t* results,
    uint32_t max_results
) {
    if (!collector || !results || max_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_query_by_category: required parameter is NULL (collector, results)");
        return -1;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    uint32_t count = 0;
    metric_buffer_entry_t* current = internal->buffer_head;

    while (current && count < max_results) {
        if (current->point.category == category) {
            memcpy(&results[count], &current->point, sizeof(nimcp_metric_point_t));
            count++;
        }
        current = current->next;
    }

    return (int32_t)count;
}

int32_t nimcp_metrics_query_by_time(
    nimcp_metrics_collector_t collector,
    uint64_t start_time_ms,
    uint64_t end_time_ms,
    nimcp_metric_point_t* results,
    uint32_t max_results
) {
    if (!collector || !results || max_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_query_by_time: required parameter is NULL (collector, results)");
        return -1;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    uint32_t count = 0;
    metric_buffer_entry_t* current = internal->buffer_head;

    while (current && count < max_results) {
        if (current->point.timestamp_ms >= start_time_ms &&
            current->point.timestamp_ms <= end_time_ms) {
            memcpy(&results[count], &current->point, sizeof(nimcp_metric_point_t));
            count++;
        }
        current = current->next;
    }

    return (int32_t)count;
}

int32_t nimcp_metrics_query_by_name(
    nimcp_metrics_collector_t collector,
    const char* pattern,
    nimcp_metric_point_t* results,
    uint32_t max_results
) {
    if (!collector || !pattern || !results || max_results == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_query_by_name: required parameter is NULL (collector, pattern, results)");
        return -1;
    }

    nimcp_metrics_collector_internal_t* internal =
        (nimcp_metrics_collector_internal_t*)collector;

    uint32_t count = 0;
    metric_buffer_entry_t* current = internal->buffer_head;

    while (current && count < max_results) {
        if (strstr(current->point.name, pattern) != NULL) {
            memcpy(&results[count], &current->point, sizeof(nimcp_metric_point_t));
            count++;
        }
        current = current->next;
    }

    return (int32_t)count;
}

//=============================================================================
// Bio-Async Brain Probe Handler (Loose Coupling)
//=============================================================================

#define METRICS_LOG_MODULE "METRICS"

// Global collector for bio-async message handling
static nimcp_metrics_collector_t g_metrics_collector = NULL;

// Module context for bio-router registration
static bio_module_context_t g_metrics_module_ctx = NULL;

/**
 * @brief Handle brain probe data messages received via bio-async
 *
 * WHAT: Processes BIO_MSG_BRAIN_PROBE_DATA messages from brain modules
 * WHY:  Enables loose coupling - brain broadcasts, metrics receives independently
 * HOW:  Extracts probe data and records as metrics
 *
 * Signature matches bio_message_handler_t
 */
static nimcp_error_t metrics_handle_brain_probe(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)response_promise;  // Broadcasts don't need responses

    NIMCP_CHECK_THROW(msg && msg_size >= sizeof(bio_msg_brain_probe_data_t),
                      NIMCP_ERROR_INVALID, "invalid message or size");

    const bio_msg_brain_probe_data_t* probe_msg = (const bio_msg_brain_probe_data_t*)msg;

    // Verify message type
    NIMCP_CHECK_THROW(probe_msg->header.type == BIO_MSG_BRAIN_PROBE_DATA,
                      NIMCP_ERROR_INVALID, "wrong message type");

    // Get collector from user_data or global
    nimcp_metrics_collector_t collector = (nimcp_metrics_collector_t)user_data;
    if (!collector) {
        collector = g_metrics_collector;
    }
    if (!collector) {
        // No collector registered yet - this is fine, just skip
        return NIMCP_SUCCESS;
    }

    // Create metric prefix with brain ID for multi-brain support
    char prefix[NIMCP_LABEL_BUFFER_SIZE];
    snprintf(prefix, sizeof(prefix), "brain.%llx", (unsigned long long)probe_msg->brain_id);

    // Record brain metrics
    char metric_name[NIMCP_NAME_BUFFER_SIZE];

    // Architecture metrics
    snprintf(metric_name, sizeof(metric_name), "%s.num_neurons", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->num_neurons,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    snprintf(metric_name, sizeof(metric_name), "%s.num_synapses", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->num_synapses,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    snprintf(metric_name, sizeof(metric_name), "%s.num_active_synapses", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->num_active_synapses,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    // Performance metrics
    snprintf(metric_name, sizeof(metric_name), "%s.total_inferences", prefix);
    nimcp_metrics_record_counter(collector, metric_name, probe_msg->total_inferences,
                                  NIMCP_METRIC_CATEGORY_PERFORMANCE);

    snprintf(metric_name, sizeof(metric_name), "%s.total_learning_steps", prefix);
    nimcp_metrics_record_counter(collector, metric_name, probe_msg->total_learning_steps,
                                  NIMCP_METRIC_CATEGORY_LEARNING);

    snprintf(metric_name, sizeof(metric_name), "%s.avg_inference_time_us", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->avg_inference_time_us,
                                NIMCP_METRIC_CATEGORY_PERFORMANCE);

    // Learning metrics
    snprintf(metric_name, sizeof(metric_name), "%s.learning_rate", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->current_learning_rate,
                                NIMCP_METRIC_CATEGORY_LEARNING);

    snprintf(metric_name, sizeof(metric_name), "%s.accuracy", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->accuracy,
                                NIMCP_METRIC_CATEGORY_LEARNING);

    snprintf(metric_name, sizeof(metric_name), "%s.sparsity", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, probe_msg->avg_sparsity,
                                NIMCP_METRIC_CATEGORY_SYSTEM);

    // Memory metrics
    snprintf(metric_name, sizeof(metric_name), "%s.memory_bytes", prefix);
    nimcp_metrics_record_gauge(collector, metric_name, (double)probe_msg->memory_bytes,
                                NIMCP_METRIC_CATEGORY_MEMORY);

    // COW metrics
    if (probe_msg->is_cow_clone) {
        snprintf(metric_name, sizeof(metric_name), "%s.cow_shared_bytes", prefix);
        nimcp_metrics_record_gauge(collector, metric_name, (double)probe_msg->cow_shared_bytes,
                                    NIMCP_METRIC_CATEGORY_MEMORY);

        snprintf(metric_name, sizeof(metric_name), "%s.cow_private_bytes", prefix);
        nimcp_metrics_record_gauge(collector, metric_name, (double)probe_msg->cow_private_bytes,
                                    NIMCP_METRIC_CATEGORY_MEMORY);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Register metrics module to receive brain probe broadcasts via bio-async
 */
bool nimcp_metrics_register_bio_async(nimcp_metrics_collector_t collector) {
    if (!collector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_register_bio_async: collector is NULL");
        return false;
    }

    // Store collector for handler
    g_metrics_collector = collector;

    // Check if router is available
    if (!bio_router_is_initialized()) {
        // Router not initialized yet - this is OK, registration can be deferred
        return true;
    }

    // Register module if not already registered
    if (!g_metrics_module_ctx) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_METRICS,
            .module_name = "metrics",
            .inbox_capacity = 256,
            .user_data = collector
        };
        g_metrics_module_ctx = bio_router_register_module(&info);
        if (!g_metrics_module_ctx) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_metrics_register_bio_async: g_metrics_module_ctx is NULL");
            return false;
        }
    }

    // Try KG-driven wiring callback registration first
    nimcp_error_t wiring_result = bio_router_register_wiring_callback(
        BIO_MODULE_METRICS,
        (void*)metrics_handler_callback,
        collector
    );

    if (wiring_result == NIMCP_SUCCESS) {
        // KG-driven wiring registered successfully
    } else {
        // Legacy fallback - register handler directly
        nimcp_error_t err = LEGACY_HANDLER_REGISTRATION(bio_router_register_handler(
            g_metrics_module_ctx,
            BIO_MSG_BRAIN_PROBE_DATA,
            metrics_handle_brain_probe
        ));

        if (err != NIMCP_SUCCESS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_metrics_register_bio_async: validation failed");
            return false;
        }
    }

    return true;
}

/**
 * @brief Process pending bio-async messages for metrics module
 */
void nimcp_metrics_process_bio_async(void) {
    if (!g_metrics_module_ctx) {
        return;
    }

    // Process pending messages in inbox
    // Handlers registered via bio_router_register_handler will be invoked
    bio_router_process_inbox(g_metrics_module_ctx, 100);
}
