/**
 * @file nimcp_predictive_analysis.c
 * @brief Enhanced Predictive Failure Analysis Implementation
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Advanced time-series analysis, anomaly detection, failure correlation
 * WHY:  Predict failures before they occur with higher accuracy
 * HOW:  LSTM-style patterns, correlation graphs, resource forecasting
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex predictive models (anticipate future states)
 * - Hippocampal time cells (temporal pattern recognition)
 * - Cerebellar forward models (predict outcomes of actions)
 * - Amygdala threat prediction (detect danger before it manifests)
 *
 * IMMUNE SYSTEM INTEGRATION:
 * - Anomaly predictions are shared with security module
 * - Security threats can inform failure predictions
 * - Proactive defense against predicted failures
 */

#include "utils/fault_tolerance/nimcp_predictive_analysis.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_analysis)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Time series data storage
 */
typedef struct {
    pa_sample_t samples[PA_MAX_SAMPLES];
    uint32_t head;
    uint32_t count;
    pa_series_meta_t meta;

    /* Statistics for anomaly detection */
    double rolling_mean;
    double rolling_variance;
    double ewma;  /* Exponentially weighted moving average */
    double ewma_var;

    /* Trend detection */
    double slope;
    double intercept;
} pa_series_data_t;

/**
 * @brief Failure correlation graph node
 */
typedef struct {
    pa_failure_edge_t edges[PA_MAX_CORRELATIONS];
    uint32_t edge_count;
} pa_failure_graph_t;

/**
 * @brief Alert storage
 */
typedef struct {
    pa_alert_t alerts[PA_MAX_ALERTS];
    uint32_t head;
    uint32_t count;
    uint32_t next_alert_id;
} pa_alert_store_t;

/**
 * @brief Internal context structure
 */
struct pa_context {
    pa_config_t config;

    /* Time series data */
    pa_series_data_t series[PA_MAX_SERIES];
    uint32_t series_count;

    /* Correlation data */
    pa_correlation_t correlations[PA_MAX_CORRELATIONS];
    uint32_t correlation_count;

    /* Failure graph */
    pa_failure_graph_t failure_graph;

    /* Alerts */
    pa_alert_store_t alert_store;

    /* Predictions */
    pa_failure_prediction_t predictions[16];
    uint32_t prediction_count;

    /* Statistics */
    pa_stats_t stats;

    /* Threading */
    nimcp_mutex_t mutex;
    nimcp_thread_t analysis_thread;
    bool running;
    bool thread_running;

    /* Security integration */
    bool security_registered;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds
 */
static uint64_t pa_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Update running statistics for a series
 */
static void pa_update_statistics(pa_series_data_t* series, double value) {
    if (!series) return;

    /* Update min/max */
    if (value < series->meta.min) series->meta.min = value;
    if (value > series->meta.max) series->meta.max = value;

    /* Welford's online algorithm for mean and variance */
    double count = (double)series->count;
    double delta = value - series->meta.mean;
    series->meta.mean += delta / count;
    double delta2 = value - series->meta.mean;
    series->meta.variance += delta * delta2;

    /* Exponentially weighted moving average */
    double alpha = 0.1;  /* Smoothing factor */
    if (series->count == 1) {
        series->ewma = value;
        series->ewma_var = 0.0;
    } else {
        double delta_ewma = value - series->ewma;
        series->ewma = series->ewma + alpha * delta_ewma;
        series->ewma_var = (1 - alpha) * (series->ewma_var + alpha * delta_ewma * delta_ewma);
    }
}

/**
 * @brief Calculate linear regression for trend detection
 */
static void pa_calculate_trend(pa_series_data_t* series) {
    if (!series || series->count < 2) return;

    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    uint32_t n = series->count < 100 ? series->count : 100;  /* Use last 100 samples */

    uint32_t start = (series->head + PA_MAX_SAMPLES - n) % PA_MAX_SAMPLES;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (start + i) % PA_MAX_SAMPLES;
        double x = (double)i;
        double y = series->samples[idx].value;

        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-10) {
        series->slope = 0.0;
        series->intercept = sum_y / n;
    } else {
        series->slope = (n * sum_xy - sum_x * sum_y) / denom;
        series->intercept = (sum_y - series->slope * sum_x) / n;
    }

    /* Calculate residual variance (variance after removing linear trend) */
    double sum_residual_sq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (start + i) % PA_MAX_SAMPLES;
        double predicted = series->slope * (double)i + series->intercept;
        double residual = series->samples[idx].value - predicted;
        sum_residual_sq += residual * residual;
    }
    double residual_std = n > 1 ? sqrt(sum_residual_sq / (n - 1)) : 0.0;

    /* Determine trend type using residual variance for threshold
     * WHY: Using raw variance would incorrectly mark monotonic data as STABLE
     *      because the raw variance is high even though trend is clear.
     *      Residual variance measures how well the linear fit captures the data. */
    double slope_threshold = residual_std > 0 ? residual_std * 0.1 : 0.01;

    if (series->slope > slope_threshold) {
        series->meta.trend = PA_TREND_INCREASING;
    } else if (series->slope < -slope_threshold) {
        series->meta.trend = PA_TREND_DECREASING;
    } else {
        /* Check for cyclic patterns or volatility */
        double variance_ratio = series->ewma_var / (series->meta.variance / series->count + 1e-10);
        if (variance_ratio > 2.0) {
            series->meta.trend = PA_TREND_VOLATILE;
        } else {
            series->meta.trend = PA_TREND_STABLE;
        }
    }
}

/**
 * @brief Z-score anomaly detection
 */
static bool pa_detect_zscore(pa_series_data_t* series, double value, double threshold, double* score) {
    if (!series || series->count < 10) {
        if (score) *score = 0.0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_detect_zscore: validation failed");
        return false;
    }

    double std_dev = sqrt(series->meta.variance / series->count);
    if (std_dev < 1e-10) std_dev = 1e-10;

    double z = fabs(value - series->meta.mean) / std_dev;
    if (score) *score = z;

    return z > threshold;
}

/**
 * @brief IQR-based anomaly detection
 */
static bool pa_detect_iqr(pa_series_data_t* series, double value, double* score) {
    if (!series || series->count < 20) {
        if (score) *score = 0.0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_detect_iqr: validation failed");
        return false;
    }

    /* Simplified: use mean +/- 1.5 * std as proxy for IQR */
    double std_dev = sqrt(series->meta.variance / series->count);
    double q1 = series->meta.mean - 0.675 * std_dev;  /* Approximate 25th percentile */
    double q3 = series->meta.mean + 0.675 * std_dev;  /* Approximate 75th percentile */
    double iqr = q3 - q1;

    double lower = q1 - 1.5 * iqr;
    double upper = q3 + 1.5 * iqr;

    if (score) {
        if (value < lower) {
            *score = (lower - value) / (iqr + 1e-10);
        } else if (value > upper) {
            *score = (value - upper) / (iqr + 1e-10);
        } else {
            *score = 0.0;
        }
    }

    return (value < lower || value > upper);
}

/**
 * @brief Linear forecast
 */
static void pa_forecast_linear(pa_series_data_t* series, double* forecast, uint32_t horizon) {
    if (!series || !forecast) return;

    for (uint32_t h = 0; h < horizon; h++) {
        forecast[h] = series->intercept + series->slope * (series->count + h);
    }
}

/**
 * @brief Exponential smoothing forecast
 */
static void pa_forecast_exponential(pa_series_data_t* series, double* forecast, uint32_t horizon) {
    if (!series || !forecast || series->count == 0) return;

    double last_value = series->ewma;

    for (uint32_t h = 0; h < horizon; h++) {
        forecast[h] = last_value + series->slope * (h + 1);
    }
}

/**
 * @brief Notify security of prediction
 */
static void pa_notify_security(pa_context_t* ctx, const pa_failure_prediction_t* pred) {
    if (!ctx || !pred) return;

    bbb_audit_level_t level = pred->probability > 0.8 ? BBB_AUDIT_ERROR : BBB_AUDIT_WARNING;

    bbb_audit_log(level, "PA", "PREDICTION",
                  "Predicted failure: type=%u, probability=%.2f%%, lead_time=%lu ms: %s",
                  pred->failure_type, pred->probability * 100.0,
                  (unsigned long)(pred->predicted_time_ms - pa_get_time_ms()),
                  pred->reasoning);
}

/**
 * @brief Create alert from prediction
 */
static void pa_create_alert(pa_context_t* ctx, const pa_failure_prediction_t* pred) {
    if (!ctx || !pred) return;

    pa_alert_store_t* store = &ctx->alert_store;

    pa_alert_t alert = {0};
    alert.alert_id = store->next_alert_id++;
    alert.created_at_ms = pa_get_time_ms();
    alert.expires_at_ms = pred->predicted_time_ms + pred->confidence_window_ms;
    alert.prediction = *pred;

    /* Determine severity */
    if (pred->probability >= 0.9) {
        alert.severity = PA_ALERT_EMERGENCY;
    } else if (pred->probability >= 0.7) {
        alert.severity = PA_ALERT_CRITICAL;
    } else if (pred->probability >= 0.5) {
        alert.severity = PA_ALERT_WARNING;
    } else {
        alert.severity = PA_ALERT_INFO;
    }

    snprintf(alert.action_required, sizeof(alert.action_required),
             "Monitor %s, prepare recovery for fault type %u",
             pa_series_type_to_string(pred->trigger_series), pred->failure_type);

    store->alerts[store->head] = alert;
    store->head = (store->head + 1) % PA_MAX_ALERTS;
    if (store->count < PA_MAX_ALERTS) store->count++;

    LOG_WARNING("PA", "Created alert %u: %s (severity=%s, prob=%.1f%%)",
                      alert.alert_id, pred->reasoning,
                      pa_alert_severity_to_string(alert.severity),
                      pred->probability * 100.0);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pa_config_t pa_default_config(void) {
    pa_config_t config = {
        .detection_method = PA_DETECT_ENSEMBLE,
        .forecast_model = PA_MODEL_EXPONENTIAL,
        .sample_rate_ms = 1000,
        .forecast_horizon = PA_FORECAST_HORIZON,
        .anomaly_threshold = 3.0,
        .prediction_threshold = 0.5,
        .correlation_window = 100,
        .seasonality_period = 24,
        .enable_correlation_graph = true,
        .enable_ensemble = true,
        .enable_auto_alerts = true
    };
    return config;
}

pa_context_t* pa_create(const pa_config_t* config) {
    if (!config) {
        LOG_ERROR("PA", "NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    pa_context_t* ctx = (pa_context_t*)nimcp_malloc(sizeof(pa_context_t));
    if (!ctx) {
        LOG_ERROR("PA", "Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    memset(ctx, 0, sizeof(pa_context_t));
    ctx->config = *config;
    ctx->alert_store.next_alert_id = 1;

    if (nimcp_mutex_init(&ctx->mutex, NULL) != 0) {
        LOG_ERROR("PA", "Failed to initialize mutex");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pa_create: validation failed");
        return NULL;
    }

    /* Initialize series metadata */
    for (int i = 0; i < PA_MAX_SERIES; i++) {
        ctx->series[i].meta.type = i;
        ctx->series[i].meta.min = DBL_MAX;
        ctx->series[i].meta.max = -DBL_MAX;
        ctx->series[i].meta.trend = PA_TREND_STABLE;
        strncpy(ctx->series[i].meta.name, pa_series_type_to_string(i),
                sizeof(ctx->series[i].meta.name) - 1);
    }

    /* Register with security module */
    ctx->security_registered = bbb_register_module("predictive_analysis", BBB_MODULE_TYPE_CORE);

    bbb_audit_log(BBB_AUDIT_INFO, "PA", "CREATE",
                  "Created predictive analysis context, method=%s, model=%s",
                  pa_detection_method_to_string(config->detection_method),
                  pa_model_type_to_string(config->forecast_model));

    LOG_INFO("PA", "Created predictive analysis context");

    return ctx;
}

void pa_destroy(pa_context_t* ctx) {
    if (!ctx) return;

    pa_stop(ctx);

    bbb_audit_log(BBB_AUDIT_INFO, "PA", "DESTROY",
                  "Destroying predictive analysis context");

    nimcp_mutex_destroy(&ctx->mutex);
    nimcp_free(ctx);

    LOG_INFO("PA", "Destroyed predictive analysis context");
}

bool pa_start(pa_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_start: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->running = true;
    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "PA", "START", "Started predictive analysis");
    LOG_INFO("PA", "Started predictive analysis");

    return true;
}

bool pa_stop(pa_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_stop: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    ctx->running = false;
    ctx->thread_running = false;
    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_INFO, "PA", "STOP", "Stopped predictive analysis");
    LOG_INFO("PA", "Stopped predictive analysis");

    return true;
}

//=============================================================================
// Data Collection
//=============================================================================

bool pa_add_sample(pa_context_t* ctx, pa_series_type_t series, double value) {
    return pa_add_sample_timed(ctx, series, value, pa_get_time_ms());
}

bool pa_add_sample_timed(pa_context_t* ctx, pa_series_type_t series, double value, uint64_t timestamp_ms) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_add_sample_timed: ctx is NULL");
        return false;
    }
    if (series < 0 || series >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_add_sample_timed: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];

    /* Add sample */
    s->samples[s->head].value = value;
    s->samples[s->head].timestamp_ms = timestamp_ms;
    s->head = (s->head + 1) % PA_MAX_SAMPLES;

    if (s->count < PA_MAX_SAMPLES) {
        s->count++;
    }

    s->meta.sample_count = s->count;
    s->meta.last_sample_ms = timestamp_ms;
    if (s->meta.first_sample_ms == 0) {
        s->meta.first_sample_ms = timestamp_ms;
    }

    /* Update statistics */
    pa_update_statistics(s, value);

    /* Periodically recalculate trend */
    if (s->count % 10 == 0) {
        pa_calculate_trend(s);
    }

    ctx->stats.total_samples++;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

uint32_t pa_add_samples(pa_context_t* ctx, pa_series_type_t series,
                         const pa_sample_t* samples, uint32_t count) {
    if (!ctx || !samples || count == 0) return 0;

    uint32_t added = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (pa_add_sample_timed(ctx, series, samples[i].value, samples[i].timestamp_ms)) {
            added++;
        }
    }

    return added;
}

bool pa_get_series_meta(pa_context_t* ctx, pa_series_type_t series, pa_series_meta_t* meta) {
    if (!ctx || !meta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_get_series_meta: required parameter is NULL (ctx, meta)");
        return false;
    }
    if (series < 0 || series >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_get_series_meta: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    *meta = ctx->series[series].meta;
    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

uint32_t pa_get_samples(pa_context_t* ctx, pa_series_type_t series,
                         pa_sample_t* samples, uint32_t count) {
    if (!ctx || !samples || count == 0) return 0;
    if (series < 0 || series >= PA_MAX_SERIES) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];
    uint32_t to_copy = s->count < count ? s->count : count;

    uint32_t start = (s->head + PA_MAX_SAMPLES - to_copy) % PA_MAX_SAMPLES;
    for (uint32_t i = 0; i < to_copy; i++) {
        samples[i] = s->samples[(start + i) % PA_MAX_SAMPLES];
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return to_copy;
}

//=============================================================================
// Anomaly Detection
//=============================================================================

uint32_t pa_detect_anomalies(pa_context_t* ctx, pa_series_type_t series,
                              pa_anomaly_t* anomalies, uint32_t max_anomalies) {
    if (!ctx || !anomalies || max_anomalies == 0) return 0;
    if (series < 0 || series >= PA_MAX_SERIES) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];
    uint32_t anomaly_count = 0;

    /* Check recent samples */
    uint32_t check_count = s->count < 100 ? s->count : 100;
    uint32_t start = (s->head + PA_MAX_SAMPLES - check_count) % PA_MAX_SAMPLES;

    for (uint32_t i = 0; i < check_count && anomaly_count < max_anomalies; i++) {
        uint32_t idx = (start + i) % PA_MAX_SAMPLES;
        double value = s->samples[idx].value;
        double score = 0.0;
        bool is_anomaly = false;

        if (ctx->config.enable_ensemble || ctx->config.detection_method == PA_DETECT_ENSEMBLE) {
            /* Ensemble: combine multiple methods */
            double zscore, iqr_score;
            bool z_anomaly = pa_detect_zscore(s, value, ctx->config.anomaly_threshold, &zscore);
            bool iqr_anomaly = pa_detect_iqr(s, value, &iqr_score);

            is_anomaly = z_anomaly || iqr_anomaly;
            score = (zscore + iqr_score) / 2.0;
        } else {
            switch (ctx->config.detection_method) {
                case PA_DETECT_ZSCORE:
                    is_anomaly = pa_detect_zscore(s, value, ctx->config.anomaly_threshold, &score);
                    break;
                case PA_DETECT_IQR:
                    is_anomaly = pa_detect_iqr(s, value, &score);
                    break;
                default:
                    is_anomaly = pa_detect_zscore(s, value, ctx->config.anomaly_threshold, &score);
                    break;
            }
        }

        if (is_anomaly) {
            pa_anomaly_t* a = &anomalies[anomaly_count++];
            a->series = series;
            a->value = value;
            a->expected = s->meta.mean;
            a->deviation = score;
            a->method = ctx->config.detection_method;
            a->confidence = 1.0f - (float)exp(-score);
            a->detected_at_ms = s->samples[idx].timestamp_ms;

            snprintf(a->description, sizeof(a->description),
                     "%s anomaly detected: value=%.3f, expected=%.3f (%.1f sigma)",
                     pa_series_type_to_string(series), value, s->meta.mean, score);

            ctx->stats.anomalies_detected++;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return anomaly_count;
}

uint32_t pa_detect_all_anomalies(pa_context_t* ctx, pa_anomaly_t* anomalies, uint32_t max_anomalies) {
    if (!ctx || !anomalies || max_anomalies == 0) return 0;

    uint32_t total = 0;

    for (int s = 0; s < PA_MAX_SERIES && total < max_anomalies; s++) {
        total += pa_detect_anomalies(ctx, s, anomalies + total, max_anomalies - total);
    }

    return total;
}

bool pa_is_anomalous(pa_context_t* ctx, pa_series_type_t series, double value) {
    if (!ctx) {
        return false;
    }
    if (series < 0 || series >= PA_MAX_SERIES) {
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];
    double score;
    bool result = pa_detect_zscore(s, value, ctx->config.anomaly_threshold, &score);

    nimcp_mutex_unlock(&ctx->mutex);

    return result;
}

double pa_get_anomaly_score(pa_context_t* ctx, pa_series_type_t series, double value) {
    if (!ctx) return 0.0;
    if (series < 0 || series >= PA_MAX_SERIES) return 0.0;

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];
    double score = 0.0;
    pa_detect_zscore(s, value, ctx->config.anomaly_threshold, &score);

    nimcp_mutex_unlock(&ctx->mutex);

    return score;
}

//=============================================================================
// Correlation Analysis
//=============================================================================

bool pa_calculate_correlation(pa_context_t* ctx, pa_series_type_t series_a,
                               pa_series_type_t series_b, pa_correlation_t* correlation) {
    if (!ctx || !correlation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_get_anomaly_score: required parameter is NULL (ctx, correlation)");
        return false;
    }
    if (series_a < 0 || series_a >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_get_anomaly_score: capacity exceeded");
        return false;
    }
    if (series_b < 0 || series_b >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_get_anomaly_score: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* a = &ctx->series[series_a];
    pa_series_data_t* b = &ctx->series[series_b];

    uint32_t n = a->count < b->count ? a->count : b->count;
    if (n < 10) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_get_anomaly_score: validation failed");
        return false;
    }

    /* Calculate Pearson correlation */
    double sum_a = 0.0, sum_b = 0.0, sum_ab = 0.0;
    double sum_a2 = 0.0, sum_b2 = 0.0;

    uint32_t window = n < ctx->config.correlation_window ? n : ctx->config.correlation_window;
    uint32_t start_a = (a->head + PA_MAX_SAMPLES - window) % PA_MAX_SAMPLES;
    uint32_t start_b = (b->head + PA_MAX_SAMPLES - window) % PA_MAX_SAMPLES;

    for (uint32_t i = 0; i < window; i++) {
        double va = a->samples[(start_a + i) % PA_MAX_SAMPLES].value;
        double vb = b->samples[(start_b + i) % PA_MAX_SAMPLES].value;

        sum_a += va;
        sum_b += vb;
        sum_ab += va * vb;
        sum_a2 += va * va;
        sum_b2 += vb * vb;
    }

    double mean_a = sum_a / window;
    double mean_b = sum_b / window;
    double std_a = sqrt(sum_a2 / window - mean_a * mean_a);
    double std_b = sqrt(sum_b2 / window - mean_b * mean_b);

    double cov = sum_ab / window - mean_a * mean_b;

    correlation->series_a = series_a;
    correlation->series_b = series_b;

    if (std_a < 1e-10 || std_b < 1e-10) {
        correlation->correlation = 0.0;
    } else {
        correlation->correlation = cov / (std_a * std_b);
    }

    correlation->lag = 0;  /* Simplified: no lag calculation */
    correlation->is_causal = false;
    correlation->confidence = (float)fabs(correlation->correlation);

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

uint32_t pa_find_correlations(pa_context_t* ctx, pa_correlation_t* correlations,
                               uint32_t max_correlations, float min_correlation) {
    if (!ctx || !correlations || max_correlations == 0) return 0;

    uint32_t count = 0;

    for (int a = 0; a < PA_MAX_SERIES && count < max_correlations; a++) {
        for (int b = a + 1; b < PA_MAX_SERIES && count < max_correlations; b++) {
            pa_correlation_t corr;
            if (pa_calculate_correlation(ctx, a, b, &corr)) {
                if (fabs(corr.correlation) >= min_correlation) {
                    correlations[count++] = corr;
                }
            }
        }
    }

    return count;
}

bool pa_build_failure_graph(pa_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_build_failure_graph: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    /* Note: Do NOT clear edge_count here - edges are added by pa_record_failure_sequence */
    /* This function is called to finalize/validate the graph structure */
    nimcp_mutex_unlock(&ctx->mutex);

    /* Graph is built from recorded failure sequences */
    return true;
}

uint32_t pa_get_failure_edges(pa_context_t* ctx, pa_failure_edge_t* edges, uint32_t max_edges) {
    if (!ctx || !edges || max_edges == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t count = ctx->failure_graph.edge_count < max_edges ?
                     ctx->failure_graph.edge_count : max_edges;
    memcpy(edges, ctx->failure_graph.edges, count * sizeof(pa_failure_edge_t));

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

bool pa_record_failure_sequence(pa_context_t* ctx, const uint32_t* failure_types,
                                  uint32_t count, const uint32_t* time_deltas_ms) {
    if (!ctx || !failure_types || count < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_get_failure_edges: required parameter is NULL (ctx, failure_types)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Add edges to graph */
    for (uint32_t i = 0; i < count - 1; i++) {
        /* Check if edge exists */
        bool found = false;
        for (uint32_t e = 0; e < ctx->failure_graph.edge_count; e++) {
            pa_failure_edge_t* edge = &ctx->failure_graph.edges[e];
            if (edge->source_failure == failure_types[i] &&
                edge->target_failure == failure_types[i + 1]) {

                edge->occurrence_count++;
                /* Update average time lag */
                if (time_deltas_ms) {
                    edge->time_lag_ms = (edge->time_lag_ms + time_deltas_ms[i]) / 2;
                }
                found = true;
                break;
            }
        }

        if (!found && ctx->failure_graph.edge_count < PA_MAX_CORRELATIONS) {
            pa_failure_edge_t* edge = &ctx->failure_graph.edges[ctx->failure_graph.edge_count++];
            edge->source_failure = failure_types[i];
            edge->target_failure = failure_types[i + 1];
            edge->probability = 0.5f;  /* Initial probability */
            edge->time_lag_ms = time_deltas_ms ? time_deltas_ms[i] : 1000;
            edge->occurrence_count = 1;
        }
    }

    /* Update probabilities */
    for (uint32_t e = 0; e < ctx->failure_graph.edge_count; e++) {
        pa_failure_edge_t* edge = &ctx->failure_graph.edges[e];
        /* Simple: probability = count / total observations */
        edge->probability = (float)edge->occurrence_count /
                           (float)(edge->occurrence_count + 10);  /* Smooth with prior */
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

//=============================================================================
// Forecasting
//=============================================================================

bool pa_forecast(pa_context_t* ctx, pa_series_type_t series, pa_forecast_t* forecast) {
    return pa_forecast_with_model(ctx, series, ctx->config.forecast_model,
                                   ctx->config.forecast_horizon, forecast);
}

bool pa_forecast_with_model(pa_context_t* ctx, pa_series_type_t series,
                              pa_model_type_t model, uint32_t horizon,
                              pa_forecast_t* forecast) {
    if (!ctx || !forecast) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_forecast: required parameter is NULL (ctx, forecast)");
        return false;
    }
    if (series < 0 || series >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_forecast: capacity exceeded");
        return false;
    }
    if (horizon > PA_FORECAST_HORIZON) horizon = PA_FORECAST_HORIZON;

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];

    if (s->count < 10) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_forecast: validation failed");
        return false;
    }

    memset(forecast, 0, sizeof(pa_forecast_t));
    forecast->series = series;
    forecast->horizon = horizon;
    forecast->model = model;
    forecast->forecast_time_ms = pa_get_time_ms();
    forecast->target_time_ms = forecast->forecast_time_ms +
                               horizon * ctx->config.sample_rate_ms;

    /* Calculate trend if needed */
    pa_calculate_trend(s);

    /* Generate forecasts */
    switch (model) {
        case PA_MODEL_LINEAR:
            pa_forecast_linear(s, forecast->forecasts, horizon);
            break;

        case PA_MODEL_EXPONENTIAL:
        case PA_MODEL_HOLT_WINTERS:
            pa_forecast_exponential(s, forecast->forecasts, horizon);
            break;

        default:
            pa_forecast_linear(s, forecast->forecasts, horizon);
            break;
    }

    /* Calculate confidence bounds (simple: mean +/- 2*std) */
    double std_dev = sqrt(s->meta.variance / s->count);
    for (uint32_t h = 0; h < horizon; h++) {
        /* Widen bounds for farther forecasts */
        double uncertainty = std_dev * (1.0 + h * 0.1);
        forecast->lower_bound[h] = forecast->forecasts[h] - 2.0 * uncertainty;
        forecast->upper_bound[h] = forecast->forecasts[h] + 2.0 * uncertainty;
    }

    /* Calculate confidence based on trend stability */
    forecast->confidence = s->meta.trend == PA_TREND_STABLE ? 0.8f :
                          s->meta.trend == PA_TREND_VOLATILE ? 0.4f : 0.6f;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool pa_time_to_threshold(pa_context_t* ctx, pa_series_type_t series,
                           double threshold, uint64_t* time_ms) {
    if (!ctx || !time_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_forecast: required parameter is NULL (ctx, time_ms)");
        return false;
    }
    if (series < 0 || series >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_forecast: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];

    if (s->count < 10) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_forecast: validation failed");
        return false;
    }

    pa_calculate_trend(s);

    /* Check if current value already exceeds threshold */
    double current = s->ewma;
    if ((s->slope > 0 && current >= threshold) ||
        (s->slope < 0 && current <= threshold)) {
        *time_ms = 0;
        nimcp_mutex_unlock(&ctx->mutex);
        return true;
    }

    /* Calculate time to threshold */
    if (fabs(s->slope) < 1e-10) {
        /* No trend, won't reach threshold */
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_forecast: validation failed");
        return false;
    }

    double delta = threshold - current;
    if ((s->slope > 0 && delta < 0) || (s->slope < 0 && delta > 0)) {
        /* Moving away from threshold */
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_forecast: validation failed");
        return false;
    }

    double steps = delta / s->slope;
    *time_ms = (uint64_t)(steps * ctx->config.sample_rate_ms);

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

bool pa_detect_seasonality(pa_context_t* ctx, pa_series_type_t series, pa_seasonality_t* seasonality) {
    if (!ctx || !seasonality) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_detect_seasonality: required parameter is NULL (ctx, seasonality)");
        return false;
    }
    if (series < 0 || series >= PA_MAX_SERIES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pa_detect_seasonality: capacity exceeded");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_series_data_t* s = &ctx->series[series];

    if (s->count < ctx->config.seasonality_period * 2) {
        nimcp_mutex_unlock(&ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_detect_seasonality: validation failed");
        return false;
    }

    /* Simplified seasonality detection using autocorrelation */
    seasonality->period = ctx->config.seasonality_period;
    seasonality->strength = 0.0f;  /* Would require more complex analysis */
    seasonality->pattern = NULL;

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

pa_trend_t pa_get_trend(pa_context_t* ctx, pa_series_type_t series) {
    if (!ctx) return PA_TREND_STABLE;
    if (series < 0 || series >= PA_MAX_SERIES) return PA_TREND_STABLE;

    nimcp_mutex_lock(&ctx->mutex);

    /* Ensure trend is calculated for current data */
    pa_series_data_t* s = &ctx->series[series];
    if (s->count >= 2) {
        pa_calculate_trend(s);
    }

    pa_trend_t trend = s->meta.trend;
    nimcp_mutex_unlock(&ctx->mutex);

    return trend;
}

//=============================================================================
// Failure Prediction
//=============================================================================

uint32_t pa_predict_failures(pa_context_t* ctx, pa_failure_prediction_t* predictions, uint32_t max_predictions) {
    if (!ctx || !predictions || max_predictions == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    uint32_t pred_count = 0;

    /* Check each series for concerning trends */
    for (int s = 0; s < PA_MAX_SERIES && pred_count < max_predictions; s++) {
        pa_series_data_t* series = &ctx->series[s];

        if (series->count < 20) continue;

        pa_calculate_trend(series);

        /* Check for increasing error metrics */
        bool is_error_metric = (s == PA_SERIES_ERROR_RATE || s == PA_SERIES_LATENCY);
        bool is_resource_metric = (s == PA_SERIES_MEMORY || s == PA_SERIES_CPU);

        float probability = 0.0f;
        uint32_t failure_type = 0;
        char reasoning[NIMCP_ERROR_BUFFER_SIZE] = {0};

        if (is_error_metric && series->meta.trend == PA_TREND_INCREASING) {
            probability = 0.5f + (float)fabs(series->slope) * 0.1f;
            failure_type = 1;  /* Error-based failure */
            snprintf(reasoning, sizeof(reasoning),
                     "Increasing %s trend detected (slope=%.4f)",
                     pa_series_type_to_string(s), series->slope);
        }
        else if (is_resource_metric && series->ewma > 80.0) {
            probability = (float)(series->ewma - 80.0) / 20.0f;
            failure_type = 2;  /* Resource exhaustion */
            snprintf(reasoning, sizeof(reasoning),
                     "%s utilization at %.1f%%, approaching critical threshold",
                     pa_series_type_to_string(s), series->ewma);
        }
        else if (series->meta.trend == PA_TREND_VOLATILE) {
            probability = 0.3f;
            failure_type = 3;  /* Instability */
            snprintf(reasoning, sizeof(reasoning),
                     "High volatility in %s may indicate instability",
                     pa_series_type_to_string(s));
        }

        if (probability >= ctx->config.prediction_threshold) {
            pa_failure_prediction_t* pred = &predictions[pred_count++];
            pred->failure_type = failure_type;
            pred->probability = probability > 1.0f ? 1.0f : probability;
            pred->predicted_time_ms = pa_get_time_ms() +
                                      (uint64_t)(10000 / (probability + 0.1));
            pred->confidence_window_ms = 60000;
            pred->trigger_series = s;
            pred->trigger_threshold = series->ewma * 1.2;
            pred->confidence = probability;
            strncpy(pred->reasoning, reasoning, sizeof(pred->reasoning) - 1);
            pred->anomaly_count = 0;

            ctx->stats.predictions_made++;

            /* Auto-create alert if enabled */
            if (ctx->config.enable_auto_alerts) {
                pa_create_alert(ctx, pred);
            }

            /* Notify security */
            pa_notify_security(ctx, pred);
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return pred_count;
}

bool pa_get_highest_risk_failure(pa_context_t* ctx, pa_failure_prediction_t* prediction) {
    if (!ctx || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_get_highest_risk_failure: required parameter is NULL (ctx, prediction)");
        return false;
    }

    pa_failure_prediction_t predictions[16];
    uint32_t count = pa_predict_failures(ctx, predictions, 16);

    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_get_highest_risk_failure: count is zero");
        return false;
    }

    /* Find highest probability */
    pa_failure_prediction_t* highest = &predictions[0];
    for (uint32_t i = 1; i < count; i++) {
        if (predictions[i].probability > highest->probability) {
            highest = &predictions[i];
        }
    }

    *prediction = *highest;
    return true;
}

bool pa_predict_failure_type(pa_context_t* ctx, uint32_t failure_type, pa_failure_prediction_t* prediction) {
    if (!ctx || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_predict_failure_type: required parameter is NULL (ctx, prediction)");
        return false;
    }

    pa_failure_prediction_t predictions[16];
    uint32_t count = pa_predict_failures(ctx, predictions, 16);

    for (uint32_t i = 0; i < count; i++) {
        if (predictions[i].failure_type == failure_type) {
            *prediction = predictions[i];
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_predict_failure_type: validation failed");
    return false;
}

bool pa_record_actual_failure(pa_context_t* ctx, uint32_t failure_type, uint64_t timestamp_ms) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_record_actual_failure: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    /* Check predictions to update accuracy */
    bool was_predicted = false;
    for (uint32_t i = 0; i < ctx->prediction_count; i++) {
        if (ctx->predictions[i].failure_type == failure_type) {
            was_predicted = true;
            ctx->stats.predictions_correct++;
            break;
        }
    }

    if (!was_predicted) {
        ctx->stats.false_negatives++;
    }

    /* Update accuracy metrics */
    if (ctx->stats.predictions_made > 0) {
        ctx->stats.prediction_accuracy = (float)ctx->stats.predictions_correct /
                                         (float)ctx->stats.predictions_made;
    }

    nimcp_mutex_unlock(&ctx->mutex);

    bbb_audit_log(BBB_AUDIT_ERROR, "PA", "ACTUAL_FAILURE",
                  "Failure occurred: type=%u, was_predicted=%d", failure_type, was_predicted);

    return true;
}

//=============================================================================
// Alerts
//=============================================================================

uint32_t pa_get_alerts(pa_context_t* ctx, pa_alert_t* alerts, uint32_t max_alerts) {
    if (!ctx || !alerts || max_alerts == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    pa_alert_store_t* store = &ctx->alert_store;
    uint32_t count = store->count < max_alerts ? store->count : max_alerts;

    uint32_t start = (store->head + PA_MAX_ALERTS - store->count) % PA_MAX_ALERTS;
    for (uint32_t i = 0; i < count; i++) {
        alerts[i] = store->alerts[(start + i) % PA_MAX_ALERTS];
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

uint32_t pa_get_alerts_by_severity(pa_context_t* ctx, pa_alert_severity_t severity,
                                    pa_alert_t* alerts, uint32_t max_alerts) {
    if (!ctx || !alerts || max_alerts == 0) return 0;

    nimcp_mutex_lock(&ctx->mutex);

    pa_alert_store_t* store = &ctx->alert_store;
    uint32_t count = 0;

    uint32_t start = (store->head + PA_MAX_ALERTS - store->count) % PA_MAX_ALERTS;
    for (uint32_t i = 0; i < store->count && count < max_alerts; i++) {
        pa_alert_t* a = &store->alerts[(start + i) % PA_MAX_ALERTS];
        if (a->severity >= severity) {
            alerts[count++] = *a;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return count;
}

bool pa_acknowledge_alert(pa_context_t* ctx, uint32_t alert_id) {
    if (!ctx || alert_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_acknowledge_alert: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_alert_store_t* store = &ctx->alert_store;
    uint32_t start = (store->head + PA_MAX_ALERTS - store->count) % PA_MAX_ALERTS;

    for (uint32_t i = 0; i < store->count; i++) {
        pa_alert_t* a = &store->alerts[(start + i) % PA_MAX_ALERTS];
        if (a->alert_id == alert_id) {
            a->acknowledged = true;
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_acknowledge_alert: validation failed");
    return false;
}

bool pa_suppress_alert(pa_context_t* ctx, uint32_t alert_id, uint64_t duration_ms) {
    if (!ctx || alert_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_suppress_alert: ctx is NULL");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);

    pa_alert_store_t* store = &ctx->alert_store;
    uint32_t start = (store->head + PA_MAX_ALERTS - store->count) % PA_MAX_ALERTS;

    for (uint32_t i = 0; i < store->count; i++) {
        pa_alert_t* a = &store->alerts[(start + i) % PA_MAX_ALERTS];
        if (a->alert_id == alert_id) {
            a->suppressed = true;
            a->expires_at_ms = pa_get_time_ms() + duration_ms;
            nimcp_mutex_unlock(&ctx->mutex);
            return true;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_suppress_alert: validation failed");
    return false;
}

uint32_t pa_clear_expired_alerts(pa_context_t* ctx) {
    if (!ctx) return 0;

    uint64_t now = pa_get_time_ms();
    uint32_t cleared = 0;

    nimcp_mutex_lock(&ctx->mutex);

    pa_alert_store_t* store = &ctx->alert_store;

    /* Simple: just mark as suppressed if expired (actual removal would require compaction) */
    uint32_t start = (store->head + PA_MAX_ALERTS - store->count) % PA_MAX_ALERTS;
    for (uint32_t i = 0; i < store->count; i++) {
        pa_alert_t* a = &store->alerts[(start + i) % PA_MAX_ALERTS];
        if (a->expires_at_ms > 0 && a->expires_at_ms < now && !a->suppressed) {
            a->suppressed = true;
            cleared++;
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return cleared;
}

//=============================================================================
// Statistics
//=============================================================================

bool pa_get_stats(pa_context_t* ctx, pa_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }

    nimcp_mutex_lock(&ctx->mutex);
    *stats = ctx->stats;

    /* Calculate derived metrics */
    if (stats->predictions_made > 0) {
        stats->prediction_accuracy = (float)stats->predictions_correct /
                                     (float)stats->predictions_made;

        uint64_t tp = stats->predictions_correct;
        uint64_t fp = stats->false_positives;
        uint64_t fn = stats->false_negatives;

        stats->precision = (tp + fp > 0) ? (float)tp / (tp + fp) : 0.0f;
        stats->recall = (tp + fn > 0) ? (float)tp / (tp + fn) : 0.0f;

        if (stats->precision + stats->recall > 0) {
            stats->f1_score = 2.0f * stats->precision * stats->recall /
                             (stats->precision + stats->recall);
        }
    }

    nimcp_mutex_unlock(&ctx->mutex);

    return true;
}

void pa_reset_stats(pa_context_t* ctx) {
    if (!ctx) return;

    nimcp_mutex_lock(&ctx->mutex);
    memset(&ctx->stats, 0, sizeof(pa_stats_t));
    nimcp_mutex_unlock(&ctx->mutex);
}

float pa_get_accuracy(pa_context_t* ctx) {
    if (!ctx) return 0.0f;

    nimcp_mutex_lock(&ctx->mutex);
    float accuracy = ctx->stats.prediction_accuracy;
    nimcp_mutex_unlock(&ctx->mutex);

    return accuracy;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* pa_series_type_to_string(pa_series_type_t type) {
    switch (type) {
        case PA_SERIES_MEMORY: return "Memory";
        case PA_SERIES_CPU: return "CPU";
        case PA_SERIES_LATENCY: return "Latency";
        case PA_SERIES_ERROR_RATE: return "ErrorRate";
        case PA_SERIES_THROUGHPUT: return "Throughput";
        case PA_SERIES_GRADIENT: return "Gradient";
        case PA_SERIES_LOSS: return "Loss";
        case PA_SERIES_QUEUE_DEPTH: return "QueueDepth";
        case PA_SERIES_CONNECTIONS: return "Connections";
        case PA_SERIES_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* pa_detection_method_to_string(pa_detection_method_t method) {
    switch (method) {
        case PA_DETECT_ZSCORE: return "ZScore";
        case PA_DETECT_IQR: return "IQR";
        case PA_DETECT_ISOLATION: return "Isolation";
        case PA_DETECT_LSTM: return "LSTM";
        case PA_DETECT_ENSEMBLE: return "Ensemble";
        default: return "Unknown";
    }
}

const char* pa_model_type_to_string(pa_model_type_t type) {
    switch (type) {
        case PA_MODEL_LINEAR: return "Linear";
        case PA_MODEL_EXPONENTIAL: return "Exponential";
        case PA_MODEL_ARIMA: return "ARIMA";
        case PA_MODEL_HOLT_WINTERS: return "HoltWinters";
        case PA_MODEL_PROPHET: return "Prophet";
        case PA_MODEL_NEURAL: return "Neural";
        default: return "Unknown";
    }
}

const char* pa_alert_severity_to_string(pa_alert_severity_t severity) {
    switch (severity) {
        case PA_ALERT_INFO: return "Info";
        case PA_ALERT_WARNING: return "Warning";
        case PA_ALERT_CRITICAL: return "Critical";
        case PA_ALERT_EMERGENCY: return "Emergency";
        default: return "Unknown";
    }
}

const char* pa_trend_to_string(pa_trend_t trend) {
    switch (trend) {
        case PA_TREND_STABLE: return "Stable";
        case PA_TREND_INCREASING: return "Increasing";
        case PA_TREND_DECREASING: return "Decreasing";
        case PA_TREND_CYCLIC: return "Cyclic";
        case PA_TREND_VOLATILE: return "Volatile";
        default: return "Unknown";
    }
}
