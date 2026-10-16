/**
 * @file nimcp_plasticity_anomaly_detection.c
 * @brief Implementation of Plasticity Anomaly Detection
 * @version 1.0.0
 * @date 2026-01-20
 */

#include "utils/fault_tolerance/nimcp_plasticity_anomaly_detection.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_MODULE "plasticity_anomaly"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(plasticity_anomaly_detection)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Metric history entry
 */
typedef struct {
    float value;
    uint64_t timestamp;
} metric_sample_t;

/**
 * @brief Metric tracking
 */
typedef struct {
    char name[NIMCP_ID_BUFFER_SIZE];
    plasticity_anomaly_category_t category;
    metric_sample_t* history;
    uint32_t history_size;
    uint32_t history_index;
    uint32_t sample_count;

    /* Statistics */
    float baseline_mean;
    float baseline_std;
    bool baseline_established;
} metric_tracker_t;

/**
 * @brief Anomaly history entry
 */
typedef struct {
    plasticity_anomaly_report_t report;
    uint64_t first_seen;
    uint64_t last_seen;
    uint32_t count;
} anomaly_entry_t;

/**
 * @brief Plasticity anomaly detector structure
 */
struct plasticity_anomaly_detector {
    uint32_t magic;

    /* Configuration */
    plasticity_anomaly_config_t config;

    /* Health monitor connection */
    health_monitor_t* health_monitor;

    /* Rules */
    plasticity_detection_rule_t* rules;
    uint32_t rule_count;
    uint32_t rule_capacity;

    /* Metric tracking */
    metric_tracker_t* trackers;
    uint32_t tracker_count;
    uint32_t tracker_capacity;

    /* Anomaly history */
    anomaly_entry_t* anomaly_history;
    uint32_t history_count;
    uint32_t history_capacity;

    /* Bio-async */
    bio_router_t bio_router;

    /* Callbacks */
    plasticity_anomaly_cb_t anomaly_callback;
    void* anomaly_callback_data;
    plasticity_health_cb_t health_callback;
    void* health_callback_data;

    /* Statistics */
    plasticity_detection_stats_t stats;

    /* Synchronization */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Name Tables
 * ============================================================================ */

static const char* category_names[] = {
    "weight",
    "timing",
    "bcm",
    "homeostatic",
    "learning",
    "structural"
};

static const char* severity_names[] = {
    "info",
    "warning",
    "error",
    "critical"
};

static const char* action_names[] = {
    "none",
    "log",
    "alert",
    "reduce_lr",
    "pause_learning",
    "reset_weights",
    "quarantine",
    "notify_health"
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static int check_rule(plasticity_anomaly_detector_t* detector,
                      plasticity_detection_rule_t* rule,
                      float value);
static void record_anomaly(plasticity_anomaly_detector_t* detector,
                           const plasticity_anomaly_report_t* report);
static float compute_health_score(const plasticity_anomaly_detector_t* detector);
static metric_tracker_t* find_or_create_tracker(
    plasticity_anomaly_detector_t* detector,
    const char* name,
    plasticity_anomaly_category_t category);

/* ============================================================================
 * Default Rules
 * ============================================================================ */

static const plasticity_detection_rule_t default_weight_rules[] = {
    {
        .rule_id = 1,
        .name = "weight_explosion",
        .enabled = true,
        .type = PLASTICITY_RULE_THRESHOLD,
        .anomaly = PLASTICITY_ANOMALY_WEIGHT_EXPLOSION,
        .category = PLASTICITY_CATEGORY_WEIGHT,
        .severity = PLASTICITY_SEVERITY_ERROR,
        .params.threshold = { .upper_threshold = 100.0f, .check_upper = true },
        .action = PLASTICITY_ACTION_REDUCE_LR,
        .cooldown_ms = 5000
    },
    {
        .rule_id = 2,
        .name = "weight_vanishing",
        .enabled = true,
        .type = PLASTICITY_RULE_THRESHOLD,
        .anomaly = PLASTICITY_ANOMALY_WEIGHT_VANISHING,
        .category = PLASTICITY_CATEGORY_WEIGHT,
        .severity = PLASTICITY_SEVERITY_WARNING,
        .params.threshold = { .lower_threshold = 1e-6f, .check_lower = true },
        .action = PLASTICITY_ACTION_LOG,
        .cooldown_ms = 10000
    },
    {
        .rule_id = 3,
        .name = "weight_nan",
        .enabled = true,
        .type = PLASTICITY_RULE_THRESHOLD,
        .anomaly = PLASTICITY_ANOMALY_WEIGHT_GRADIENT_NAN,
        .category = PLASTICITY_CATEGORY_WEIGHT,
        .severity = PLASTICITY_SEVERITY_CRITICAL,
        .params.threshold = { .upper_threshold = 0.0f, .check_upper = true },
        .action = PLASTICITY_ACTION_PAUSE_LEARNING,
        .cooldown_ms = 1000
    }
};

static const plasticity_detection_rule_t default_bcm_rules[] = {
    {
        .rule_id = 10,
        .name = "bcm_threshold_high",
        .enabled = true,
        .type = PLASTICITY_RULE_THRESHOLD,
        .anomaly = PLASTICITY_ANOMALY_BCM_THRESHOLD_HIGH,
        .category = PLASTICITY_CATEGORY_BCM,
        .severity = PLASTICITY_SEVERITY_WARNING,
        .params.threshold = { .upper_threshold = 1.0f, .check_upper = true },
        .action = PLASTICITY_ACTION_ALERT,
        .cooldown_ms = 5000
    },
    {
        .rule_id = 11,
        .name = "bcm_threshold_drift",
        .enabled = true,
        .type = PLASTICITY_RULE_RATE_OF_CHANGE,
        .anomaly = PLASTICITY_ANOMALY_BCM_THRESHOLD_DRIFT,
        .category = PLASTICITY_CATEGORY_BCM,
        .severity = PLASTICITY_SEVERITY_WARNING,
        .params.rate = { .max_rate = 0.1f, .time_window_ms = 1000 },
        .action = PLASTICITY_ACTION_LOG,
        .cooldown_ms = 5000
    }
};

static const plasticity_detection_rule_t default_homeostatic_rules[] = {
    {
        .rule_id = 20,
        .name = "activity_too_high",
        .enabled = true,
        .type = PLASTICITY_RULE_THRESHOLD,
        .anomaly = PLASTICITY_ANOMALY_ACTIVITY_TOO_HIGH,
        .category = PLASTICITY_CATEGORY_HOMEOSTATIC,
        .severity = PLASTICITY_SEVERITY_WARNING,
        .params.threshold = { .upper_threshold = 0.9f, .check_upper = true },
        .action = PLASTICITY_ACTION_ALERT,
        .cooldown_ms = 5000
    },
    {
        .rule_id = 21,
        .name = "activity_too_low",
        .enabled = true,
        .type = PLASTICITY_RULE_THRESHOLD,
        .anomaly = PLASTICITY_ANOMALY_ACTIVITY_TOO_LOW,
        .category = PLASTICITY_CATEGORY_HOMEOSTATIC,
        .severity = PLASTICITY_SEVERITY_WARNING,
        .params.threshold = { .lower_threshold = 0.01f, .check_lower = true },
        .action = PLASTICITY_ACTION_ALERT,
        .cooldown_ms = 5000
    }
};

static const plasticity_detection_rule_t default_learning_rules[] = {
    {
        .rule_id = 30,
        .name = "learning_stalled",
        .enabled = true,
        .type = PLASTICITY_RULE_RATE_OF_CHANGE,
        .anomaly = PLASTICITY_ANOMALY_LEARNING_STALLED,
        .category = PLASTICITY_CATEGORY_LEARNING,
        .severity = PLASTICITY_SEVERITY_WARNING,
        .params.rate = { .min_rate = 1e-8f, .time_window_ms = 10000 },
        .action = PLASTICITY_ACTION_LOG,
        .cooldown_ms = 30000
    },
    {
        .rule_id = 31,
        .name = "learning_diverging",
        .enabled = true,
        .type = PLASTICITY_RULE_TREND,
        .anomaly = PLASTICITY_ANOMALY_LEARNING_DIVERGING,
        .category = PLASTICITY_CATEGORY_LEARNING,
        .severity = PLASTICITY_SEVERITY_ERROR,
        .params.trend = { .slope_threshold = 0.1f, .window_size = 10, .detect_increasing = true },
        .action = PLASTICITY_ACTION_REDUCE_LR,
        .cooldown_ms = 5000
    }
};

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int plasticity_anomaly_default_config(plasticity_anomaly_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->enabled = true;
    config->check_interval_ms = PLASTICITY_DETECTION_WINDOW_MS;
    config->history_size = PLASTICITY_METRIC_HISTORY_SIZE;

    /* Enable all categories */
    config->detect_weight_anomalies = true;
    config->detect_timing_anomalies = true;
    config->detect_bcm_anomalies = true;
    config->detect_homeostatic_anomalies = true;
    config->detect_learning_anomalies = true;
    config->detect_structural_anomalies = true;

    /* Sensitivity */
    config->sensitivity = 0.7f;
    config->false_positive_tolerance = 0.05f;

    /* Response */
    config->auto_respond = true;
    config->escalate_to_health_agent = true;
    config->min_report_severity = PLASTICITY_SEVERITY_WARNING;

    /* Bio-async */
    config->enable_bio_async = false;

    /* Logging */
    config->verbose_logging = false;

    return 0;
}

plasticity_anomaly_detector_t* plasticity_anomaly_create(
    const plasticity_anomaly_config_t* config,
    health_monitor_t* health_monitor
) {
    plasticity_anomaly_detector_t* detector = nimcp_calloc(1, sizeof(*detector));
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "detector is NULL");

        return NULL;
    }

    detector->magic = PLASTICITY_ANOMALY_MAGIC;

    /* Apply configuration */
    if (config) {
        detector->config = *config;
    } else {
        plasticity_anomaly_default_config(&detector->config);
    }

    /* Store health monitor */
    detector->health_monitor = health_monitor;

    /* Create mutex */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    detector->mutex = nimcp_mutex_create(&attr);
    if (!detector->mutex) {
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_anomaly_create: detector->mutex is NULL");
        return NULL;
    }

    /* Allocate rules */
    detector->rule_capacity = PLASTICITY_MAX_RULES;
    detector->rules = nimcp_calloc(detector->rule_capacity,
                                   sizeof(plasticity_detection_rule_t));
    if (!detector->rules) {
        nimcp_mutex_destroy(detector->mutex);
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_anomaly_create: detector->rules is NULL");
        return NULL;
    }

    /* Allocate trackers */
    detector->tracker_capacity = 32;
    detector->trackers = nimcp_calloc(detector->tracker_capacity, sizeof(metric_tracker_t));
    if (!detector->trackers) {
        nimcp_free(detector->rules);
        nimcp_mutex_destroy(detector->mutex);
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_anomaly_create: detector->trackers is NULL");
        return NULL;
    }

    /* Allocate history */
    detector->history_capacity = 100;
    detector->anomaly_history = nimcp_calloc(detector->history_capacity,
                                             sizeof(anomaly_entry_t));
    if (!detector->anomaly_history) {
        nimcp_free(detector->trackers);
        nimcp_free(detector->rules);
        nimcp_mutex_destroy(detector->mutex);
        nimcp_free(detector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plasticity_anomaly_create: detector->anomaly_history is NULL");
        return NULL;
    }

    /* Initialize health */
    detector->stats.current_health_score = 1.0f;

    return detector;
}

void plasticity_anomaly_destroy(plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        return;
    }

    /* Free tracker histories */
    for (uint32_t i = 0; i < detector->tracker_count; i++) {
        if (detector->trackers[i].history) {
            nimcp_free(detector->trackers[i].history);
        }
    }

    if (detector->anomaly_history) {
        nimcp_free(detector->anomaly_history);
    }

    if (detector->trackers) {
        nimcp_free(detector->trackers);
    }

    if (detector->rules) {
        nimcp_free(detector->rules);
    }

    if (detector->mutex) {
        nimcp_mutex_destroy(detector->mutex);
    }

    detector->magic = 0;
    nimcp_free(detector);
}

/* ============================================================================
 * Rule Management Implementation
 * ============================================================================ */

int plasticity_anomaly_add_rule(
    plasticity_anomaly_detector_t* detector,
    const plasticity_detection_rule_t* rule
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC || !rule) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_anomaly_add_rule: required parameter is NULL (detector, rule)");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    if (detector->rule_count >= detector->rule_capacity) {
        nimcp_mutex_unlock(detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "plasticity_anomaly_add_rule: capacity exceeded");
        return -1;
    }

    detector->rules[detector->rule_count] = *rule;
    detector->rule_count++;
    detector->stats.rules_active++;

    nimcp_mutex_unlock(detector->mutex);

    return rule->rule_id;
}

int plasticity_anomaly_remove_rule(
    plasticity_anomaly_detector_t* detector,
    uint32_t rule_id
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_remove_rule: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    for (uint32_t i = 0; i < detector->rule_count; i++) {
        if (detector->rules[i].rule_id == rule_id) {
            /* Shift remaining rules */
            memmove(&detector->rules[i], &detector->rules[i + 1],
                    (detector->rule_count - i - 1) * sizeof(plasticity_detection_rule_t));
            detector->rule_count--;
            detector->stats.rules_active--;
            nimcp_mutex_unlock(detector->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_remove_rule: operation failed");
    return -1;
}

int plasticity_anomaly_set_rule_enabled(
    plasticity_anomaly_detector_t* detector,
    uint32_t rule_id,
    bool enabled
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_set_rule_enabled: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    for (uint32_t i = 0; i < detector->rule_count; i++) {
        if (detector->rules[i].rule_id == rule_id) {
            detector->rules[i].enabled = enabled;
            nimcp_mutex_unlock(detector->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_set_rule_enabled: validation failed");
    return -1;
}

int plasticity_anomaly_load_default_rules(
    plasticity_anomaly_detector_t* detector,
    plasticity_anomaly_category_t category
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_load_default_rules: detector is NULL");
        return -1;
    }

    int loaded = 0;

    switch (category) {
        case PLASTICITY_CATEGORY_WEIGHT:
            for (size_t i = 0; i < sizeof(default_weight_rules) / sizeof(default_weight_rules[0]); i++) {
                if (plasticity_anomaly_add_rule(detector, &default_weight_rules[i]) >= 0) {
                    loaded++;
                }
            }
            break;

        case PLASTICITY_CATEGORY_BCM:
            for (size_t i = 0; i < sizeof(default_bcm_rules) / sizeof(default_bcm_rules[0]); i++) {
                if (plasticity_anomaly_add_rule(detector, &default_bcm_rules[i]) >= 0) {
                    loaded++;
                }
            }
            break;

        case PLASTICITY_CATEGORY_HOMEOSTATIC:
            for (size_t i = 0; i < sizeof(default_homeostatic_rules) / sizeof(default_homeostatic_rules[0]); i++) {
                if (plasticity_anomaly_add_rule(detector, &default_homeostatic_rules[i]) >= 0) {
                    loaded++;
                }
            }
            break;

        case PLASTICITY_CATEGORY_LEARNING:
            for (size_t i = 0; i < sizeof(default_learning_rules) / sizeof(default_learning_rules[0]); i++) {
                if (plasticity_anomaly_add_rule(detector, &default_learning_rules[i]) >= 0) {
                    loaded++;
                }
            }
            break;

        default:
            break;
    }

    return loaded;
}

int plasticity_anomaly_load_all_default_rules(plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_load_all_default_rules: detector is NULL");
        return -1;
    }

    int total = 0;

    for (int cat = 0; cat < PLASTICITY_CATEGORY_COUNT; cat++) {
        int loaded = plasticity_anomaly_load_default_rules(detector, cat);
        if (loaded > 0) {
            total += loaded;
        }
    }

    return total;
}

/* ============================================================================
 * Detection Implementation
 * ============================================================================ */

int plasticity_anomaly_detect(plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_detect: detector is NULL");
        return -1;
    }

    if (!detector->config.enabled) {
        return 0;
    }

    nimcp_mutex_lock(detector->mutex);

    int anomalies = 0;
    uint64_t now = nimcp_time_get_us();

    detector->stats.checks_performed++;
    detector->stats.last_check_time_us = now;

    /* Check each tracker against rules */
    for (uint32_t t = 0; t < detector->tracker_count; t++) {
        metric_tracker_t* tracker = &detector->trackers[t];

        if (tracker->sample_count == 0) {
            continue;
        }

        /* Get latest value */
        uint32_t latest_idx = (tracker->history_index + tracker->history_size - 1) % tracker->history_size;
        float value = tracker->history[latest_idx].value;

        /* Check against rules in this category */
        for (uint32_t r = 0; r < detector->rule_count; r++) {
            plasticity_detection_rule_t* rule = &detector->rules[r];

            if (!rule->enabled || rule->category != tracker->category) {
                continue;
            }

            /* Check cooldown */
            if (now - rule->last_triggered < rule->cooldown_ms * 1000) {
                continue;
            }

            if (check_rule(detector, rule, value) > 0) {
                anomalies++;
                rule->trigger_count++;
                rule->last_triggered = now;
                detector->stats.rules_triggered++;
            }
        }
    }

    /* Update health score */
    detector->stats.current_health_score = compute_health_score(detector);

    /* Invoke health callback */
    if (detector->health_callback) {
        nimcp_mutex_unlock(detector->mutex);
        detector->health_callback(detector->stats.current_health_score,
                                  detector->health_callback_data);
        nimcp_mutex_lock(detector->mutex);
    }

    nimcp_mutex_unlock(detector->mutex);

    return anomalies;
}

/* Internal helper - must be called with mutex held */
static int submit_metric_unlocked(
    plasticity_anomaly_detector_t* detector,
    const char* metric_name,
    float value,
    plasticity_anomaly_category_t category
) {
    metric_tracker_t* tracker = find_or_create_tracker(detector, metric_name, category);
    if (!tracker) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tracker is NULL");

        return -1;
    }

    /* Add sample to history */
    tracker->history[tracker->history_index].value = value;
    tracker->history[tracker->history_index].timestamp = nimcp_time_get_us();
    tracker->history_index = (tracker->history_index + 1) % tracker->history_size;
    if (tracker->sample_count < tracker->history_size) {
        tracker->sample_count++;
    }

    return 0;
}

int plasticity_anomaly_submit_metric(
    plasticity_anomaly_detector_t* detector,
    const char* metric_name,
    float value,
    plasticity_anomaly_category_t category
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC || !metric_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_anomaly_submit_metric: required parameter is NULL (detector, metric_name)");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);
    int result = submit_metric_unlocked(detector, metric_name, value, category);
    nimcp_mutex_unlock(detector->mutex);

    return result;
}

int plasticity_anomaly_analyze_weights(
    plasticity_anomaly_detector_t* detector,
    const float* weights,
    size_t count,
    const char* component_name
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC ||
        !weights || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_analyze_weights: operation failed");
        return -1;
    }

    if (!detector->config.detect_weight_anomalies) {
        return 0;
    }

    nimcp_mutex_lock(detector->mutex);

    int anomalies = 0;
    float sum = 0.0f;
    float sum_sq = 0.0f;
    float max_w = -FLT_MAX;
    float min_w = FLT_MAX;
    uint32_t nan_count = 0;

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
    }

    size_t valid = count - nan_count;
    if (valid > 0) {
        float mean = sum / valid;
        float variance = (sum_sq / valid) - (mean * mean);

        /* Submit metrics (using unlocked version since we hold mutex) */
        submit_metric_unlocked(detector, "weight_mean",
                               mean, PLASTICITY_CATEGORY_WEIGHT);
        submit_metric_unlocked(detector, "weight_variance",
                               variance, PLASTICITY_CATEGORY_WEIGHT);
        submit_metric_unlocked(detector, "weight_max",
                               max_w, PLASTICITY_CATEGORY_WEIGHT);
    }

    /* Check for NaN */
    if (nan_count > 0) {
        plasticity_anomaly_report_t report = {0};
        report.type = PLASTICITY_ANOMALY_WEIGHT_GRADIENT_NAN;
        report.category = PLASTICITY_CATEGORY_WEIGHT;
        report.severity = PLASTICITY_SEVERITY_CRITICAL;
        report.metric_value = (float)nan_count;
        snprintf(report.description, sizeof(report.description),
                 "%u NaN weights in %s", nan_count,
                 component_name ? component_name : "unknown");
        report.detection_time_us = nimcp_time_get_us();
        if (component_name) {
            strncpy(report.component_name, component_name, sizeof(report.component_name) - 1);
        }
        record_anomaly(detector, &report);
        anomalies++;
    }

    nimcp_mutex_unlock(detector->mutex);

    return anomalies;
}

int plasticity_anomaly_analyze_timing(
    plasticity_anomaly_detector_t* detector,
    const float* pre_times,
    const float* post_times,
    size_t count
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC ||
        !pre_times || !post_times || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_analyze_timing: operation failed");
        return -1;
    }

    if (!detector->config.detect_timing_anomalies) {
        return 0;
    }

    nimcp_mutex_lock(detector->mutex);

    int anomalies = 0;
    float sum_delta = 0.0f;

    for (size_t i = 0; i < count; i++) {
        float delta = post_times[i] - pre_times[i];
        sum_delta += delta;
    }

    float mean_delta = sum_delta / count;
    submit_metric_unlocked(detector, "timing_delta",
                           mean_delta, PLASTICITY_CATEGORY_TIMING);

    nimcp_mutex_unlock(detector->mutex);

    return anomalies;
}

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

int plasticity_anomaly_set_callback(
    plasticity_anomaly_detector_t* detector,
    plasticity_anomaly_cb_t callback,
    void* user_data
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_set_callback: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);
    detector->anomaly_callback = callback;
    detector->anomaly_callback_data = user_data;
    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

int plasticity_anomaly_set_health_callback(
    plasticity_anomaly_detector_t* detector,
    plasticity_health_cb_t callback,
    void* user_data
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_set_health_callback: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);
    detector->health_callback = callback;
    detector->health_callback_data = user_data;
    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int plasticity_anomaly_get_stats(
    const plasticity_anomaly_detector_t* detector,
    plasticity_detection_stats_t* stats
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_anomaly_get_stats: required parameter is NULL (detector, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)detector->mutex);
    *stats = detector->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)detector->mutex);

    return 0;
}

float plasticity_anomaly_get_health(const plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        return -1.0f;
    }
    return detector->stats.current_health_score;
}

int plasticity_anomaly_get_reports(
    const plasticity_anomaly_detector_t* detector,
    plasticity_anomaly_report_t* reports,
    size_t max_reports
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC || !reports) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_anomaly_get_reports: required parameter is NULL (detector, reports)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)detector->mutex);

    size_t to_copy = detector->history_count;
    if (to_copy > max_reports) {
        to_copy = max_reports;
    }

    for (size_t i = 0; i < to_copy; i++) {
        reports[i] = detector->anomaly_history[i].report;
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)detector->mutex);

    return (int)to_copy;
}

void plasticity_anomaly_reset_stats(plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        return;
    }

    nimcp_mutex_lock(detector->mutex);

    uint32_t rules_active = detector->stats.rules_active;
    memset(&detector->stats, 0, sizeof(detector->stats));
    detector->stats.rules_active = rules_active;
    detector->stats.current_health_score = 1.0f;

    nimcp_mutex_unlock(detector->mutex);
}

void plasticity_anomaly_clear_history(plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        return;
    }

    nimcp_mutex_lock(detector->mutex);
    detector->history_count = 0;
    nimcp_mutex_unlock(detector->mutex);
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int plasticity_anomaly_connect_bio_async(
    plasticity_anomaly_detector_t* detector,
    bio_router_t router
) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_connect_bio_async: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);
    detector->bio_router = router;
    detector->config.enable_bio_async = (router != NULL);
    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

int plasticity_anomaly_broadcast(plasticity_anomaly_detector_t* detector) {
    if (!detector || detector->magic != PLASTICITY_ANOMALY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "plasticity_anomaly_broadcast: detector is NULL");
        return -1;
    }

    if (!detector->bio_router || !detector->config.enable_bio_async) {
        return 0;
    }

    /* Broadcast would go here */
    return 0;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* plasticity_anomaly_type_name(plasticity_anomaly_type_t type) {
    switch (type) {
        case PLASTICITY_ANOMALY_NONE: return "none";
        case PLASTICITY_ANOMALY_WEIGHT_EXPLOSION: return "weight_explosion";
        case PLASTICITY_ANOMALY_WEIGHT_VANISHING: return "weight_vanishing";
        case PLASTICITY_ANOMALY_WEIGHT_SATURATION: return "weight_saturation";
        case PLASTICITY_ANOMALY_WEIGHT_BIMODAL: return "weight_bimodal";
        case PLASTICITY_ANOMALY_WEIGHT_GRADIENT_NAN: return "weight_gradient_nan";
        case PLASTICITY_ANOMALY_WEIGHT_GRADIENT_INF: return "weight_gradient_inf";
        case PLASTICITY_ANOMALY_STDP_WINDOW_VIOLATION: return "stdp_window_violation";
        case PLASTICITY_ANOMALY_STDP_ASYMMETRY: return "stdp_asymmetry";
        case PLASTICITY_ANOMALY_STDP_RATE_MISMATCH: return "stdp_rate_mismatch";
        case PLASTICITY_ANOMALY_SPIKE_BURST: return "spike_burst";
        case PLASTICITY_ANOMALY_BCM_THRESHOLD_HIGH: return "bcm_threshold_high";
        case PLASTICITY_ANOMALY_BCM_THRESHOLD_LOW: return "bcm_threshold_low";
        case PLASTICITY_ANOMALY_BCM_THRESHOLD_DRIFT: return "bcm_threshold_drift";
        case PLASTICITY_ANOMALY_BCM_INVERSION: return "bcm_inversion";
        case PLASTICITY_ANOMALY_ACTIVITY_TOO_HIGH: return "activity_too_high";
        case PLASTICITY_ANOMALY_ACTIVITY_TOO_LOW: return "activity_too_low";
        case PLASTICITY_ANOMALY_SCALING_FAILURE: return "scaling_failure";
        case PLASTICITY_ANOMALY_INTRINSIC_INSTABILITY: return "intrinsic_instability";
        case PLASTICITY_ANOMALY_LEARNING_STALLED: return "learning_stalled";
        case PLASTICITY_ANOMALY_LEARNING_OSCILLATING: return "learning_oscillating";
        case PLASTICITY_ANOMALY_LEARNING_DIVERGING: return "learning_diverging";
        case PLASTICITY_ANOMALY_CONVERGENCE_FAILURE: return "convergence_failure";
        case PLASTICITY_ANOMALY_LOCAL_MINIMUM: return "local_minimum";
        case PLASTICITY_ANOMALY_SYNAPSE_DEATH: return "synapse_death";
        case PLASTICITY_ANOMALY_SYNAPSE_PROLIFERATION: return "synapse_proliferation";
        case PLASTICITY_ANOMALY_CONNECTIVITY_COLLAPSE: return "connectivity_collapse";
        case PLASTICITY_ANOMALY_SPINE_INSTABILITY: return "spine_instability";
        default: return "unknown";
    }
}

const char* plasticity_anomaly_category_name(plasticity_anomaly_category_t category) {
    if (category >= PLASTICITY_CATEGORY_COUNT) {
        return "unknown";
    }
    return category_names[category];
}

const char* plasticity_anomaly_severity_name(plasticity_severity_t severity) {
    if (severity > PLASTICITY_SEVERITY_CRITICAL) {
        return "unknown";
    }
    return severity_names[severity];
}

const char* plasticity_anomaly_action_name(plasticity_response_action_t action) {
    if (action > PLASTICITY_ACTION_NOTIFY_HEALTH) {
        return "unknown";
    }
    return action_names[action];
}

const char* plasticity_anomaly_detection_version(void) {
    return PLASTICITY_ANOMALY_VERSION;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static int check_rule(plasticity_anomaly_detector_t* detector,
                      plasticity_detection_rule_t* rule,
                      float value) {
    bool triggered = false;

    switch (rule->type) {
        case PLASTICITY_RULE_THRESHOLD:
            if (rule->params.threshold.check_upper &&
                value > rule->params.threshold.upper_threshold) {
                triggered = true;
            }
            if (rule->params.threshold.check_lower &&
                value < rule->params.threshold.lower_threshold) {
                triggered = true;
            }
            break;

        case PLASTICITY_RULE_TREND:
            /* Would need history analysis */
            break;

        case PLASTICITY_RULE_RATE_OF_CHANGE:
            /* Would need history analysis */
            break;

        case PLASTICITY_RULE_STATISTICAL:
            /* Would need baseline comparison */
            break;

        default:
            break;
    }

    if (triggered) {
        /* Create anomaly report */
        plasticity_anomaly_report_t report = {0};
        report.type = rule->anomaly;
        report.category = rule->category;
        report.severity = rule->severity;
        report.rule_id = rule->rule_id;
        strncpy(report.rule_name, rule->name, sizeof(report.rule_name) - 1);
        report.metric_value = value;
        report.detection_time_us = nimcp_time_get_us();
        report.action_taken = rule->action;

        snprintf(report.description, sizeof(report.description),
                 "Rule '%s' triggered: value=%.4f", rule->name, value);

        record_anomaly(detector, &report);

        /* Update statistics */
        detector->stats.anomalies_detected++;
        detector->stats.by_category[rule->category]++;
        detector->stats.by_severity[rule->severity]++;

        if (detector->config.auto_respond) {
            detector->stats.actions_taken++;
        }

        return 1;
    }

    return 0;
}

static void record_anomaly(plasticity_anomaly_detector_t* detector,
                           const plasticity_anomaly_report_t* report) {
    /* Check if already in history */
    for (uint32_t i = 0; i < detector->history_count; i++) {
        if (detector->anomaly_history[i].report.type == report->type &&
            detector->anomaly_history[i].report.rule_id == report->rule_id) {
            /* Update existing */
            detector->anomaly_history[i].last_seen = report->detection_time_us;
            detector->anomaly_history[i].count++;
            return;
        }
    }

    /* Add new */
    if (detector->history_count < detector->history_capacity) {
        anomaly_entry_t* entry = &detector->anomaly_history[detector->history_count];
        entry->report = *report;
        entry->first_seen = report->detection_time_us;
        entry->last_seen = report->detection_time_us;
        entry->count = 1;
        detector->history_count++;
    }

    /* Invoke callback */
    if (detector->anomaly_callback &&
        report->severity >= detector->config.min_report_severity) {
        detector->anomaly_callback(report, detector->anomaly_callback_data);
    }
}

static float compute_health_score(const plasticity_anomaly_detector_t* detector) {
    if (detector->stats.anomalies_detected == 0) {
        return 1.0f;
    }

    float score = 1.0f;

    /* Reduce for critical anomalies */
    score -= detector->stats.by_severity[PLASTICITY_SEVERITY_CRITICAL] * 0.3f;
    score -= detector->stats.by_severity[PLASTICITY_SEVERITY_ERROR] * 0.15f;
    score -= detector->stats.by_severity[PLASTICITY_SEVERITY_WARNING] * 0.05f;

    return fmaxf(0.0f, score);
}

static metric_tracker_t* find_or_create_tracker(
    plasticity_anomaly_detector_t* detector,
    const char* name,
    plasticity_anomaly_category_t category
) {
    /* Find existing */
    for (uint32_t i = 0; i < detector->tracker_count; i++) {
        if (strcmp(detector->trackers[i].name, name) == 0) {
            return &detector->trackers[i];
        }
    }

    /* Create new */
    if (detector->tracker_count >= detector->tracker_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_tracker: capacity exceeded");
        return NULL;
    }

    metric_tracker_t* tracker = &detector->trackers[detector->tracker_count];
    memset(tracker, 0, sizeof(*tracker));

    strncpy(tracker->name, name, sizeof(tracker->name) - 1);
    tracker->category = category;
    tracker->history_size = detector->config.history_size;
    tracker->history = nimcp_calloc(tracker->history_size, sizeof(metric_sample_t));

    if (!tracker->history) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "find_or_create_tracker: tracker->history is NULL");
        return NULL;
    }

    detector->tracker_count++;

    return tracker;
}
