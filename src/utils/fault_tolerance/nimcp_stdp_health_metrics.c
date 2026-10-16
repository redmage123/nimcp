/**
 * @file nimcp_stdp_health_metrics.c
 * @brief Implementation of STDP Health Metrics
 * @version 1.0.0
 * @date 2026-01-20
 */

#include "utils/fault_tolerance/nimcp_stdp_health_metrics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE "stdp_health"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(stdp_health_metrics)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Monitored STDP context entry
 */
typedef struct {
    bool active;
    stdp_context_t* stdp;
    bcm_context_t* bcm;
    char name[NIMCP_ID_BUFFER_SIZE];
    uint64_t last_check_time;

    /* Cached metrics */
    float last_mean_weight;
    float last_variance;
    uint64_t sign_changes;
} monitored_context_t;

/**
 * @brief STDP health metrics structure
 */
struct stdp_health_metrics {
    uint32_t magic;

    /* Configuration */
    stdp_health_config_t config;

    /* Health agent connection */
    nimcp_health_agent_t* health_agent;

    /* Monitored contexts */
    monitored_context_t contexts[STDP_HEALTH_MAX_CONTEXTS];
    uint32_t context_count;

    /* Plasticity coordinator */
    plasticity_coordinator_t* coordinator;

    /* Bio-async */
    bio_router_t bio_router;

    /* Callbacks */
    stdp_anomaly_callback_t anomaly_callback;
    void* anomaly_callback_data;
    stdp_health_check_callback_t check_callback;
    void* check_callback_data;

    /* Statistics */
    stdp_health_stats_t stats;

    /* Synchronization */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Anomaly Type Names
 * ============================================================================ */

static const char* anomaly_type_names[] = {
    /* Weight anomalies (0x100) */
    [STDP_ANOMALY_WEIGHT_DIVERGENCE & 0xFF] = "weight_divergence",
    [STDP_ANOMALY_WEIGHT_SATURATION & 0xFF] = "weight_saturation",
    [STDP_ANOMALY_WEIGHT_OSCILLATION & 0xFF] = "weight_oscillation",
    [STDP_ANOMALY_WEIGHT_NAN & 0xFF] = "weight_nan",
    [STDP_ANOMALY_WEIGHT_COLLAPSE & 0xFF] = "weight_collapse",
};

static const char* severity_names[] = {
    "info",
    "warning",
    "error",
    "critical"
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int check_weight_health(stdp_health_metrics_t* metrics,
                               const float* weights, size_t count,
                               const char* context_name);
static void update_health_scores(stdp_health_metrics_t* metrics);
static void report_anomaly(stdp_health_metrics_t* metrics,
                           const stdp_anomaly_report_t* report);

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int stdp_health_default_config(stdp_health_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Enable flags */
    config->enable_weight_monitoring = true;
    config->enable_timing_monitoring = true;
    config->enable_lr_monitoring = true;
    config->enable_trace_monitoring = true;
    config->enable_bcm_monitoring = true;
    config->enable_homeostatic_monitoring = true;

    /* Check frequency */
    config->check_interval_ms = STDP_HEALTH_CHECK_INTERVAL_MS;
    config->sample_window_size = 100;

    /* Weight thresholds */
    config->weight_thresholds.max_weight_value = 10.0f;
    config->weight_thresholds.min_weight_value = -10.0f;
    config->weight_thresholds.max_weight_change_rate = 1.0f;
    config->weight_thresholds.saturation_threshold = 0.9f;
    config->weight_thresholds.oscillation_threshold = 5;
    config->weight_thresholds.oscillation_window = 10;

    /* Timing thresholds */
    config->timing_thresholds.max_timing_window_ms = 100.0f;
    config->timing_thresholds.timing_jitter_threshold = 10.0f;
    config->timing_thresholds.timing_skew_threshold = 5.0f;

    /* Learning rate thresholds */
    config->lr_thresholds.max_learning_rate = 1.0f;
    config->lr_thresholds.min_learning_rate = 1e-8f;
    config->lr_thresholds.lr_change_threshold = 0.5f;

    /* Trace thresholds */
    config->trace_thresholds.max_trace_value = 10.0f;
    config->trace_thresholds.trace_decay_min = 0.9f;
    config->trace_thresholds.trace_accumulation_limit = 100.0f;

    /* BCM thresholds */
    config->bcm_thresholds.threshold_drift_limit = 0.5f;
    config->bcm_thresholds.target_activity_tolerance = 0.2f;

    /* Homeostatic thresholds */
    config->homeostatic_thresholds.activity_min = 0.01f;
    config->homeostatic_thresholds.activity_max = 0.9f;
    config->homeostatic_thresholds.scaling_rate_limit = 0.1f;

    /* Response configuration */
    config->auto_pause_on_critical = true;
    config->auto_reset_on_divergence = false;
    config->notify_health_agent = true;

    /* Bio-async */
    config->enable_bio_async = false;

    /* Logging */
    config->verbose_logging = false;

    return 0;
}

stdp_health_metrics_t* stdp_health_create(
    const stdp_health_config_t* config,
    nimcp_health_agent_t* health_agent
) {
    stdp_health_metrics_t* metrics = nimcp_calloc(1, sizeof(*metrics));
    if (!metrics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metrics is NULL");

        return NULL;
    }

    metrics->magic = STDP_HEALTH_METRICS_MAGIC;

    /* Apply configuration */
    if (config) {
        metrics->config = *config;
    } else {
        stdp_health_default_config(&metrics->config);
    }

    /* Store health agent */
    metrics->health_agent = health_agent;

    /* Create mutex */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    metrics->mutex = nimcp_mutex_create(&attr);
    if (!metrics->mutex) {
        nimcp_free(metrics);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stdp_health_create: metrics->mutex is NULL");
        return NULL;
    }

    /* Initialize health scores */
    metrics->stats.weight_health_score = 1.0f;
    metrics->stats.timing_health_score = 1.0f;
    metrics->stats.lr_health_score = 1.0f;
    metrics->stats.overall_health_score = 1.0f;

    return metrics;
}

void stdp_health_destroy(stdp_health_metrics_t* metrics) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        return;
    }

    if (metrics->mutex) {
        nimcp_mutex_destroy(metrics->mutex);
    }

    metrics->magic = 0;
    nimcp_free(metrics);
}

/* ============================================================================
 * Registration Implementation
 * ============================================================================ */

int stdp_health_register_stdp(
    stdp_health_metrics_t* metrics,
    stdp_context_t* stdp,
    const char* name
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC || !stdp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_health_register_stdp: required parameter is NULL (metrics, stdp)");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);

    if (metrics->context_count >= STDP_HEALTH_MAX_CONTEXTS) {
        nimcp_mutex_unlock(metrics->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "stdp_health_register_stdp: capacity exceeded");
        return -1;
    }

    /* Find free slot */
    int context_id = -1;
    for (int i = 0; i < STDP_HEALTH_MAX_CONTEXTS; i++) {
        if (!metrics->contexts[i].active) {
            context_id = i;
            break;
        }
    }

    if (context_id < 0) {
        nimcp_mutex_unlock(metrics->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_register_stdp: validation failed");
        return -1;
    }

    monitored_context_t* ctx = &metrics->contexts[context_id];
    ctx->active = true;
    ctx->stdp = stdp;
    ctx->bcm = NULL;
    if (name) {
        strncpy(ctx->name, name, sizeof(ctx->name) - 1);
    } else {
        snprintf(ctx->name, sizeof(ctx->name), "stdp_%d", context_id);
    }

    metrics->context_count++;
    metrics->stats.contexts_monitored = metrics->context_count;

    nimcp_mutex_unlock(metrics->mutex);

    return context_id;
}

int stdp_health_register_bcm(
    stdp_health_metrics_t* metrics,
    bcm_context_t* bcm,
    const char* name
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC || !bcm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_health_register_bcm: required parameter is NULL (metrics, bcm)");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);

    if (metrics->context_count >= STDP_HEALTH_MAX_CONTEXTS) {
        nimcp_mutex_unlock(metrics->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "stdp_health_register_bcm: capacity exceeded");
        return -1;
    }

    /* Find free slot */
    int context_id = -1;
    for (int i = 0; i < STDP_HEALTH_MAX_CONTEXTS; i++) {
        if (!metrics->contexts[i].active) {
            context_id = i;
            break;
        }
    }

    if (context_id < 0) {
        nimcp_mutex_unlock(metrics->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_register_bcm: validation failed");
        return -1;
    }

    monitored_context_t* ctx = &metrics->contexts[context_id];
    ctx->active = true;
    ctx->stdp = NULL;
    ctx->bcm = bcm;
    if (name) {
        strncpy(ctx->name, name, sizeof(ctx->name) - 1);
    } else {
        snprintf(ctx->name, sizeof(ctx->name), "bcm_%d", context_id);
    }

    metrics->context_count++;
    metrics->stats.contexts_monitored = metrics->context_count;

    nimcp_mutex_unlock(metrics->mutex);

    return context_id;
}

int stdp_health_register_coordinator(
    stdp_health_metrics_t* metrics,
    plasticity_coordinator_t* coordinator
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_register_coordinator: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);
    metrics->coordinator = coordinator;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int stdp_health_unregister(
    stdp_health_metrics_t* metrics,
    int context_id
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_unregister: metrics is NULL");
        return -1;
    }

    if (context_id < 0 || context_id >= STDP_HEALTH_MAX_CONTEXTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "stdp_health_unregister: capacity exceeded");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);

    if (metrics->contexts[context_id].active) {
        metrics->contexts[context_id].active = false;
        metrics->contexts[context_id].stdp = NULL;
        metrics->contexts[context_id].bcm = NULL;
        metrics->context_count--;
        metrics->stats.contexts_monitored = metrics->context_count;
    }

    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

/* ============================================================================
 * Monitoring Implementation
 * ============================================================================ */

int stdp_health_check(stdp_health_metrics_t* metrics) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_check: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);

    int anomalies = 0;
    uint64_t now = nimcp_time_get_us();

    metrics->stats.checks_performed++;
    metrics->stats.last_check_time_us = now;

    /* Check each registered context */
    for (int i = 0; i < STDP_HEALTH_MAX_CONTEXTS; i++) {
        if (!metrics->contexts[i].active) {
            continue;
        }

        /* Context-specific checks would go here */
        /* For now, just update timing */
        metrics->contexts[i].last_check_time = now;
    }

    /* Update health scores */
    update_health_scores(metrics);

    /* Invoke check callback */
    if (metrics->check_callback) {
        nimcp_mutex_unlock(metrics->mutex);
        metrics->check_callback(&metrics->stats, metrics->check_callback_data);
        nimcp_mutex_lock(metrics->mutex);
    }

    nimcp_mutex_unlock(metrics->mutex);

    return anomalies;
}

int stdp_health_check_weights(
    stdp_health_metrics_t* metrics,
    const float* weights,
    size_t count,
    const char* context_name
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC || !weights || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_check_weights: required parameter is NULL (metrics, weights)");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);
    int anomalies = check_weight_health(metrics, weights, count, context_name);
    nimcp_mutex_unlock(metrics->mutex);

    return anomalies;
}

int stdp_health_check_timing(
    stdp_health_metrics_t* metrics,
    const float* pre_times,
    const float* post_times,
    size_t count
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC ||
        !pre_times || !post_times || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_check_timing: operation failed");
        return -1;
    }

    if (!metrics->config.enable_timing_monitoring) {
        return 0;
    }

    nimcp_mutex_lock(metrics->mutex);

    int violations = 0;
    float sum_delta = 0.0f;
    float sum_sq = 0.0f;

    for (size_t i = 0; i < count; i++) {
        float delta = post_times[i] - pre_times[i];
        sum_delta += delta;
        sum_sq += delta * delta;

        /* Check window violation */
        if (fabsf(delta) > metrics->config.timing_thresholds.max_timing_window_ms) {
            violations++;
        }
    }

    /* Update timing statistics */
    float mean = sum_delta / count;
    float variance = (sum_sq / count) - (mean * mean);
    float std_dev = sqrtf(fmaxf(0.0f, variance));

    metrics->stats.timing_stats.mean_timing_ms = mean;
    metrics->stats.timing_stats.timing_std_dev = std_dev;
    metrics->stats.timing_stats.timing_skew = mean;
    metrics->stats.timing_stats.violations += violations;

    /* Check for anomalies */
    if (std_dev > metrics->config.timing_thresholds.timing_jitter_threshold) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_TIMING_JITTER;
        report.severity = STDP_SEVERITY_WARNING;
        report.current_value = std_dev;
        report.threshold_value = metrics->config.timing_thresholds.timing_jitter_threshold;
        snprintf(report.description, sizeof(report.description),
                 "Timing jitter (%.2f ms) exceeds threshold", std_dev);
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        violations++;
    }

    nimcp_mutex_unlock(metrics->mutex);

    return violations;
}

int stdp_health_check_learning_rate(
    stdp_health_metrics_t* metrics,
    float learning_rate,
    const char* context_name
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_check_learning_rate: metrics is NULL");
        return -1;
    }

    if (!metrics->config.enable_lr_monitoring) {
        return 0;
    }

    nimcp_mutex_lock(metrics->mutex);

    int anomaly = 0;

    /* Check bounds */
    if (learning_rate > metrics->config.lr_thresholds.max_learning_rate) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_LR_EXPLOSION;
        report.severity = STDP_SEVERITY_ERROR;
        report.current_value = learning_rate;
        report.threshold_value = metrics->config.lr_thresholds.max_learning_rate;
        snprintf(report.description, sizeof(report.description),
                 "Learning rate %.6f exceeds max in %s",
                 learning_rate, context_name ? context_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        anomaly = STDP_ANOMALY_LR_EXPLOSION;
    } else if (learning_rate < metrics->config.lr_thresholds.min_learning_rate) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_LR_COLLAPSE;
        report.severity = STDP_SEVERITY_WARNING;
        report.current_value = learning_rate;
        report.threshold_value = metrics->config.lr_thresholds.min_learning_rate;
        snprintf(report.description, sizeof(report.description),
                 "Learning rate %.6f below min in %s",
                 learning_rate, context_name ? context_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        anomaly = STDP_ANOMALY_LR_COLLAPSE;
    }

    /* Update statistics */
    metrics->stats.learning_stats.current_lr = learning_rate;
    metrics->stats.learning_stats.lr_changes++;

    nimcp_mutex_unlock(metrics->mutex);

    return anomaly;
}

int stdp_health_check_traces(
    stdp_health_metrics_t* metrics,
    const float* traces,
    size_t count
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC ||
        !traces || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_check_traces: operation failed");
        return -1;
    }

    if (!metrics->config.enable_trace_monitoring) {
        return 0;
    }

    nimcp_mutex_lock(metrics->mutex);

    int anomalies = 0;

    for (size_t i = 0; i < count; i++) {
        if (traces[i] > metrics->config.trace_thresholds.max_trace_value) {
            anomalies++;
        }
        if (isnan(traces[i]) || isinf(traces[i])) {
            anomalies++;
        }
    }

    if (anomalies > 0) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_TRACE_OVERFLOW;
        report.severity = STDP_SEVERITY_WARNING;
        report.current_value = (float)anomalies;
        snprintf(report.description, sizeof(report.description),
                 "%d eligibility traces exceeded threshold", anomalies);
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
    }

    nimcp_mutex_unlock(metrics->mutex);

    return anomalies;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int stdp_health_set_anomaly_callback(
    stdp_health_metrics_t* metrics,
    stdp_anomaly_callback_t callback,
    void* user_data
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_set_anomaly_callback: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);
    metrics->anomaly_callback = callback;
    metrics->anomaly_callback_data = user_data;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int stdp_health_set_check_callback(
    stdp_health_metrics_t* metrics,
    stdp_health_check_callback_t callback,
    void* user_data
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_set_check_callback: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);
    metrics->check_callback = callback;
    metrics->check_callback_data = user_data;
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int stdp_health_get_stats(
    const stdp_health_metrics_t* metrics,
    stdp_health_stats_t* stats
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stdp_health_get_stats: required parameter is NULL (metrics, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)metrics->mutex);
    *stats = metrics->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)metrics->mutex);

    return 0;
}

float stdp_health_get_score(const stdp_health_metrics_t* metrics) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        return -1.0f;
    }
    return metrics->stats.overall_health_score;
}

void stdp_health_reset_stats(stdp_health_metrics_t* metrics) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        return;
    }

    nimcp_mutex_lock(metrics->mutex);

    uint64_t contexts = metrics->stats.contexts_monitored;
    memset(&metrics->stats, 0, sizeof(metrics->stats));
    metrics->stats.contexts_monitored = contexts;
    metrics->stats.weight_health_score = 1.0f;
    metrics->stats.timing_health_score = 1.0f;
    metrics->stats.lr_health_score = 1.0f;
    metrics->stats.overall_health_score = 1.0f;

    nimcp_mutex_unlock(metrics->mutex);
}

bool stdp_health_is_healthy(const stdp_health_metrics_t* metrics) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        return false;
    }
    return metrics->stats.overall_health_score > 0.5f &&
           metrics->stats.critical_count == 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int stdp_health_connect_bio_async(
    stdp_health_metrics_t* metrics,
    bio_router_t router
) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_connect_bio_async: metrics is NULL");
        return -1;
    }

    nimcp_mutex_lock(metrics->mutex);
    metrics->bio_router = router;
    metrics->config.enable_bio_async = (router != NULL);
    nimcp_mutex_unlock(metrics->mutex);

    return 0;
}

int stdp_health_broadcast_status(stdp_health_metrics_t* metrics) {
    if (!metrics || metrics->magic != STDP_HEALTH_METRICS_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "stdp_health_broadcast_status: metrics is NULL");
        return -1;
    }

    if (!metrics->bio_router || !metrics->config.enable_bio_async) {
        return 0;
    }

    /* Broadcast would go here if bio-async is connected */
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* stdp_anomaly_type_name(stdp_anomaly_type_t type) {
    uint32_t base = type & 0xFF0;
    uint32_t idx = type & 0x00F;

    if (base == 0x100 && idx < 5) {
        return anomaly_type_names[idx];
    }

    switch (type) {
        case STDP_ANOMALY_TIMING_VIOLATION: return "timing_violation";
        case STDP_ANOMALY_TIMING_SKEW: return "timing_skew";
        case STDP_ANOMALY_TIMING_JITTER: return "timing_jitter";
        case STDP_ANOMALY_LR_EXPLOSION: return "lr_explosion";
        case STDP_ANOMALY_LR_COLLAPSE: return "lr_collapse";
        case STDP_ANOMALY_LR_OSCILLATION: return "lr_oscillation";
        case STDP_ANOMALY_TRACE_OVERFLOW: return "trace_overflow";
        case STDP_ANOMALY_TRACE_DECAY_FAILURE: return "trace_decay_failure";
        case STDP_ANOMALY_TRACE_ACCUMULATION: return "trace_accumulation";
        case STDP_ANOMALY_BCM_THRESHOLD_DRIFT: return "bcm_threshold_drift";
        case STDP_ANOMALY_BCM_RUNAWAY: return "bcm_runaway";
        case STDP_ANOMALY_HOMEOSTATIC_FAILURE: return "homeostatic_failure";
        case STDP_ANOMALY_ACTIVITY_IMBALANCE: return "activity_imbalance";
        case STDP_ANOMALY_PLASTICITY_FROZEN: return "plasticity_frozen";
        case STDP_ANOMALY_PLASTICITY_RUNAWAY: return "plasticity_runaway";
        case STDP_ANOMALY_SYNAPSE_DEATH: return "synapse_death";
        default: return "unknown";
    }
}

const char* stdp_anomaly_severity_name(stdp_anomaly_severity_t severity) {
    if (severity > STDP_SEVERITY_CRITICAL) {
        return "unknown";
    }
    return severity_names[severity];
}

const char* stdp_health_metrics_version(void) {
    return STDP_HEALTH_METRICS_VERSION;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static int check_weight_health(stdp_health_metrics_t* metrics,
                               const float* weights, size_t count,
                               const char* context_name) {
    if (!metrics->config.enable_weight_monitoring) {
        return 0;
    }

    int anomalies = 0;
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float max_w = -FLT_MAX;
    float min_w = FLT_MAX;
    uint64_t nan_count = 0;
    uint64_t saturated = 0;

    for (size_t i = 0; i < count; i++) {
        float w = weights[i];

        if (isnan(w) || isinf(w)) {
            nan_count++;
            continue;
        }

        sum += w;
        sum_sq += w * w;
        if (w > max_w) max_w = w;
        if (w < min_w) min_w = w;

        /* Check saturation */
        if (w >= metrics->config.weight_thresholds.max_weight_value * 0.95f ||
            w <= metrics->config.weight_thresholds.min_weight_value * 0.95f) {
            saturated++;
        }
    }

    size_t valid_count = count - nan_count;
    if (valid_count == 0) {
        /* All NaN - critical! */
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_WEIGHT_NAN;
        report.severity = STDP_SEVERITY_CRITICAL;
        report.current_value = (float)nan_count;
        snprintf(report.description, sizeof(report.description),
                 "All %zu weights are NaN in %s", count,
                 context_name ? context_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        return 1;
    }

    float mean = sum / valid_count;
    float variance = (sum_sq / valid_count) - (mean * mean);

    /* Update statistics */
    metrics->stats.weight_stats.mean_weight = mean;
    metrics->stats.weight_stats.weight_variance = variance;
    metrics->stats.weight_stats.max_weight = max_w;
    metrics->stats.weight_stats.min_weight = min_w;
    metrics->stats.weight_stats.nan_count += nan_count;
    metrics->stats.weight_stats.saturation_ratio = (float)saturated / count;

    /* Check for NaN anomaly */
    if (nan_count > 0) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_WEIGHT_NAN;
        report.severity = STDP_SEVERITY_ERROR;
        report.current_value = (float)nan_count;
        snprintf(report.description, sizeof(report.description),
                 "%llu NaN weights in %s", (unsigned long long)nan_count,
                 context_name ? context_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        anomalies++;
    }

    /* Check for saturation */
    float sat_ratio = (float)saturated / count;
    if (sat_ratio > metrics->config.weight_thresholds.saturation_threshold) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_WEIGHT_SATURATION;
        report.severity = STDP_SEVERITY_WARNING;
        report.current_value = sat_ratio;
        report.threshold_value = metrics->config.weight_thresholds.saturation_threshold;
        snprintf(report.description, sizeof(report.description),
                 "%.1f%% weights saturated in %s", sat_ratio * 100,
                 context_name ? context_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        anomalies++;
    }

    /* Check for divergence */
    if (max_w > metrics->config.weight_thresholds.max_weight_value ||
        min_w < metrics->config.weight_thresholds.min_weight_value) {
        stdp_anomaly_report_t report = {0};
        report.type = STDP_ANOMALY_WEIGHT_DIVERGENCE;
        report.severity = STDP_SEVERITY_ERROR;
        report.current_value = fmaxf(fabsf(max_w), fabsf(min_w));
        snprintf(report.description, sizeof(report.description),
                 "Weights diverging [%.2f, %.2f] in %s", min_w, max_w,
                 context_name ? context_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        report_anomaly(metrics, &report);
        anomalies++;
    }

    return anomalies;
}

static void update_health_scores(stdp_health_metrics_t* metrics) {
    /* Weight health score */
    float weight_score = 1.0f;
    if (metrics->stats.weight_stats.nan_count > 0) {
        weight_score -= 0.5f;
    }
    if (metrics->stats.weight_stats.saturation_ratio > 0.5f) {
        weight_score -= 0.3f;
    }
    metrics->stats.weight_health_score = fmaxf(0.0f, weight_score);

    /* Timing health score */
    float timing_score = 1.0f;
    if (metrics->stats.timing_stats.violations > 10) {
        timing_score -= 0.3f;
    }
    if (metrics->stats.timing_stats.timing_std_dev >
        metrics->config.timing_thresholds.timing_jitter_threshold) {
        timing_score -= 0.2f;
    }
    metrics->stats.timing_health_score = fmaxf(0.0f, timing_score);

    /* Learning rate health score */
    float lr_score = 1.0f;
    float lr = metrics->stats.learning_stats.current_lr;
    if (lr > metrics->config.lr_thresholds.max_learning_rate ||
        lr < metrics->config.lr_thresholds.min_learning_rate) {
        lr_score -= 0.4f;
    }
    metrics->stats.lr_health_score = fmaxf(0.0f, lr_score);

    /* Overall score */
    metrics->stats.overall_health_score =
        (metrics->stats.weight_health_score +
         metrics->stats.timing_health_score +
         metrics->stats.lr_health_score) / 3.0f;
}

static void report_anomaly(stdp_health_metrics_t* metrics,
                           const stdp_anomaly_report_t* report) {
    /* Update statistics */
    metrics->stats.anomalies_detected++;

    if (report->severity == STDP_SEVERITY_CRITICAL) {
        metrics->stats.critical_count++;
    } else if (report->severity >= STDP_SEVERITY_WARNING) {
        metrics->stats.warning_count++;
    }

    /* Invoke callback */
    if (metrics->anomaly_callback) {
        metrics->anomaly_callback(report, metrics->anomaly_callback_data);
    }

    /* Log if verbose */
    if (metrics->config.verbose_logging) {
        /* Would log here */
    }
}
