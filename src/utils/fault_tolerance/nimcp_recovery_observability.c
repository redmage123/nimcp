/**
 * @file nimcp_recovery_observability.c
 * @brief Recovery Metrics and Observability Implementation
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
 * IMMUNE SYSTEM INTEGRATION:
 * - All recovery events are audited via security module
 * - Anomalous recovery patterns trigger security alerts
 * - Metrics exposed for security analysis
 */

#include "utils/fault_tolerance/nimcp_recovery_observability.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recovery_observability)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Recovery duration sample for MTTR calculation
 */
typedef struct {
    uint64_t duration_ms;
    bool success;
    uint32_t fault_type;
} ro_recovery_sample_t;

/**
 * @brief Failure timestamp for MTBF calculation
 */
typedef struct {
    uint64_t timestamp_ns;
    uint32_t fault_type;
} ro_failure_record_t;

/**
 * @brief Exporter registration
 */
typedef struct {
    ro_export_callback_t callback;
    ro_export_format_t format;
    void* user_data;
    bool active;
} ro_exporter_t;

/**
 * @brief Internal context structure
 */
struct ro_context {
    ro_config_t config;

    /* Counters */
    ro_counter_t counters[RO_MAX_METRICS];
    uint32_t counter_count;

    /* Gauges */
    ro_gauge_t gauges[RO_MAX_METRICS];
    uint32_t gauge_count;

    /* Histograms */
    ro_histogram_t histograms[RO_MAX_METRICS];
    uint32_t histogram_count;

    /* Spans */
    ro_span_t spans[RO_MAX_SPANS];
    uint32_t span_count;
    uint32_t span_head;

    /* Events */
    ro_event_t events[RO_MAX_EVENTS];
    uint32_t event_head;
    uint32_t event_count;

    /* Recovery samples for MTTR */
    ro_recovery_sample_t recovery_samples[256];
    uint32_t recovery_sample_head;
    uint32_t recovery_sample_count;

    /* Failure records for MTBF */
    ro_failure_record_t failure_records[256];
    uint32_t failure_record_head;
    uint32_t failure_record_count;
    uint64_t start_time_ns;

    /* Active recoveries */
    ro_recovery_context_t* active_recoveries[32];
    uint32_t active_recovery_count;
    uint32_t next_recovery_id;

    /* Exporters */
    ro_exporter_t exporters[RO_MAX_EXPORTERS];
    uint32_t exporter_count;

    /* Built-in metrics */
    ro_counter_t* failures_total;
    ro_counter_t* recoveries_total;
    ro_counter_t* recoveries_success;
    ro_counter_t* recoveries_failed;
    ro_histogram_t* recovery_duration_hist;
    ro_gauge_t* active_recoveries_gauge;

    /* Threading */
    nimcp_mutex_t mutex;
    bool running;

    /* Security integration */
    bool security_registered;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current timestamp in nanoseconds
 */
static uint64_t ro_get_time_ns_internal(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Generate random bytes for IDs
 */
static void ro_generate_random_bytes(uint8_t* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(nimcp_tl_rand() & 0xFF);
    }
}

/**
 * @brief Calculate percentile from histogram
 */
static double ro_histogram_percentile_internal(const ro_histogram_t* hist, double percentile) {
    if (!hist || hist->sample_count == 0) return 0.0;

    uint64_t target = (uint64_t)(hist->sample_count * percentile);
    uint64_t cumulative = 0;

    for (uint32_t i = 0; i < hist->bucket_count; i++) {
        cumulative += hist->buckets[i].count;
        if (cumulative >= target) {
            /* Linear interpolation within bucket */
            if (i == 0) {
                return hist->buckets[i].upper_bound / 2.0;
            }
            double lower = hist->buckets[i - 1].upper_bound;
            double upper = hist->buckets[i].upper_bound;
            return (lower + upper) / 2.0;
        }
    }

    return hist->max;
}

/**
 * @brief Notify security of recovery events
 */
static void ro_notify_security(ro_context_t* ctx, const ro_event_t* event) {
    if (!ctx || !event) return;

    bbb_audit_level_t level = BBB_AUDIT_INFO;

    switch (event->severity) {
        case RO_SEVERITY_ERROR:
        case RO_SEVERITY_FATAL:
            level = BBB_AUDIT_ERROR;
            break;
        case RO_SEVERITY_WARN:
            level = BBB_AUDIT_WARNING;
            break;
        default:
            level = BBB_AUDIT_INFO;
            break;
    }

    bbb_audit_log(level, "RO", ro_event_type_to_string(event->type),
                  "node=%u, fault=%u, success=%d: %s",
                  event->node_id, event->fault_type,
                  event->success, event->message);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

ro_config_t ro_default_config(void) {
    ro_config_t config = {
        .enable_tracing = true,
        .enable_metrics = true,
        .enable_events = true,
        .event_buffer_size = RO_MAX_EVENTS,
        .metrics_interval_ms = 1000,
        .trace_sample_rate = 1,
        .export_format = RO_EXPORT_JSON,
        .retention_ms = 3600000  /* 1 hour */
    };
    return config;
}

ro_context_t* ro_create(const ro_config_t* config) {
    if (!config) {
        LOG_ERROR("RO", "NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ro_create: config is NULL");
        return NULL;
    }

    ro_context_t* ctx = (ro_context_t*)nimcp_malloc(sizeof(ro_context_t));
    if (!ctx) {
        LOG_ERROR("RO", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ro_create: ctx is NULL");
        return NULL;
    }

    memset(ctx, 0, sizeof(ro_context_t));
    ctx->config = *config;
    ctx->next_recovery_id = 1;
    ctx->start_time_ns = ro_get_time_ns_internal();

    if (nimcp_mutex_init(&ctx->mutex, NULL) != 0) {
        LOG_ERROR("RO", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "ro_create: validation failed");
        return NULL;
    }

    /* Create built-in metrics */
    if (config->enable_metrics) {
        ctx->failures_total = ro_create_counter(ctx, "ro_failures_total", NULL, 0);
        ctx->recoveries_total = ro_create_counter(ctx, "ro_recoveries_total", NULL, 0);
        ctx->recoveries_success = ro_create_counter(ctx, "ro_recoveries_success", NULL, 0);
        ctx->recoveries_failed = ro_create_counter(ctx, "ro_recoveries_failed", NULL, 0);
        ctx->active_recoveries_gauge = ro_create_gauge(ctx, "ro_active_recoveries", NULL, 0);

        /* MTTR histogram with buckets in ms */
        double buckets[] = {10, 50, 100, 250, 500, 1000, 2500, 5000, 10000, 30000, 60000};
        ctx->recovery_duration_hist = ro_create_histogram(ctx, "ro_recovery_duration_ms",
                                                          buckets, 11, NULL, 0);
    }

    /* Register with security module */
    ctx->security_registered = bbb_register_module("recovery_observability", BBB_MODULE_TYPE_CORE);

    bbb_audit_log(BBB_AUDIT_INFO, "RO", "CREATE",
                  "Created recovery observability context");

    LOG_INFO("RO", "Created recovery observability context");

    return ctx;
}

void ro_destroy(ro_context_t* ctx) {
    if (!ctx) return;

    ro_stop(ctx);

    /* Free active recoveries */
    for (uint32_t i = 0; i < ctx->active_recovery_count; i++) {
        if (ctx->active_recoveries[i]) {
            nimcp_free(ctx->active_recoveries[i]);
        }
    }

    bbb_audit_log(BBB_AUDIT_INFO, "RO", "DESTROY",
                  "Destroying recovery observability context");

    nimcp_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx);

    LOG_INFO("RO", "Destroyed recovery observability context");
}

bool ro_start(ro_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_start: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->running = true;
    ctx->start_time_ns = ro_get_time_ns_internal();
    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "RO", "START", "Started observability collection");
    LOG_INFO("RO", "Started observability collection");

    return true;
}

bool ro_stop(ro_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_stop: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->running = false;
    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "RO", "STOP", "Stopped observability collection");
    LOG_INFO("RO", "Stopped observability collection");

    return true;
}

//=============================================================================
// Counter Operations
//=============================================================================

ro_counter_t* ro_create_counter(ro_context_t* ctx, const char* name,
                                 const ro_label_t* labels, uint32_t label_count) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ro_stop: required parameter is NULL (ctx, name)");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->counter_count >= RO_MAX_METRICS) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "ro_stop: capacity exceeded");
        return NULL;
    }

    ro_counter_t* counter = &ctx->counters[ctx->counter_count++];
    memset(counter, 0, sizeof(ro_counter_t));
    strncpy(counter->name, name, sizeof(counter->name) - 1);

    if (labels && label_count > 0) {
        uint32_t copy_count = label_count < RO_MAX_LABELS ? label_count : RO_MAX_LABELS;
        memcpy(counter->labels, labels, copy_count * sizeof(ro_label_t));
        counter->label_count = copy_count;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return counter;
}

uint64_t ro_counter_inc(ro_counter_t* counter, uint64_t delta) {
    if (!counter) return 0;
    return __sync_add_and_fetch(&counter->value, delta);
}

uint64_t ro_counter_get(const ro_counter_t* counter) {
    if (!counter) return 0;
    return counter->value;
}

//=============================================================================
// Gauge Operations
//=============================================================================

ro_gauge_t* ro_create_gauge(ro_context_t* ctx, const char* name,
                             const ro_label_t* labels, uint32_t label_count) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ro_counter_get: required parameter is NULL (ctx, name)");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->gauge_count >= RO_MAX_METRICS) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "ro_counter_get: capacity exceeded");
        return NULL;
    }

    ro_gauge_t* gauge = &ctx->gauges[ctx->gauge_count++];
    memset(gauge, 0, sizeof(ro_gauge_t));
    strncpy(gauge->name, name, sizeof(gauge->name) - 1);

    if (labels && label_count > 0) {
        uint32_t copy_count = label_count < RO_MAX_LABELS ? label_count : RO_MAX_LABELS;
        memcpy(gauge->labels, labels, copy_count * sizeof(ro_label_t));
        gauge->label_count = copy_count;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return gauge;
}

void ro_gauge_set(ro_gauge_t* gauge, double value) {
    if (!gauge) return;
    gauge->value = value;
}

void ro_gauge_inc(ro_gauge_t* gauge, double delta) {
    if (!gauge) return;
    gauge->value += delta;
}

void ro_gauge_dec(ro_gauge_t* gauge, double delta) {
    if (!gauge) return;
    gauge->value -= delta;
}

double ro_gauge_get(const ro_gauge_t* gauge) {
    if (!gauge) return 0.0;
    return gauge->value;
}

//=============================================================================
// Histogram Operations
//=============================================================================

ro_histogram_t* ro_create_histogram(ro_context_t* ctx, const char* name,
                                     const double* buckets, uint32_t bucket_count,
                                     const ro_label_t* labels, uint32_t label_count) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ro_gauge_get: required parameter is NULL (ctx, name)");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->histogram_count >= RO_MAX_METRICS) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "ro_gauge_get: capacity exceeded");
        return NULL;
    }

    ro_histogram_t* hist = &ctx->histograms[ctx->histogram_count++];
    memset(hist, 0, sizeof(ro_histogram_t));
    strncpy(hist->name, name, sizeof(hist->name) - 1);
    hist->min = INFINITY;
    hist->max = -INFINITY;

    /* Set up buckets */
    uint32_t copy_count = bucket_count < RO_HISTOGRAM_BUCKETS ? bucket_count : RO_HISTOGRAM_BUCKETS;
    for (uint32_t i = 0; i < copy_count; i++) {
        hist->buckets[i].upper_bound = buckets[i];
        hist->buckets[i].count = 0;
    }
    hist->bucket_count = copy_count;

    if (labels && label_count > 0) {
        uint32_t lbl_count = label_count < RO_MAX_LABELS ? label_count : RO_MAX_LABELS;
        memcpy(hist->labels, labels, lbl_count * sizeof(ro_label_t));
        hist->label_count = lbl_count;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return hist;
}

void ro_histogram_observe(ro_histogram_t* hist, double value) {
    if (!hist) return;

    hist->sample_count++;
    hist->sample_sum += value;

    if (value < hist->min) hist->min = value;
    if (value > hist->max) hist->max = value;

    /* Find bucket */
    for (uint32_t i = 0; i < hist->bucket_count; i++) {
        if (value <= hist->buckets[i].upper_bound) {
            hist->buckets[i].count++;
            return;
        }
    }

    /* Value exceeds all buckets - put in last */
    if (hist->bucket_count > 0) {
        hist->buckets[hist->bucket_count - 1].count++;
    }
}

double ro_histogram_percentile(const ro_histogram_t* hist, double percentile) {
    return ro_histogram_percentile_internal(hist, percentile);
}

double ro_histogram_mean(const ro_histogram_t* hist) {
    if (!hist || hist->sample_count == 0) return 0.0;
    return hist->sample_sum / (double)hist->sample_count;
}

//=============================================================================
// Tracing Operations
//=============================================================================

/**
 * @brief Internal unlocked trace start - caller must hold ctx->mutex
 */
static ro_span_t* ro_start_trace_unlocked(ro_context_t* ctx, const char* name) {
    if (ctx->span_count >= RO_MAX_SPANS) {
        /* Wrap around */
        ctx->span_head = (ctx->span_head + 1) % RO_MAX_SPANS;
    } else {
        ctx->span_count++;
    }

    ro_span_t* span = &ctx->spans[ctx->span_head];
    memset(span, 0, sizeof(ro_span_t));

    ro_generate_random_bytes(span->trace_id, RO_TRACE_ID_SIZE);
    ro_generate_random_bytes(span->span_id, RO_SPAN_ID_SIZE);
    strncpy(span->name, name, sizeof(span->name) - 1);
    span->start_time_ns = ro_get_time_ns_internal();
    span->is_recording = true;

    ctx->span_head = (ctx->span_head + 1) % RO_MAX_SPANS;

    return span;
}

ro_span_t* ro_start_trace(ro_context_t* ctx, const char* name) {
    if (!ctx || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_start_trace: required parameter is NULL (ctx, name)");
        return NULL;
    }
    if (!ctx->config.enable_tracing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_start_trace: ctx->config is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->mutex);

    ro_span_t* span = ro_start_trace_unlocked(ctx, name);

    nimcp_mutex_unlock(&ctx->mutex);

    return span;
}

ro_span_t* ro_start_span(ro_context_t* ctx, const ro_span_t* parent, const char* name) {
    if (!ctx || !parent || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_start_span: required parameter is NULL (ctx, parent, name)");
        return NULL;
    }
    if (!ctx->config.enable_tracing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_start_span: ctx->config is NULL");
        return NULL;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->span_count >= RO_MAX_SPANS) {
        ctx->span_head = (ctx->span_head + 1) % RO_MAX_SPANS;
    } else {
        ctx->span_count++;
    }

    ro_span_t* span = &ctx->spans[ctx->span_head];
    memset(span, 0, sizeof(ro_span_t));

    memcpy(span->trace_id, parent->trace_id, RO_TRACE_ID_SIZE);
    ro_generate_random_bytes(span->span_id, RO_SPAN_ID_SIZE);
    memcpy(span->parent_span_id, parent->span_id, RO_SPAN_ID_SIZE);
    strncpy(span->name, name, sizeof(span->name) - 1);
    span->start_time_ns = ro_get_time_ns_internal();
    span->is_recording = true;

    ctx->span_head = (ctx->span_head + 1) % RO_MAX_SPANS;

    nimcp_mutex_unlock(&ctx->mutex);

    return span;
}

void ro_end_span(ro_span_t* span, ro_span_status_t status, const char* message) {
    if (!span) return;

    span->end_time_ns = ro_get_time_ns_internal();
    span->status = status;
    span->is_recording = false;

    if (message) {
        strncpy(span->status_message, message, sizeof(span->status_message) - 1);
    }
}

void ro_span_set_attribute(ro_span_t* span, const char* key, const char* value) {
    if (!span || !key || !value) return;
    if (span->attribute_count >= RO_MAX_LABELS) return;

    ro_label_t* attr = &span->attributes[span->attribute_count++];
    strncpy(attr->key, key, sizeof(attr->key) - 1);
    strncpy(attr->value, value, sizeof(attr->value) - 1);
}

void ro_span_add_event(ro_span_t* span, const char* name) {
    (void)span;
    (void)name;
    /* Simplified: just log for now */
}

uint64_t ro_span_duration_ns(const ro_span_t* span) {
    if (!span) return 0;
    if (span->end_time_ns == 0) {
        return ro_get_time_ns_internal() - span->start_time_ns;
    }
    return span->end_time_ns - span->start_time_ns;
}

//=============================================================================
// Recovery-Specific Metrics
//=============================================================================

ro_recovery_context_t* ro_start_recovery(ro_context_t* ctx, uint32_t fault_type) {
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;

    }

    ro_recovery_context_t* recovery = (ro_recovery_context_t*)nimcp_malloc(sizeof(ro_recovery_context_t));
    if (!recovery) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery is NULL");

        return NULL;

    }

    memset(recovery, 0, sizeof(ro_recovery_context_t));

    nimcp_mutex_lock(&ctx->mutex);

    recovery->recovery_id = ctx->next_recovery_id++;
    recovery->fault_type = fault_type;
    recovery->start_time_ns = ro_get_time_ns_internal();

    /* Start trace span (use unlocked version since we already hold ctx->mutex) */
    if (ctx->config.enable_tracing) {
        char span_name[NIMCP_ID_BUFFER_SIZE];
        snprintf(span_name, sizeof(span_name), "recovery_%u", recovery->recovery_id);
        ro_span_t* span = ro_start_trace_unlocked(ctx, span_name);
        if (span) {
            recovery->root_span = *span;
        }
    }

    /* Track active recovery */
    if (ctx->active_recovery_count < 32) {
        ctx->active_recoveries[ctx->active_recovery_count++] = recovery;
    }

    /* Update gauge */
    if (ctx->active_recoveries_gauge) {
        ro_gauge_set(ctx->active_recoveries_gauge, (double)ctx->active_recovery_count);
    }

    /* Increment counter */
    if (ctx->recoveries_total) {
        ro_counter_inc(ctx->recoveries_total, 1);
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Log event */
    ro_event_t event = {0};
    event.type = RO_EVENT_RECOVERY_STARTED;
    event.severity = RO_SEVERITY_INFO;
    event.timestamp_ns = recovery->start_time_ns;
    event.fault_type = fault_type;
    event.recovery_id = recovery->recovery_id;
    snprintf(event.message, sizeof(event.message), "Recovery started for fault type %u", fault_type);
    ro_log_event(ctx, &event);

    return recovery;
}

void ro_end_recovery(ro_context_t* ctx, ro_recovery_context_t* recovery, bool success) {
    if (!ctx || !recovery) return;

    recovery->end_time_ns = ro_get_time_ns_internal();
    recovery->is_complete = true;
    recovery->success = success;

    uint64_t duration_ms = (recovery->end_time_ns - recovery->start_time_ns) / 1000000ULL;

    nimcp_mutex_lock(&ctx->mutex);

    /* Record sample for MTTR */
    uint32_t idx = ctx->recovery_sample_head;
    ctx->recovery_samples[idx].duration_ms = duration_ms;
    ctx->recovery_samples[idx].success = success;
    ctx->recovery_samples[idx].fault_type = recovery->fault_type;
    ctx->recovery_sample_head = (ctx->recovery_sample_head + 1) % 256;
    if (ctx->recovery_sample_count < 256) {
        ctx->recovery_sample_count++;
    }

    /* Update histogram */
    if (ctx->recovery_duration_hist) {
        ro_histogram_observe(ctx->recovery_duration_hist, (double)duration_ms);
    }

    /* Update counters */
    if (success && ctx->recoveries_success) {
        ro_counter_inc(ctx->recoveries_success, 1);
    } else if (!success && ctx->recoveries_failed) {
        ro_counter_inc(ctx->recoveries_failed, 1);
    }

    /* Remove from active */
    for (uint32_t i = 0; i < ctx->active_recovery_count; i++) {
        if (ctx->active_recoveries[i] == recovery) {
            for (uint32_t j = i; j < ctx->active_recovery_count - 1; j++) {
                ctx->active_recoveries[j] = ctx->active_recoveries[j + 1];
            }
            ctx->active_recovery_count--;
            break;
        }
    }

    /* Update gauge */
    if (ctx->active_recoveries_gauge) {
        ro_gauge_set(ctx->active_recoveries_gauge, (double)ctx->active_recovery_count);
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* End span */
    ro_end_span(&recovery->root_span,
                success ? RO_SPAN_OK : RO_SPAN_ERROR,
                success ? "Recovery successful" : "Recovery failed");

    /* Log event */
    ro_event_t event = {0};
    event.type = success ? RO_EVENT_RECOVERY_SUCCESS : RO_EVENT_RECOVERY_FAILED;
    event.severity = success ? RO_SEVERITY_INFO : RO_SEVERITY_ERROR;
    event.timestamp_ns = recovery->end_time_ns;
    event.fault_type = recovery->fault_type;
    event.recovery_id = recovery->recovery_id;
    event.duration_ms = duration_ms;
    event.success = success;
    snprintf(event.message, sizeof(event.message),
             "Recovery %s for fault %u after %lu ms",
             success ? "succeeded" : "failed", recovery->fault_type, (unsigned long)duration_ms);
    ro_log_event(ctx, &event);

    nimcp_free(recovery);
}

void ro_record_recovery_attempt(ro_recovery_context_t* recovery, const char* strategy, bool success) {
    if (!recovery) return;
    recovery->attempt_count++;
    (void)strategy;
    (void)success;
}

bool ro_get_mttr_stats(ro_context_t* ctx, ro_mttr_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_get_mttr_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    memset(stats, 0, sizeof(ro_mttr_stats_t));

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->recovery_sample_count == 0) {
        nimcp_mutex_unlock(&ctx->mutex);
        return true;
    }

    /* Calculate statistics from samples */
    double sum = 0.0;
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;

    for (uint32_t i = 0; i < ctx->recovery_sample_count; i++) {
        ro_recovery_sample_t* sample = &ctx->recovery_samples[i];

        stats->total_recoveries++;
        if (sample->success) {
            stats->successful_recoveries++;
        } else {
            stats->failed_recoveries++;
        }

        sum += sample->duration_ms;
        if (sample->duration_ms < min) min = sample->duration_ms;
        if (sample->duration_ms > max) max = sample->duration_ms;
    }

    stats->mttr_ms = sum / ctx->recovery_sample_count;
    stats->min_recovery_ms = (double)min;
    stats->max_recovery_ms = (double)max;

    if (stats->total_recoveries > 0) {
        stats->success_rate = (double)stats->successful_recoveries / (double)stats->total_recoveries;
    }

    /* Get percentiles from histogram */
    if (ctx->recovery_duration_hist) {
        stats->mttr_p50_ms = ro_histogram_percentile(ctx->recovery_duration_hist, 0.50);
        stats->mttr_p95_ms = ro_histogram_percentile(ctx->recovery_duration_hist, 0.95);
        stats->mttr_p99_ms = ro_histogram_percentile(ctx->recovery_duration_hist, 0.99);
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ro_get_mtbf_stats(ro_context_t* ctx, ro_mtbf_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_get_mtbf_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    memset(stats, 0, sizeof(ro_mtbf_stats_t));

    nimcp_mutex_lock(&ctx->mutex);

    uint64_t now = ro_get_time_ns_internal();
    stats->uptime_ms = (now - ctx->start_time_ns) / 1000000ULL;
    stats->total_failures = ctx->failure_record_count;

    if (ctx->failure_record_count >= 2) {
        /* Calculate mean time between failures */
        double sum = 0.0;
        uint64_t min = UINT64_MAX;
        uint64_t max = 0;

        for (uint32_t i = 1; i < ctx->failure_record_count; i++) {
            uint32_t prev_idx = (ctx->failure_record_head + 256 - ctx->failure_record_count + i - 1) % 256;
            uint32_t curr_idx = (ctx->failure_record_head + 256 - ctx->failure_record_count + i) % 256;

            uint64_t delta = ctx->failure_records[curr_idx].timestamp_ns -
                            ctx->failure_records[prev_idx].timestamp_ns;
            uint64_t delta_ms = delta / 1000000ULL;

            sum += delta_ms;
            if (delta_ms < min) min = delta_ms;
            if (delta_ms > max) max = delta_ms;
        }

        stats->mtbf_ms = sum / (ctx->failure_record_count - 1);
        stats->min_tbf_ms = (double)min;
        stats->max_tbf_ms = (double)max;
    } else if (stats->total_failures == 1) {
        stats->mtbf_ms = (double)stats->uptime_ms;
    } else {
        stats->mtbf_ms = (double)stats->uptime_ms;
    }

    /* Calculate availability */
    if (stats->uptime_ms > 0 && stats->total_failures > 0) {
        ro_mttr_stats_t mttr;
        nimcp_mutex_unlock(&ctx->mutex);
        ro_get_mttr_stats(ctx, &mttr);
        nimcp_mutex_lock(&ctx->mutex);

        double downtime = mttr.mttr_ms * stats->total_failures;
        stats->availability = 1.0 - (downtime / stats->uptime_ms);
        if (stats->availability < 0.0) stats->availability = 0.0;
    } else {
        stats->availability = 1.0;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

void ro_record_failure(ro_context_t* ctx, uint32_t node_id, uint32_t fault_type) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->mutex);

    uint64_t now = ro_get_time_ns_internal();

    /* Record failure */
    uint32_t idx = ctx->failure_record_head;
    ctx->failure_records[idx].timestamp_ns = now;
    ctx->failure_records[idx].fault_type = fault_type;
    ctx->failure_record_head = (ctx->failure_record_head + 1) % 256;
    if (ctx->failure_record_count < 256) {
        ctx->failure_record_count++;
    }

    /* Update counter */
    if (ctx->failures_total) {
        ro_counter_inc(ctx->failures_total, 1);
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Log event */
    ro_event_t event = {0};
    event.type = RO_EVENT_FAILURE_DETECTED;
    event.severity = RO_SEVERITY_ERROR;
    event.timestamp_ns = now;
    event.node_id = node_id;
    event.fault_type = fault_type;
    snprintf(event.message, sizeof(event.message),
             "Failure detected: node=%u, fault_type=%u", node_id, fault_type);
    ro_log_event(ctx, &event);
}

//=============================================================================
// Event Logging
//=============================================================================

bool ro_log_event(ro_context_t* ctx, const ro_event_t* event) {
    if (!ctx || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_log_event: required parameter is NULL (ctx, event)");
        return false;
    }
    if (!ctx->config.enable_events) return true;

    nimcp_mutex_lock(&ctx->mutex);

    ctx->events[ctx->event_head] = *event;
    ctx->event_head = (ctx->event_head + 1) % RO_MAX_EVENTS;
    if (ctx->event_count < RO_MAX_EVENTS) {
        ctx->event_count++;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    /* Notify security */
    ro_notify_security(ctx, event);

    return true;
}

uint32_t ro_get_events(ro_context_t* ctx, ro_event_t* events, uint32_t max_events, uint64_t since_timestamp) {
    if (!ctx || !events || max_events == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t count = 0;
    uint32_t start = (ctx->event_head + RO_MAX_EVENTS - ctx->event_count) % RO_MAX_EVENTS;

    for (uint32_t i = 0; i < ctx->event_count && count < max_events; i++) {
        uint32_t idx = (start + i) % RO_MAX_EVENTS;
        if (ctx->events[idx].timestamp_ns >= since_timestamp) {
            events[count++] = ctx->events[idx];
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

uint32_t ro_get_events_by_type(ro_context_t* ctx, ro_event_type_t type,
                                ro_event_t* events, uint32_t max_events) {
    if (!ctx || !events || max_events == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t count = 0;
    uint32_t start = (ctx->event_head + RO_MAX_EVENTS - ctx->event_count) % RO_MAX_EVENTS;

    for (uint32_t i = 0; i < ctx->event_count && count < max_events; i++) {
        uint32_t idx = (start + i) % RO_MAX_EVENTS;
        if (ctx->events[idx].type == type) {
            events[count++] = ctx->events[idx];
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

//=============================================================================
// Export Operations
//=============================================================================

/**
 * @brief Internal unlocked metrics export - caller must hold ctx->mutex
 */
static size_t ro_export_metrics_unlocked(ro_context_t* ctx, ro_export_format_t format, char* buffer, size_t buffer_size) {
    size_t written = 0;

    if (format == RO_EXPORT_JSON) {
        int n = snprintf(buffer, buffer_size, "{\n  \"counters\": [\n");
        if (n > 0) written += (size_t)n;
        if (written >= buffer_size) return buffer_size - 1;

        for (uint32_t i = 0; i < ctx->counter_count && written < buffer_size - 100; i++) {
            n = snprintf(buffer + written, buffer_size - written,
                               "    {\"name\": \"%s\", \"value\": %lu}%s\n",
                               ctx->counters[i].name, (unsigned long)ctx->counters[i].value,
                               i < ctx->counter_count - 1 ? "," : "");
            if (n > 0) written += (size_t)n;
            if (written >= buffer_size) return buffer_size - 1;
        }

        n = snprintf(buffer + written, buffer_size - written, "  ],\n  \"gauges\": [\n");
        if (n > 0) written += (size_t)n;
        if (written >= buffer_size) return buffer_size - 1;

        for (uint32_t i = 0; i < ctx->gauge_count && written < buffer_size - 100; i++) {
            n = snprintf(buffer + written, buffer_size - written,
                               "    {\"name\": \"%s\", \"value\": %.6f}%s\n",
                               ctx->gauges[i].name, ctx->gauges[i].value,
                               i < ctx->gauge_count - 1 ? "," : "");
            if (n > 0) written += (size_t)n;
            if (written >= buffer_size) return buffer_size - 1;
        }

        n = snprintf(buffer + written, buffer_size - written, "  ]\n}\n");
        if (n > 0) written += (size_t)n;
        if (written >= buffer_size) return buffer_size - 1;
    } else if (format == RO_EXPORT_PROMETHEUS) {
        for (uint32_t i = 0; i < ctx->counter_count && written < buffer_size - 100; i++) {
            int n = snprintf(buffer + written, buffer_size - written,
                               "# TYPE %s counter\n%s %lu\n",
                               ctx->counters[i].name, ctx->counters[i].name,
                               (unsigned long)ctx->counters[i].value);
            if (n > 0) written += (size_t)n;
            if (written >= buffer_size) return buffer_size - 1;
        }

        for (uint32_t i = 0; i < ctx->gauge_count && written < buffer_size - 100; i++) {
            int n = snprintf(buffer + written, buffer_size - written,
                               "# TYPE %s gauge\n%s %.6f\n",
                               ctx->gauges[i].name, ctx->gauges[i].name,
                               ctx->gauges[i].value);
            if (n > 0) written += (size_t)n;
            if (written >= buffer_size) return buffer_size - 1;
        }
    }

    return written;
}

size_t ro_export_metrics(ro_context_t* ctx, ro_export_format_t format, char* buffer, size_t buffer_size) {
    if (!ctx || !buffer || buffer_size == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    size_t written = ro_export_metrics_unlocked(ctx, format, buffer, buffer_size);

    nimcp_mutex_unlock(&ctx->mutex);

    return written;
}

size_t ro_export_traces(ro_context_t* ctx, ro_export_format_t format, char* buffer, size_t buffer_size) {
    if (!ctx || !buffer || buffer_size == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    size_t written = 0;

    if (format == RO_EXPORT_JSON) {
        written = snprintf(buffer, buffer_size, "{\n  \"spans\": [\n");

        for (uint32_t i = 0; i < ctx->span_count && written < buffer_size - 200; i++) {
            ro_span_t* span = &ctx->spans[i];
            written += snprintf(buffer + written, buffer_size - written,
                               "    {\"name\": \"%s\", \"duration_ns\": %lu, \"status\": \"%s\"}%s\n",
                               span->name, (unsigned long)ro_span_duration_ns(span),
                               ro_span_status_to_string(span->status),
                               i < ctx->span_count - 1 ? "," : "");
        }

        written += snprintf(buffer + written, buffer_size - written, "  ]\n}\n");
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return written;
}

bool ro_register_exporter(ro_context_t* ctx, ro_export_callback_t callback,
                           ro_export_format_t format, void* user_data) {
    if (!ctx || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_export_traces: required parameter is NULL (ctx, callback)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    if (ctx->exporter_count >= RO_MAX_EXPORTERS) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "ro_export_traces: capacity exceeded");
        return false;
    }

    ro_exporter_t* exp = &ctx->exporters[ctx->exporter_count++];
    exp->callback = callback;
    exp->format = format;
    exp->user_data = user_data;
    exp->active = true;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool ro_flush(ro_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ro_flush: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    char buffer[NIMCP_PATH_BUFFER_SIZE];

    for (uint32_t i = 0; i < ctx->exporter_count; i++) {
        if (ctx->exporters[i].active && ctx->exporters[i].callback) {
            size_t written = ro_export_metrics_unlocked(ctx, ctx->exporters[i].format, buffer, sizeof(buffer));
            if (written > 0) {
                ctx->exporters[i].callback(buffer, written, ctx->exporters[i].format,
                                           ctx->exporters[i].user_data);
            }
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

//=============================================================================
// Utility Functions
//=============================================================================

void ro_generate_trace_id(uint8_t* trace_id) {
    if (!trace_id) return;
    ro_generate_random_bytes(trace_id, RO_TRACE_ID_SIZE);
}

void ro_generate_span_id(uint8_t* span_id) {
    if (!span_id) return;
    ro_generate_random_bytes(span_id, RO_SPAN_ID_SIZE);
}

uint64_t ro_timestamp_ns(void) {
    return ro_get_time_ns_internal();
}

void ro_reset_metrics(ro_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->counter_count; i++) {
        ctx->counters[i].value = 0;
    }

    for (uint32_t i = 0; i < ctx->gauge_count; i++) {
        ctx->gauges[i].value = 0.0;
    }

    for (uint32_t i = 0; i < ctx->histogram_count; i++) {
        ctx->histograms[i].sample_count = 0;
        ctx->histograms[i].sample_sum = 0.0;
        ctx->histograms[i].min = INFINITY;
        ctx->histograms[i].max = -INFINITY;
        for (uint32_t b = 0; b < ctx->histograms[i].bucket_count; b++) {
            ctx->histograms[i].buckets[b].count = 0;
        }
    }

    ctx->recovery_sample_count = 0;
    ctx->recovery_sample_head = 0;
    ctx->failure_record_count = 0;
    ctx->failure_record_head = 0;

    nimcp_mutex_unlock(&ctx->mutex);

    LOG_INFO("RO", "Reset all metrics");
}

//=============================================================================
// String Conversion
//=============================================================================

const char* ro_metric_type_to_string(ro_metric_type_t type) {
    switch (type) {
        case RO_METRIC_COUNTER: return "Counter";
        case RO_METRIC_GAUGE: return "Gauge";
        case RO_METRIC_HISTOGRAM: return "Histogram";
        case RO_METRIC_SUMMARY: return "Summary";
        default: return "Unknown";
    }
}

const char* ro_span_status_to_string(ro_span_status_t status) {
    switch (status) {
        case RO_SPAN_UNSET: return "Unset";
        case RO_SPAN_OK: return "OK";
        case RO_SPAN_ERROR: return "Error";
        default: return "Unknown";
    }
}

const char* ro_severity_to_string(ro_severity_t severity) {
    switch (severity) {
        case RO_SEVERITY_TRACE: return "Trace";
        case RO_SEVERITY_DEBUG: return "Debug";
        case RO_SEVERITY_INFO: return "Info";
        case RO_SEVERITY_WARN: return "Warn";
        case RO_SEVERITY_ERROR: return "Error";
        case RO_SEVERITY_FATAL: return "Fatal";
        default: return "Unknown";
    }
}

const char* ro_event_type_to_string(ro_event_type_t type) {
    switch (type) {
        case RO_EVENT_FAILURE_DETECTED: return "FailureDetected";
        case RO_EVENT_RECOVERY_STARTED: return "RecoveryStarted";
        case RO_EVENT_RECOVERY_SUCCESS: return "RecoverySuccess";
        case RO_EVENT_RECOVERY_FAILED: return "RecoveryFailed";
        case RO_EVENT_CHECKPOINT_CREATED: return "CheckpointCreated";
        case RO_EVENT_CHECKPOINT_RESTORED: return "CheckpointRestored";
        case RO_EVENT_ESCALATION: return "Escalation";
        case RO_EVENT_DEGRADATION: return "Degradation";
        case RO_EVENT_RECOVERY_TIMEOUT: return "RecoveryTimeout";
        case RO_EVENT_RESOURCE_EXHAUSTED: return "ResourceExhausted";
        default: return "Unknown";
    }
}

const char* ro_export_format_to_string(ro_export_format_t format) {
    switch (format) {
        case RO_EXPORT_JSON: return "JSON";
        case RO_EXPORT_PROMETHEUS: return "Prometheus";
        case RO_EXPORT_OPENTELEMETRY: return "OpenTelemetry";
        case RO_EXPORT_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}
