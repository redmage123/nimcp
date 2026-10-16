/**
 * @file nimcp_protocol_metrics.c
 * @brief Implementation of protocol metrics dashboard and semantic analytics
 *
 * This module provides real-time monitoring and analytics for NLP protocol
 * usage, performance, and semantic primitive effectiveness.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_protocol_metrics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protocol_metrics)

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define METRICS_MODULE "ProtocolMetrics"
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Circular buffer for time-series history
 */
typedef struct {
    protocol_stats_t* buffer;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
} stats_history_t;

/**
 * @brief Semantic primitive entry
 */
typedef struct {
    uint32_t primitive_id;
    char name[METRICS_MAX_PRIMITIVE_NAME];
    uint64_t usage_count;
    float total_relevance;      /* Sum for average calculation */
    uint64_t compression_savings;
    bool active;
} primitive_entry_t;

/**
 * @brief Protocol metrics internal state
 */
struct protocol_metrics {
    metrics_config_t config;

    /* Current window statistics */
    protocol_stats_t current_stats;
    uint64_t current_window_start_ms;

    /* Latency tracking */
    float latency_sum;
    uint64_t latency_count;

    /* Throughput tracking */
    uint64_t window_message_count;

    /* Historical data */
    stats_history_t history;

    /* Semantic primitives */
    primitive_entry_t primitives[METRICS_MAX_PRIMITIVES];
    uint32_t primitive_count;

    /* Alert system */
    metrics_alert_callback_t alert_callback;
    uint64_t last_alert_time_ms;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Timing */
    uint64_t creation_time_ms;

    /* Synchronization */
    nimcp_platform_mutex_t mutex;
};

/**
 * @brief Bio-async metrics message
 */
typedef struct {
    bio_message_header_t header;
    protocol_stats_t stats;
} bio_msg_metrics_t;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Check if current window has expired
 */
static bool is_window_expired(protocol_metrics_t* pm) {
    uint64_t now_ms = nimcp_time_get_us() / NIMCP_US_PER_MS;
    return (now_ms - pm->current_window_start_ms) >= pm->config.metrics_window_ms;
}

/**
 * @brief Rotate to next window
 *
 * WHAT: Archives current stats and starts new window
 * WHY:  Maintain time-series history
 * HOW:  Copies current to history, resets current
 */
static void rotate_window(protocol_metrics_t* pm) {
    /* Add current stats to history */
    if (pm->history.capacity > 0) {
        pm->history.buffer[pm->history.head] = pm->current_stats;
        pm->history.head = (pm->history.head + 1) % pm->history.capacity;
        if (pm->history.count < pm->history.capacity) {
            pm->history.count++;
        }
    }

    /* Calculate final throughput for window */
    uint64_t window_duration_ms = pm->config.metrics_window_ms;
    if (window_duration_ms > 0) {
        pm->current_stats.throughput_msgs_per_sec =
            (float)pm->window_message_count * (float)NIMCP_MS_PER_SEC / (float)window_duration_ms;
    }

    /* Calculate final average latency */
    if (pm->latency_count > 0) {
        pm->current_stats.avg_latency_ms = pm->latency_sum / (float)pm->latency_count;
    }

    /* Reset current window */
    memset(&pm->current_stats, 0, sizeof(protocol_stats_t));
    pm->latency_sum = 0.0F;
    pm->latency_count = 0;
    pm->window_message_count = 0;
    pm->current_window_start_ms = nimcp_time_get_us() / NIMCP_US_PER_MS;
}

/**
 * @brief Find or create primitive entry
 */
static primitive_entry_t* find_or_create_primitive(
    protocol_metrics_t* pm,
    uint32_t primitive_id
) {
    /* Search for existing primitive */
    for (uint32_t i = 0; i < pm->primitive_count; i++) {
        if (pm->primitives[i].primitive_id == primitive_id) {
            return &pm->primitives[i];
        }
    }

    /* Guard clause: max primitives reached */
    if (pm->primitive_count >= METRICS_MAX_PRIMITIVES) {
        LOG_WARN("Maximum primitives reached (%u)", METRICS_MAX_PRIMITIVES);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "find_or_create_primitive: capacity exceeded");
        return NULL;
    }

    /* Create new primitive */
    primitive_entry_t* prim = &pm->primitives[pm->primitive_count];
    prim->primitive_id = primitive_id;
    snprintf(prim->name, METRICS_MAX_PRIMITIVE_NAME, "primitive_%u", primitive_id);
    prim->usage_count = 0;
    prim->total_relevance = 0.0F;
    prim->compression_savings = 0;
    prim->active = true;

    pm->primitive_count++;
    return prim;
}

/**
 * @brief Trigger alert
 */
static void trigger_alert(protocol_metrics_t* pm, const char* alert_msg) {
    /* Rate limit alerts (min 1 second between) */
    uint64_t now_ms = nimcp_time_get_us() / NIMCP_US_PER_MS;
    if (now_ms - pm->last_alert_time_ms < NIMCP_TIMEOUT_LONG_MS) {
        return;
    }

    pm->last_alert_time_ms = now_ms;

    /* Call user callback */
    if (pm->alert_callback) {
        pm->alert_callback(alert_msg);
    }

    /* Send bio-async message */
    if (pm->bio_async_enabled && pm->bio_ctx) {
        bio_message_header_t msg = {
            .type = BIO_MSG_METRICS_ALERT,
            .timestamp_us = nimcp_time_get_us()
        };
        bio_router_broadcast(pm->bio_ctx, &msg, sizeof(msg));
    }

    LOG_WARN("METRICS ALERT: %s", alert_msg);
}

/**
 * @brief Bio-async message handler
 */
static nimcp_result_t bio_message_handler(
    void* context,
    const bio_message_header_t* msg,
    size_t msg_size
) {
    protocol_metrics_t* pm = (protocol_metrics_t*)context;
    NIMCP_CHECK_THROW(pm && msg, NIMCP_ERROR_INVALID_PARAM,
        "bio_message_handler: protocol metrics context or message is NULL");

    /* Handle metrics-related messages */
    switch (msg->type) {
        case BIO_MSG_METRICS_UPDATE:
            /* Metrics update broadcast - could trigger UI refresh */
            break;

        default:
            break;
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

protocol_metrics_t* protocol_metrics_create(const metrics_config_t* config) {
    /* Allocate metrics - nimcp_calloc zeroes all pointers for safe cleanup */
    bool mutex_initialized = false;
    protocol_metrics_t* pm = (protocol_metrics_t*)nimcp_calloc(
        1, sizeof(protocol_metrics_t)
    );
    if (!pm) {
        LOG_ERROR("Failed to allocate protocol metrics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protocol_metrics_create: failed to allocate protocol metrics");
        return NULL;
    }

    /* Initialize config with defaults if not provided */
    if (config) {
        pm->config = *config;
    } else {
        pm->config = metrics_default_config();
    }

    /* Allocate history buffer */
    if (pm->config.history_depth > 0) {
        pm->history.buffer = (protocol_stats_t*)nimcp_calloc(
            pm->config.history_depth, sizeof(protocol_stats_t)
        );
        if (!pm->history.buffer) goto cleanup;
        pm->history.capacity = pm->config.history_depth;
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&pm->mutex, false) != NIMCP_SUCCESS) goto cleanup;
    mutex_initialized = true;

    /* Initialize timing */
    pm->creation_time_ms = nimcp_time_get_us() / NIMCP_US_PER_MS;
    pm->current_window_start_ms = pm->creation_time_ms;

    /* Register with bio-async if enabled */
    if (pm->config.enable_bio_async) {
        bio_module_info_t info = {
            .module_name = METRICS_MODULE,
            .inbox_capacity = 32,
            .user_data = pm
        };

        pm->bio_ctx = bio_router_register_module(&info);
        if (pm->bio_ctx) {
            pm->bio_async_enabled = true;
            LOG_INFO("Protocol metrics registered with bio-async");
        } else {
            LOG_WARN("Failed to register with bio-async");
        }
    }

    LOG_INFO("Created protocol metrics (window=%u ms, history=%u)",
             pm->config.metrics_window_ms, pm->config.history_depth);

    return pm;

cleanup:
    LOG_ERROR("Failed to allocate protocol metrics resources");
    if (mutex_initialized) {
        nimcp_platform_mutex_destroy(&pm->mutex);
    }
    nimcp_free(pm->history.buffer);
    nimcp_free(pm);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "protocol_metrics_create: resource allocation failed");
    return NULL;
}

void protocol_metrics_destroy(protocol_metrics_t* pm) {
    if (!pm) {
        return;
    }

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Free history buffer */
    if (pm->history.buffer) {
        nimcp_free(pm->history.buffer);
    }

    /* Unregister from bio-async */
    if (pm->bio_async_enabled && pm->bio_ctx) {
        bio_router_unregister_module(pm->bio_ctx);
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    nimcp_platform_mutex_destroy(&pm->mutex);

    nimcp_free(pm);

    LOG_INFO("Destroyed protocol metrics");
}

/* ============================================================================
 * Protocol Tracking Functions
 * ============================================================================ */

int metrics_record_message(
    protocol_metrics_t* pm,
    uint32_t msg_type,
    uint32_t size,
    float latency_ms,
    bool success
) {
    NIMCP_CHECK_THROW(pm, NIMCP_ERROR_INVALID_PARAM,
        "metrics_record_message: protocol metrics context is NULL");

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Check if window expired */
    if (is_window_expired(pm)) {
        rotate_window(pm);
    }

    /* Update counters */
    pm->current_stats.messages_sent++;
    pm->current_stats.bytes_sent += size;
    pm->window_message_count++;

    /* Update latency */
    if (latency_ms > 0.0F) {
        pm->latency_sum += latency_ms;
        pm->latency_count++;
        pm->current_stats.avg_latency_ms = pm->latency_sum / (float)pm->latency_count;
    }

    /* Update error count */
    if (!success) {
        pm->current_stats.errors++;
    }

    nimcp_platform_mutex_unlock(&pm->mutex);

    /* Check for alerts */
    if (pm->config.enable_real_time_alerts) {
        metrics_check_alerts(pm);
    }

    return NIMCP_SUCCESS;
}

protocol_stats_t metrics_get_protocol_stats(protocol_metrics_t* pm) {
    protocol_stats_t stats = {0};

    if (!pm) {
        return stats;
    }

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Check if window expired */
    if (is_window_expired(pm)) {
        rotate_window(pm);
    }

    stats = pm->current_stats;

    nimcp_platform_mutex_unlock(&pm->mutex);

    return stats;
}

int metrics_get_stats_history(
    protocol_metrics_t* pm,
    protocol_stats_t** history,
    uint32_t* count
) {
    NIMCP_CHECK_THROW(pm && history && count, NIMCP_ERROR_INVALID_PARAM,
        "metrics_get_stats_history: protocol metrics context, history, or count is NULL");

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Guard clause: no history */
    if (pm->history.count == 0) {
        *history = NULL;
        *count = 0;
        nimcp_platform_mutex_unlock(&pm->mutex);
        return NIMCP_SUCCESS;
    }

    /* Allocate history array */
    *count = pm->history.count;
    *history = (protocol_stats_t*)nimcp_malloc(
        pm->history.count * sizeof(protocol_stats_t)
    );
    if (!*history) {
        nimcp_platform_mutex_unlock(&pm->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Copy history (in chronological order) */
    uint32_t start_idx = (pm->history.head + pm->history.capacity - pm->history.count) %
                         pm->history.capacity;
    for (uint32_t i = 0; i < pm->history.count; i++) {
        uint32_t idx = (start_idx + i) % pm->history.capacity;
        (*history)[i] = pm->history.buffer[idx];
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Semantic Primitive Analytics
 * ============================================================================ */

int metrics_record_primitive_usage(
    protocol_metrics_t* pm,
    uint32_t primitive_id,
    float context_relevance
) {
    NIMCP_CHECK_THROW(pm, NIMCP_ERROR_INVALID_PARAM,
        "metrics_record_primitive_usage: protocol metrics context is NULL");

    /* Guard clause: semantic tracking disabled */
    if (!pm->config.enable_semantic_tracking) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(&pm->mutex);

    primitive_entry_t* prim = find_or_create_primitive(pm, primitive_id);
    if (prim) {
        prim->usage_count++;
        prim->total_relevance += context_relevance;

        /* Estimate compression savings (placeholder) */
        prim->compression_savings += 10;  /* Assume 10 bytes saved per use */
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    return NIMCP_SUCCESS;
}

int metrics_get_primitive_stats(
    protocol_metrics_t* pm,
    semantic_primitive_stats_t** stats,
    uint32_t* count
) {
    NIMCP_CHECK_THROW(pm && stats && count, NIMCP_ERROR_INVALID_PARAM,
        "metrics_get_primitive_stats: protocol metrics context, stats, or count is NULL");

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Guard clause: no primitives */
    if (pm->primitive_count == 0) {
        *stats = NULL;
        *count = 0;
        nimcp_platform_mutex_unlock(&pm->mutex);
        return NIMCP_SUCCESS;
    }

    /* Allocate stats array */
    *count = pm->primitive_count;
    *stats = (semantic_primitive_stats_t*)nimcp_malloc(
        pm->primitive_count * sizeof(semantic_primitive_stats_t)
    );
    if (!*stats) {
        nimcp_platform_mutex_unlock(&pm->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    /* Copy primitive stats */
    for (uint32_t i = 0; i < pm->primitive_count; i++) {
        (*stats)[i].primitive_id = pm->primitives[i].primitive_id;
        strncpy((*stats)[i].name, pm->primitives[i].name,
                METRICS_MAX_PRIMITIVE_NAME - 1);
        (*stats)[i].name[METRICS_MAX_PRIMITIVE_NAME - 1] = '\0';
        (*stats)[i].usage_count = pm->primitives[i].usage_count;
        (*stats)[i].avg_context_relevance =
            pm->primitives[i].usage_count > 0
                ? pm->primitives[i].total_relevance / (float)pm->primitives[i].usage_count
                : 0.0F;
        (*stats)[i].compression_savings = pm->primitives[i].compression_savings;
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    return NIMCP_SUCCESS;
}

int metrics_get_top_primitives(
    protocol_metrics_t* pm,
    uint32_t top_n,
    semantic_primitive_stats_t** stats
) {
    NIMCP_CHECK_THROW(pm && stats, NIMCP_ERROR_INVALID_PARAM,
        "metrics_get_top_primitives: protocol metrics context or stats is NULL");

    /* Get all primitives first */
    semantic_primitive_stats_t* all_stats = NULL;
    uint32_t count = 0;
    int result = metrics_get_primitive_stats(pm, &all_stats, &count);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    /* Guard clause: no primitives */
    if (count == 0) {
        *stats = NULL;
        return NIMCP_SUCCESS;
    }

    /* Limit to available count */
    if (top_n > count) {
        top_n = count;
    }

    /* Sort by usage count (simple bubble sort for small N) */
    for (uint32_t i = 0; i < top_n; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (all_stats[j].usage_count > all_stats[i].usage_count) {
                semantic_primitive_stats_t temp = all_stats[i];
                all_stats[i] = all_stats[j];
                all_stats[j] = temp;
            }
        }
    }

    /* Allocate and copy top N */
    *stats = (semantic_primitive_stats_t*)nimcp_malloc(
        top_n * sizeof(semantic_primitive_stats_t)
    );
    if (!*stats) {
        nimcp_free(all_stats);
        return NIMCP_ERROR_MEMORY;
    }

    memcpy(*stats, all_stats, top_n * sizeof(semantic_primitive_stats_t));

    nimcp_free(all_stats);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Dashboard and Export Functions
 * ============================================================================ */

int metrics_get_dashboard_summary(
    protocol_metrics_t* pm,
    char* json_output,
    uint32_t max_size
) {
    NIMCP_CHECK_THROW(pm && json_output && max_size > 0, NIMCP_ERROR_INVALID_PARAM,
        "metrics_get_dashboard_summary: protocol metrics context, json_output, or max_size is invalid");

    nimcp_platform_mutex_lock(&pm->mutex);

    protocol_stats_t stats = pm->current_stats;
    uint64_t timestamp_us = nimcp_time_get_us();

    /* Format JSON */
    int written = snprintf(json_output, max_size,
        "{\n"
        "  \"protocol\": {\n"
        "    \"messages_sent\": %lu,\n"
        "    \"messages_received\": %lu,\n"
        "    \"bytes_sent\": %lu,\n"
        "    \"bytes_received\": %lu,\n"
        "    \"avg_latency_ms\": %.2f,\n"
        "    \"throughput_msgs_per_sec\": %.2f,\n"
        "    \"errors\": %lu,\n"
        "    \"retransmissions\": %lu\n"
        "  },\n"
        "  \"semantic\": {\n"
        "    \"tracked_primitives\": %u,\n"
        "    \"total_compression_savings\": %lu\n"
        "  },\n"
        "  \"timestamp_us\": %lu\n"
        "}\n",
        (unsigned long)stats.messages_sent,
        (unsigned long)stats.messages_received,
        (unsigned long)stats.bytes_sent,
        (unsigned long)stats.bytes_received,
        stats.avg_latency_ms,
        stats.throughput_msgs_per_sec,
        (unsigned long)stats.errors,
        (unsigned long)stats.retransmissions,
        pm->primitive_count,
        (unsigned long)metrics_get_total_compression_savings(pm),
        (unsigned long)timestamp_us
    );

    nimcp_platform_mutex_unlock(&pm->mutex);

    if (written < 0 || (uint32_t)written >= max_size) {
        return NIMCP_ERROR_BUFFER_TOO_SMALL;
    }

    return NIMCP_SUCCESS;
}

int metrics_export_csv(protocol_metrics_t* pm, const char* filepath) {
    NIMCP_CHECK_THROW(pm && filepath, NIMCP_ERROR_INVALID_PARAM,
        "metrics_export_csv: protocol metrics context or filepath is NULL");

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        LOG_ERROR("Failed to open file: %s", filepath);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Write header */
    fprintf(fp, "timestamp,messages_sent,bytes_sent,avg_latency_ms,"
                "throughput_msgs_per_sec,errors\n");

    /* Write history */
    uint32_t start_idx = (pm->history.head + pm->history.capacity - pm->history.count) %
                         pm->history.capacity;
    for (uint32_t i = 0; i < pm->history.count; i++) {
        uint32_t idx = (start_idx + i) % pm->history.capacity;
        protocol_stats_t* s = &pm->history.buffer[idx];

        fprintf(fp, "%u,%lu,%lu,%.2f,%.2f,%lu\n",
                i,
                (unsigned long)s->messages_sent,
                (unsigned long)s->bytes_sent,
                s->avg_latency_ms,
                s->throughput_msgs_per_sec,
                (unsigned long)s->errors);
    }

    nimcp_platform_mutex_unlock(&pm->mutex);

    fclose(fp);
    LOG_INFO("Exported metrics to CSV: %s", filepath);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Alert System Functions
 * ============================================================================ */

int metrics_set_alert_callback(
    protocol_metrics_t* pm,
    metrics_alert_callback_t callback
) {
    NIMCP_CHECK_THROW(pm, NIMCP_ERROR_INVALID_PARAM,
        "metrics_set_alert_callback: protocol metrics context is NULL");

    nimcp_platform_mutex_lock(&pm->mutex);
    pm->alert_callback = callback;
    nimcp_platform_mutex_unlock(&pm->mutex);

    return NIMCP_SUCCESS;
}

int metrics_check_alerts(protocol_metrics_t* pm) {
    if (!pm || !pm->config.enable_real_time_alerts) {
        return NIMCP_SUCCESS;
    }

    nimcp_platform_mutex_lock(&pm->mutex);

    protocol_stats_t* stats = &pm->current_stats;

    /* Check error rate */
    if (stats->messages_sent > 0) {
        float error_rate = (float)stats->errors / (float)stats->messages_sent;
        if (error_rate > pm->config.alert_threshold) {
            char alert[NIMCP_ERROR_BUFFER_SIZE];
            snprintf(alert, sizeof(alert),
                     "High error rate: %.2f%% (threshold: %.2f%%)",
                     error_rate * 100.0F, pm->config.alert_threshold * 100.0F);
            trigger_alert(pm, alert);
        }
    }

    /* Check latency spike (if we have history) */
    if (pm->history.count > 0 && stats->avg_latency_ms > 0.0F) {
        /* Calculate average latency from history */
        float hist_avg = 0.0F;
        for (uint32_t i = 0; i < pm->history.count; i++) {
            hist_avg += pm->history.buffer[i].avg_latency_ms;
        }
        hist_avg /= (float)pm->history.count;

        /* Alert if current latency is 2x historical average */
        if (stats->avg_latency_ms > hist_avg * 2.0F && hist_avg > EPSILON) {
            char alert[NIMCP_ERROR_BUFFER_SIZE];
            snprintf(alert, sizeof(alert),
                     "Latency spike: %.2f ms (avg: %.2f ms)",
                     stats->avg_latency_ms, hist_avg);
            trigger_alert(pm, alert);
        }
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

metrics_config_t metrics_default_config(void) {
    metrics_config_t config = {
        .metrics_window_ms = METRICS_DEFAULT_WINDOW_MS,
        .history_depth = METRICS_DEFAULT_HISTORY_DEPTH,
        .enable_semantic_tracking = true,
        .enable_real_time_alerts = true,
        .alert_threshold = METRICS_DEFAULT_ALERT_THRESHOLD,
        .enable_bio_async = true
    };
    return config;
}

int metrics_reset_all(protocol_metrics_t* pm) {
    NIMCP_CHECK_THROW(pm, NIMCP_ERROR_INVALID_PARAM,
        "metrics_reset_all: protocol metrics context is NULL");

    nimcp_platform_mutex_lock(&pm->mutex);

    /* Reset current stats */
    memset(&pm->current_stats, 0, sizeof(protocol_stats_t));
    pm->latency_sum = 0.0F;
    pm->latency_count = 0;
    pm->window_message_count = 0;

    /* Reset history */
    pm->history.head = 0;
    pm->history.count = 0;

    /* Reset primitives */
    for (uint32_t i = 0; i < pm->primitive_count; i++) {
        pm->primitives[i].usage_count = 0;
        pm->primitives[i].total_relevance = 0.0F;
        pm->primitives[i].compression_savings = 0;
    }

    /* Reset timing */
    pm->current_window_start_ms = nimcp_time_get_us() / NIMCP_US_PER_MS;

    nimcp_platform_mutex_unlock(&pm->mutex);

    LOG_INFO("Reset all metrics");
    return NIMCP_SUCCESS;
}

uint64_t metrics_get_uptime_ms(protocol_metrics_t* pm) {
    if (!pm) {
        return 0;
    }

    nimcp_platform_mutex_lock(&pm->mutex);
    uint64_t uptime = (nimcp_time_get_us() / NIMCP_US_PER_MS) - pm->creation_time_ms;
    nimcp_platform_mutex_unlock(&pm->mutex);

    return uptime;
}

int metrics_set_primitive_name(
    protocol_metrics_t* pm,
    uint32_t primitive_id,
    const char* name
) {
    NIMCP_CHECK_THROW(pm && name, NIMCP_ERROR_INVALID_PARAM,
        "metrics_set_primitive_name: protocol metrics context or name is NULL");

    nimcp_platform_mutex_lock(&pm->mutex);

    primitive_entry_t* prim = find_or_create_primitive(pm, primitive_id);
    if (prim) {
        strncpy(prim->name, name, METRICS_MAX_PRIMITIVE_NAME - 1);
        prim->name[METRICS_MAX_PRIMITIVE_NAME - 1] = '\0';
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    return NIMCP_SUCCESS;
}

uint64_t metrics_get_total_compression_savings(protocol_metrics_t* pm) {
    if (!pm) {
        return 0;
    }

    nimcp_platform_mutex_lock(&pm->mutex);

    uint64_t total = 0;
    for (uint32_t i = 0; i < pm->primitive_count; i++) {
        total += pm->primitives[i].compression_savings;
    }

    nimcp_platform_mutex_unlock(&pm->mutex);
    return total;
}
