/**
 * @file nimcp_predictive_analysis.h
 * @brief Enhanced Predictive Failure Analysis Module
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
 * ANALYSIS METHODS:
 * 1. Time-Series Anomaly Detection (ARIMA-like patterns)
 * 2. Failure Correlation Graphs (Bayesian networks)
 * 3. Resource Forecasting (trend extrapolation)
 * 4. Ensemble Prediction (multiple models)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_ANALYSIS_H
#define NIMCP_PREDICTIVE_ANALYSIS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define PA_MAX_SERIES 32                    /**< Max time series to track */
#define PA_MAX_SAMPLES 4096                 /**< Samples per series */
#define PA_MAX_CORRELATIONS 64              /**< Max correlation pairs */
#define PA_MAX_MODELS 8                     /**< Max prediction models */
#define PA_MAX_ALERTS 32                    /**< Max active alerts */
#define PA_FORECAST_HORIZON 10              /**< Default forecast steps */
#define PA_SEASONALITY_MAX 168              /**< Max seasonality period (1 week @ hourly) */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Time series types
 */
typedef enum {
    PA_SERIES_MEMORY = 0,       /**< Memory usage */
    PA_SERIES_CPU,              /**< CPU utilization */
    PA_SERIES_LATENCY,          /**< Response latency */
    PA_SERIES_ERROR_RATE,       /**< Error rate */
    PA_SERIES_THROUGHPUT,       /**< Processing throughput */
    PA_SERIES_GRADIENT,         /**< Gradient magnitude */
    PA_SERIES_LOSS,             /**< Training loss */
    PA_SERIES_QUEUE_DEPTH,      /**< Queue depth */
    PA_SERIES_CONNECTIONS,      /**< Active connections */
    PA_SERIES_CUSTOM            /**< Custom series */
} pa_series_type_t;

/**
 * @brief Anomaly detection methods
 */
typedef enum {
    PA_DETECT_ZSCORE = 0,       /**< Z-score threshold */
    PA_DETECT_IQR,              /**< Interquartile range */
    PA_DETECT_ISOLATION,        /**< Isolation forest */
    PA_DETECT_LSTM,             /**< LSTM-style pattern */
    PA_DETECT_ENSEMBLE          /**< Ensemble of methods */
} pa_detection_method_t;

/**
 * @brief Prediction model types
 */
typedef enum {
    PA_MODEL_LINEAR = 0,        /**< Linear regression */
    PA_MODEL_EXPONENTIAL,       /**< Exponential smoothing */
    PA_MODEL_ARIMA,             /**< ARIMA-like */
    PA_MODEL_HOLT_WINTERS,      /**< Holt-Winters seasonal */
    PA_MODEL_PROPHET,           /**< Facebook Prophet-like */
    PA_MODEL_NEURAL             /**< Neural network */
} pa_model_type_t;

/**
 * @brief Alert severity
 */
typedef enum {
    PA_ALERT_INFO = 0,          /**< Informational */
    PA_ALERT_WARNING,           /**< Warning condition */
    PA_ALERT_CRITICAL,          /**< Critical condition */
    PA_ALERT_EMERGENCY          /**< Immediate action required */
} pa_alert_severity_t;

/**
 * @brief Trend direction
 */
typedef enum {
    PA_TREND_STABLE = 0,        /**< No significant trend */
    PA_TREND_INCREASING,        /**< Increasing trend */
    PA_TREND_DECREASING,        /**< Decreasing trend */
    PA_TREND_CYCLIC,            /**< Cyclic pattern */
    PA_TREND_VOLATILE           /**< High volatility */
} pa_trend_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Time series sample
 */
typedef struct {
    double value;               /**< Sample value */
    uint64_t timestamp_ms;      /**< Sample timestamp */
} pa_sample_t;

/**
 * @brief Time series metadata
 */
typedef struct {
    char name[64];              /**< Series name */
    pa_series_type_t type;      /**< Series type */
    uint32_t sample_count;      /**< Current samples */
    uint32_t max_samples;       /**< Maximum samples */
    double mean;                /**< Running mean */
    double variance;            /**< Running variance */
    double min;                 /**< Minimum value */
    double max;                 /**< Maximum value */
    pa_trend_t trend;           /**< Current trend */
    uint64_t first_sample_ms;   /**< First sample time */
    uint64_t last_sample_ms;    /**< Last sample time */
} pa_series_meta_t;

/**
 * @brief Detected anomaly
 */
typedef struct {
    pa_series_type_t series;    /**< Affected series */
    double value;               /**< Anomalous value */
    double expected;            /**< Expected value */
    double deviation;           /**< Standard deviations */
    pa_detection_method_t method; /**< Detection method */
    float confidence;           /**< Detection confidence (0-1) */
    uint64_t detected_at_ms;    /**< Detection timestamp */
    char description[256];      /**< Human description */
} pa_anomaly_t;

/**
 * @brief Correlation between series
 */
typedef struct {
    pa_series_type_t series_a;  /**< First series */
    pa_series_type_t series_b;  /**< Second series */
    double correlation;         /**< Pearson correlation (-1 to 1) */
    int32_t lag;                /**< Time lag (samples) */
    bool is_causal;             /**< series_a causes series_b */
    float confidence;           /**< Confidence in causality */
} pa_correlation_t;

/**
 * @brief Failure correlation graph edge
 */
typedef struct {
    uint32_t source_failure;    /**< Source failure type */
    uint32_t target_failure;    /**< Target failure type */
    float probability;          /**< Conditional probability */
    uint32_t time_lag_ms;       /**< Typical time between */
    uint32_t occurrence_count;  /**< Times observed */
} pa_failure_edge_t;

/**
 * @brief Resource forecast
 */
typedef struct {
    pa_series_type_t series;    /**< Forecasted series */
    double forecasts[PA_FORECAST_HORIZON]; /**< Predicted values */
    double lower_bound[PA_FORECAST_HORIZON]; /**< Lower confidence */
    double upper_bound[PA_FORECAST_HORIZON]; /**< Upper confidence */
    uint32_t horizon;           /**< Forecast horizon */
    pa_model_type_t model;      /**< Model used */
    float confidence;           /**< Overall confidence */
    uint64_t forecast_time_ms;  /**< When forecast made */
    uint64_t target_time_ms;    /**< When values expected */
} pa_forecast_t;

/**
 * @brief Failure prediction
 */
typedef struct {
    uint32_t failure_type;      /**< Predicted failure type */
    float probability;          /**< Failure probability (0-1) */
    uint64_t predicted_time_ms; /**< When failure expected */
    uint64_t confidence_window_ms; /**< Time uncertainty */
    pa_series_type_t trigger_series; /**< Series triggering */
    double trigger_threshold;   /**< Threshold that triggers */
    float confidence;           /**< Prediction confidence */
    char reasoning[256];        /**< Explanation */
    pa_anomaly_t anomalies[4];  /**< Related anomalies */
    uint32_t anomaly_count;     /**< Number of anomalies */
} pa_failure_prediction_t;

/**
 * @brief Predictive alert
 */
typedef struct {
    uint32_t alert_id;          /**< Alert identifier */
    pa_alert_severity_t severity; /**< Alert severity */
    pa_failure_prediction_t prediction; /**< Associated prediction */
    uint64_t created_at_ms;     /**< Alert creation time */
    uint64_t expires_at_ms;     /**< Alert expiration */
    bool acknowledged;          /**< Has been acknowledged */
    bool suppressed;            /**< Is suppressed */
    char action_required[256];  /**< Recommended action */
} pa_alert_t;

/**
 * @brief Seasonality component
 */
typedef struct {
    uint32_t period;            /**< Seasonality period (samples) */
    double* pattern;            /**< Seasonal pattern */
    float strength;             /**< Seasonality strength (0-1) */
} pa_seasonality_t;

/**
 * @brief Configuration for predictive analysis
 */
typedef struct {
    pa_detection_method_t detection_method; /**< Anomaly detection */
    pa_model_type_t forecast_model;     /**< Forecasting model */
    uint32_t sample_rate_ms;            /**< Sampling interval */
    uint32_t forecast_horizon;          /**< Forecast steps */
    float anomaly_threshold;            /**< Z-score threshold */
    float prediction_threshold;         /**< Min probability to alert */
    uint32_t correlation_window;        /**< Correlation window (samples) */
    uint32_t seasonality_period;        /**< Expected seasonality */
    bool enable_correlation_graph;      /**< Build failure graph */
    bool enable_ensemble;               /**< Use ensemble prediction */
    bool enable_auto_alerts;            /**< Auto-generate alerts */
} pa_config_t;

/**
 * @brief Statistics for predictive analysis
 */
typedef struct {
    uint64_t total_samples;
    uint64_t anomalies_detected;
    uint64_t predictions_made;
    uint64_t predictions_correct;
    uint64_t false_positives;
    uint64_t false_negatives;
    float prediction_accuracy;
    float precision;
    float recall;
    float f1_score;
    double avg_prediction_lead_time_ms;
} pa_stats_t;

/**
 * @brief Opaque predictive analysis handle
 */
typedef struct pa_context pa_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create predictive analysis context
 *
 * WHAT: Initialize predictive analysis system
 * WHY:  Required before any analysis
 * HOW:  Allocate buffers, initialize models
 *
 * @param config Configuration
 * @return PA context or NULL on failure
 */
pa_context_t* pa_create(const pa_config_t* config);

/**
 * @brief Destroy predictive analysis context
 *
 * @param ctx PA context
 */
void pa_destroy(pa_context_t* ctx);

/**
 * @brief Get default configuration
 *
 * @return Default configuration
 */
pa_config_t pa_default_config(void);

/**
 * @brief Start analysis
 *
 * @param ctx PA context
 * @return true on success
 */
bool pa_start(pa_context_t* ctx);

/**
 * @brief Stop analysis
 *
 * @param ctx PA context
 * @return true on success
 */
bool pa_stop(pa_context_t* ctx);

//=============================================================================
// Data Collection
//=============================================================================

/**
 * @brief Add sample to time series
 *
 * @param ctx PA context
 * @param series Series type
 * @param value Sample value
 * @return true on success
 */
bool pa_add_sample(pa_context_t* ctx, pa_series_type_t series, double value);

/**
 * @brief Add sample with timestamp
 *
 * @param ctx PA context
 * @param series Series type
 * @param value Sample value
 * @param timestamp_ms Sample timestamp
 * @return true on success
 */
bool pa_add_sample_timed(pa_context_t* ctx, pa_series_type_t series, double value, uint64_t timestamp_ms);

/**
 * @brief Bulk add samples
 *
 * @param ctx PA context
 * @param series Series type
 * @param samples Sample array
 * @param count Number of samples
 * @return Number successfully added
 */
uint32_t pa_add_samples(pa_context_t* ctx, pa_series_type_t series, const pa_sample_t* samples, uint32_t count);

/**
 * @brief Get series metadata
 *
 * @param ctx PA context
 * @param series Series type
 * @param meta Output metadata
 * @return true on success
 */
bool pa_get_series_meta(pa_context_t* ctx, pa_series_type_t series, pa_series_meta_t* meta);

/**
 * @brief Get recent samples
 *
 * @param ctx PA context
 * @param series Series type
 * @param samples Output array
 * @param count Max samples
 * @return Number of samples
 */
uint32_t pa_get_samples(pa_context_t* ctx, pa_series_type_t series, pa_sample_t* samples, uint32_t count);

//=============================================================================
// Anomaly Detection
//=============================================================================

/**
 * @brief Detect anomalies in series
 *
 * @param ctx PA context
 * @param series Series to analyze
 * @param anomalies Output array
 * @param max_anomalies Array capacity
 * @return Number of anomalies detected
 */
uint32_t pa_detect_anomalies(
    pa_context_t* ctx,
    pa_series_type_t series,
    pa_anomaly_t* anomalies,
    uint32_t max_anomalies
);

/**
 * @brief Detect anomalies in all series
 *
 * @param ctx PA context
 * @param anomalies Output array
 * @param max_anomalies Array capacity
 * @return Number of anomalies detected
 */
uint32_t pa_detect_all_anomalies(pa_context_t* ctx, pa_anomaly_t* anomalies, uint32_t max_anomalies);

/**
 * @brief Check if value is anomalous
 *
 * @param ctx PA context
 * @param series Series type
 * @param value Value to check
 * @return true if anomalous
 */
bool pa_is_anomalous(pa_context_t* ctx, pa_series_type_t series, double value);

/**
 * @brief Get anomaly score
 *
 * @param ctx PA context
 * @param series Series type
 * @param value Value to score
 * @return Anomaly score (higher = more anomalous)
 */
double pa_get_anomaly_score(pa_context_t* ctx, pa_series_type_t series, double value);

//=============================================================================
// Correlation Analysis
//=============================================================================

/**
 * @brief Calculate correlation between series
 *
 * @param ctx PA context
 * @param series_a First series
 * @param series_b Second series
 * @param correlation Output correlation
 * @return true on success
 */
bool pa_calculate_correlation(
    pa_context_t* ctx,
    pa_series_type_t series_a,
    pa_series_type_t series_b,
    pa_correlation_t* correlation
);

/**
 * @brief Find all significant correlations
 *
 * @param ctx PA context
 * @param correlations Output array
 * @param max_correlations Array capacity
 * @param min_correlation Minimum |correlation| to include
 * @return Number of correlations found
 */
uint32_t pa_find_correlations(
    pa_context_t* ctx,
    pa_correlation_t* correlations,
    uint32_t max_correlations,
    float min_correlation
);

/**
 * @brief Build failure correlation graph
 *
 * @param ctx PA context
 * @return true on success
 */
bool pa_build_failure_graph(pa_context_t* ctx);

/**
 * @brief Get failure graph edges
 *
 * @param ctx PA context
 * @param edges Output array
 * @param max_edges Array capacity
 * @return Number of edges
 */
uint32_t pa_get_failure_edges(pa_context_t* ctx, pa_failure_edge_t* edges, uint32_t max_edges);

/**
 * @brief Record observed failure sequence
 *
 * @param ctx PA context
 * @param failure_types Sequence of failure types
 * @param count Number of failures
 * @param time_deltas_ms Time between failures
 * @return true on success
 */
bool pa_record_failure_sequence(
    pa_context_t* ctx,
    const uint32_t* failure_types,
    uint32_t count,
    const uint32_t* time_deltas_ms
);

//=============================================================================
// Forecasting
//=============================================================================

/**
 * @brief Forecast series values
 *
 * @param ctx PA context
 * @param series Series to forecast
 * @param forecast Output forecast
 * @return true on success
 */
bool pa_forecast(pa_context_t* ctx, pa_series_type_t series, pa_forecast_t* forecast);

/**
 * @brief Forecast with specific model
 *
 * @param ctx PA context
 * @param series Series to forecast
 * @param model Model to use
 * @param horizon Forecast steps
 * @param forecast Output forecast
 * @return true on success
 */
bool pa_forecast_with_model(
    pa_context_t* ctx,
    pa_series_type_t series,
    pa_model_type_t model,
    uint32_t horizon,
    pa_forecast_t* forecast
);

/**
 * @brief Estimate time to threshold
 *
 * @param ctx PA context
 * @param series Series to analyze
 * @param threshold Threshold value
 * @param time_ms Output: estimated time to threshold
 * @return true if threshold will be reached
 */
bool pa_time_to_threshold(
    pa_context_t* ctx,
    pa_series_type_t series,
    double threshold,
    uint64_t* time_ms
);

/**
 * @brief Detect seasonality
 *
 * @param ctx PA context
 * @param series Series to analyze
 * @param seasonality Output seasonality
 * @return true if seasonality detected
 */
bool pa_detect_seasonality(pa_context_t* ctx, pa_series_type_t series, pa_seasonality_t* seasonality);

/**
 * @brief Get current trend
 *
 * @param ctx PA context
 * @param series Series to analyze
 * @return Current trend
 */
pa_trend_t pa_get_trend(pa_context_t* ctx, pa_series_type_t series);

//=============================================================================
// Failure Prediction
//=============================================================================

/**
 * @brief Predict failures
 *
 * @param ctx PA context
 * @param predictions Output array
 * @param max_predictions Array capacity
 * @return Number of predictions
 */
uint32_t pa_predict_failures(pa_context_t* ctx, pa_failure_prediction_t* predictions, uint32_t max_predictions);

/**
 * @brief Get highest probability failure
 *
 * @param ctx PA context
 * @param prediction Output prediction
 * @return true if prediction exists
 */
bool pa_get_highest_risk_failure(pa_context_t* ctx, pa_failure_prediction_t* prediction);

/**
 * @brief Predict specific failure type
 *
 * @param ctx PA context
 * @param failure_type Failure type
 * @param prediction Output prediction
 * @return true if prediction available
 */
bool pa_predict_failure_type(pa_context_t* ctx, uint32_t failure_type, pa_failure_prediction_t* prediction);

/**
 * @brief Update prediction after failure occurred
 *
 * @param ctx PA context
 * @param failure_type Actual failure type
 * @param timestamp_ms When failure occurred
 * @return true on success
 */
bool pa_record_actual_failure(pa_context_t* ctx, uint32_t failure_type, uint64_t timestamp_ms);

//=============================================================================
// Alerts
//=============================================================================

/**
 * @brief Get active alerts
 *
 * @param ctx PA context
 * @param alerts Output array
 * @param max_alerts Array capacity
 * @return Number of alerts
 */
uint32_t pa_get_alerts(pa_context_t* ctx, pa_alert_t* alerts, uint32_t max_alerts);

/**
 * @brief Get alerts by severity
 *
 * @param ctx PA context
 * @param severity Minimum severity
 * @param alerts Output array
 * @param max_alerts Array capacity
 * @return Number of alerts
 */
uint32_t pa_get_alerts_by_severity(
    pa_context_t* ctx,
    pa_alert_severity_t severity,
    pa_alert_t* alerts,
    uint32_t max_alerts
);

/**
 * @brief Acknowledge alert
 *
 * @param ctx PA context
 * @param alert_id Alert to acknowledge
 * @return true on success
 */
bool pa_acknowledge_alert(pa_context_t* ctx, uint32_t alert_id);

/**
 * @brief Suppress alert
 *
 * @param ctx PA context
 * @param alert_id Alert to suppress
 * @param duration_ms Suppression duration
 * @return true on success
 */
bool pa_suppress_alert(pa_context_t* ctx, uint32_t alert_id, uint64_t duration_ms);

/**
 * @brief Clear expired alerts
 *
 * @param ctx PA context
 * @return Number cleared
 */
uint32_t pa_clear_expired_alerts(pa_context_t* ctx);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get analysis statistics
 *
 * @param ctx PA context
 * @param stats Output statistics
 * @return true on success
 */
bool pa_get_stats(pa_context_t* ctx, pa_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param ctx PA context
 */
void pa_reset_stats(pa_context_t* ctx);

/**
 * @brief Get prediction accuracy
 *
 * @param ctx PA context
 * @return Accuracy (0-1)
 */
float pa_get_accuracy(pa_context_t* ctx);

//=============================================================================
// String Conversion
//=============================================================================

const char* pa_series_type_to_string(pa_series_type_t type);
const char* pa_detection_method_to_string(pa_detection_method_t method);
const char* pa_model_type_to_string(pa_model_type_t type);
const char* pa_alert_severity_to_string(pa_alert_severity_t severity);
const char* pa_trend_to_string(pa_trend_t trend);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PREDICTIVE_ANALYSIS_H
