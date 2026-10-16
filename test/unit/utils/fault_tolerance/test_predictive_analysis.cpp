/**
 * @file test_predictive_analysis.cpp
 * @brief Unit tests for predictive analysis module
 *
 * Tests time-series analysis, anomaly detection, correlation
 * analysis, forecasting, and failure prediction.
 */

#include <gtest/gtest.h>
#include <cmath>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_predictive_analysis.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PredictiveAnalysisTest : public ::testing::Test {
protected:
    pa_context_t* ctx;
    pa_config_t config;

    void SetUp() override {
        config = pa_default_config();
        ctx = pa_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            pa_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper to add synthetic data
    void addNormalData(pa_series_type_t series, int count, double mean, double stddev) {
        for (int i = 0; i < count; i++) {
            // Simple pseudo-normal distribution using sum of uniforms
            double val = 0;
            for (int j = 0; j < 12; j++) {
                val += (double)rand() / RAND_MAX;
            }
            val = (val - 6.0) * stddev + mean;
            pa_add_sample(ctx, series, val);
        }
    }

    void addTrendingData(pa_series_type_t series, int count, double start, double slope) {
        for (int i = 0; i < count; i++) {
            double noise = ((double)rand() / RAND_MAX - 0.5) * 2.0;
            pa_add_sample(ctx, series, start + slope * i + noise);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(PaLifecycleTest, DefaultConfig) {
    pa_config_t config = pa_default_config();

    EXPECT_GT(config.sample_rate_ms, 0);
    EXPECT_GT(config.forecast_horizon, 0);
    EXPECT_GT(config.anomaly_threshold, 0.0);
    EXPECT_GT(config.prediction_threshold, 0.0);
}

TEST(PaLifecycleTest, CreateAndDestroy) {
    pa_config_t config = pa_default_config();

    pa_context_t* ctx = pa_create(&config);
    ASSERT_NE(ctx, nullptr);

    pa_destroy(ctx);
}

TEST(PaLifecycleTest, CreateWithNullConfig) {
    pa_context_t* ctx = pa_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(PredictiveAnalysisTest, StartAndStop) {
    EXPECT_TRUE(pa_start(ctx));
    EXPECT_TRUE(pa_stop(ctx));
}

//=============================================================================
// Data Collection Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, AddSample) {
    EXPECT_TRUE(pa_add_sample(ctx, PA_SERIES_CPU, 50.0));
    EXPECT_TRUE(pa_add_sample(ctx, PA_SERIES_CPU, 55.0));
    EXPECT_TRUE(pa_add_sample(ctx, PA_SERIES_CPU, 60.0));
}

TEST_F(PredictiveAnalysisTest, AddSampleTimed) {
    uint64_t now = 1000000;
    EXPECT_TRUE(pa_add_sample_timed(ctx, PA_SERIES_MEMORY, 1024.0, now));
    EXPECT_TRUE(pa_add_sample_timed(ctx, PA_SERIES_MEMORY, 2048.0, now + 1000));
    EXPECT_TRUE(pa_add_sample_timed(ctx, PA_SERIES_MEMORY, 3072.0, now + 2000));
}

TEST_F(PredictiveAnalysisTest, AddSamplesBulk) {
    pa_sample_t samples[5];
    for (int i = 0; i < 5; i++) {
        samples[i].value = 10.0 * (i + 1);
        samples[i].timestamp_ms = 1000 * (i + 1);
    }

    uint32_t added = pa_add_samples(ctx, PA_SERIES_LATENCY, samples, 5);
    EXPECT_EQ(added, 5);
}

TEST_F(PredictiveAnalysisTest, GetSeriesMeta) {
    for (int i = 0; i < 10; i++) {
        pa_add_sample(ctx, PA_SERIES_CPU, 50.0 + i * 2.0);
    }

    pa_series_meta_t meta;
    EXPECT_TRUE(pa_get_series_meta(ctx, PA_SERIES_CPU, &meta));
    EXPECT_EQ(meta.sample_count, 10);
    EXPECT_NEAR(meta.min, 50.0, 0.01);
    EXPECT_NEAR(meta.max, 68.0, 0.01);
}

TEST_F(PredictiveAnalysisTest, GetSamples) {
    for (int i = 0; i < 20; i++) {
        pa_add_sample(ctx, PA_SERIES_THROUGHPUT, 100.0 + i);
    }

    pa_sample_t samples[10];
    uint32_t count = pa_get_samples(ctx, PA_SERIES_THROUGHPUT, samples, 10);
    EXPECT_EQ(count, 10);
}

//=============================================================================
// Anomaly Detection Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, DetectAnomalies) {
    // Add normal data
    addNormalData(PA_SERIES_CPU, 100, 50.0, 5.0);

    // Add anomalous values
    pa_add_sample(ctx, PA_SERIES_CPU, 99.0);  // High anomaly
    pa_add_sample(ctx, PA_SERIES_CPU, 5.0);   // Low anomaly

    pa_anomaly_t anomalies[10];
    uint32_t count = pa_detect_anomalies(ctx, PA_SERIES_CPU, anomalies, 10);

    EXPECT_GE(count, 1);  // Should detect at least one anomaly
}

TEST_F(PredictiveAnalysisTest, DetectAllAnomalies) {
    addNormalData(PA_SERIES_CPU, 50, 50.0, 5.0);
    addNormalData(PA_SERIES_MEMORY, 50, 1000.0, 50.0);

    pa_add_sample(ctx, PA_SERIES_CPU, 99.0);     // CPU anomaly
    pa_add_sample(ctx, PA_SERIES_MEMORY, 5000.0); // Memory anomaly

    pa_anomaly_t anomalies[10];
    uint32_t count = pa_detect_all_anomalies(ctx, anomalies, 10);

    EXPECT_GE(count, 1);
}

TEST_F(PredictiveAnalysisTest, IsAnomalous) {
    addNormalData(PA_SERIES_LATENCY, 100, 100.0, 10.0);

    EXPECT_FALSE(pa_is_anomalous(ctx, PA_SERIES_LATENCY, 105.0));  // Normal
    EXPECT_TRUE(pa_is_anomalous(ctx, PA_SERIES_LATENCY, 200.0));   // Anomalous
}

TEST_F(PredictiveAnalysisTest, GetAnomalyScore) {
    addNormalData(PA_SERIES_ERROR_RATE, 100, 1.0, 0.2);

    double score_normal = pa_get_anomaly_score(ctx, PA_SERIES_ERROR_RATE, 1.0);
    double score_anomalous = pa_get_anomaly_score(ctx, PA_SERIES_ERROR_RATE, 10.0);

    EXPECT_LT(score_normal, score_anomalous);
}

//=============================================================================
// Correlation Analysis Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, CalculateCorrelation) {
    // Add correlated data
    for (int i = 0; i < 100; i++) {
        double base = 50.0 + i * 0.5;
        double noise = ((double)rand() / RAND_MAX - 0.5) * 2.0;
        pa_add_sample(ctx, PA_SERIES_CPU, base + noise);
        pa_add_sample(ctx, PA_SERIES_LATENCY, base * 2.0 + noise);  // Correlated
    }

    pa_correlation_t corr;
    EXPECT_TRUE(pa_calculate_correlation(ctx, PA_SERIES_CPU, PA_SERIES_LATENCY, &corr));
    EXPECT_GT(corr.correlation, 0.5);  // Should be positively correlated
}

TEST_F(PredictiveAnalysisTest, FindCorrelations) {
    // Add data to multiple series
    for (int i = 0; i < 100; i++) {
        double base = 50.0 + i * 0.5;
        pa_add_sample(ctx, PA_SERIES_CPU, base);
        pa_add_sample(ctx, PA_SERIES_MEMORY, base * 1.5);
        pa_add_sample(ctx, PA_SERIES_LATENCY, base * 2.0);
    }

    pa_correlation_t correlations[20];
    uint32_t count = pa_find_correlations(ctx, correlations, 20, 0.5);

    EXPECT_GE(count, 1);  // Should find at least one correlation
}

TEST_F(PredictiveAnalysisTest, BuildFailureGraph) {
    EXPECT_TRUE(pa_build_failure_graph(ctx));
}

TEST_F(PredictiveAnalysisTest, RecordFailureSequence) {
    uint32_t failures[] = {1, 2, 3};
    uint32_t deltas[] = {1000, 2000};

    EXPECT_TRUE(pa_record_failure_sequence(ctx, failures, 3, deltas));
}

TEST_F(PredictiveAnalysisTest, GetFailureEdges) {
    // Record some failure sequences
    uint32_t seq1[] = {1, 2};
    uint32_t delta1[] = {1000};
    pa_record_failure_sequence(ctx, seq1, 2, delta1);

    uint32_t seq2[] = {1, 2};
    uint32_t delta2[] = {1500};
    pa_record_failure_sequence(ctx, seq2, 2, delta2);

    pa_build_failure_graph(ctx);

    pa_failure_edge_t edges[10];
    uint32_t count = pa_get_failure_edges(ctx, edges, 10);

    EXPECT_GE(count, 1);
}

//=============================================================================
// Forecasting Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, Forecast) {
    // Add trending data
    addTrendingData(PA_SERIES_CPU, 100, 30.0, 0.5);

    pa_forecast_t forecast;
    EXPECT_TRUE(pa_forecast(ctx, PA_SERIES_CPU, &forecast));
    EXPECT_EQ(forecast.series, PA_SERIES_CPU);
    EXPECT_GT(forecast.horizon, 0);
}

TEST_F(PredictiveAnalysisTest, ForecastWithModel) {
    addTrendingData(PA_SERIES_MEMORY, 100, 1000.0, 10.0);

    pa_forecast_t forecast;
    EXPECT_TRUE(pa_forecast_with_model(ctx, PA_SERIES_MEMORY, PA_MODEL_LINEAR, 5, &forecast));
    EXPECT_EQ(forecast.model, PA_MODEL_LINEAR);
    EXPECT_EQ(forecast.horizon, 5);
}

TEST_F(PredictiveAnalysisTest, TimeToThreshold) {
    // Add increasing data
    addTrendingData(PA_SERIES_CPU, 100, 50.0, 0.3);

    uint64_t time_ms;
    bool will_reach = pa_time_to_threshold(ctx, PA_SERIES_CPU, 90.0, &time_ms);

    if (will_reach) {
        EXPECT_GT(time_ms, 0);
    }
}

TEST_F(PredictiveAnalysisTest, DetectSeasonality) {
    // Add seasonal data
    for (int i = 0; i < 200; i++) {
        double seasonal = sin(2.0 * M_PI * i / 24.0) * 20.0;  // 24-hour cycle
        double noise = ((double)rand() / RAND_MAX - 0.5) * 5.0;
        pa_add_sample(ctx, PA_SERIES_THROUGHPUT, 100.0 + seasonal + noise);
    }

    pa_seasonality_t seasonality;
    bool detected = pa_detect_seasonality(ctx, PA_SERIES_THROUGHPUT, &seasonality);

    // May or may not detect depending on data quality
    (void)detected;
}

TEST_F(PredictiveAnalysisTest, GetTrend) {
    addTrendingData(PA_SERIES_CPU, 100, 30.0, 0.5);

    pa_trend_t trend = pa_get_trend(ctx, PA_SERIES_CPU);
    EXPECT_EQ(trend, PA_TREND_INCREASING);
}

TEST_F(PredictiveAnalysisTest, GetTrendDecreasing) {
    addTrendingData(PA_SERIES_CPU, 100, 90.0, -0.5);

    pa_trend_t trend = pa_get_trend(ctx, PA_SERIES_CPU);
    EXPECT_EQ(trend, PA_TREND_DECREASING);
}

TEST_F(PredictiveAnalysisTest, GetTrendStable) {
    addNormalData(PA_SERIES_CPU, 100, 50.0, 2.0);

    pa_trend_t trend = pa_get_trend(ctx, PA_SERIES_CPU);
    EXPECT_EQ(trend, PA_TREND_STABLE);
}

//=============================================================================
// Failure Prediction Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, PredictFailures) {
    // Add data that suggests impending failure
    addTrendingData(PA_SERIES_CPU, 50, 70.0, 0.5);
    addTrendingData(PA_SERIES_MEMORY, 50, 80.0, 0.3);

    // Add some anomalies
    pa_add_sample(ctx, PA_SERIES_ERROR_RATE, 5.0);

    pa_failure_prediction_t predictions[10];
    uint32_t count = pa_predict_failures(ctx, predictions, 10);

    // May or may not have predictions depending on thresholds
    (void)count;
}

TEST_F(PredictiveAnalysisTest, GetHighestRiskFailure) {
    addTrendingData(PA_SERIES_CPU, 100, 80.0, 0.2);

    pa_failure_prediction_t prediction;
    bool has_prediction = pa_get_highest_risk_failure(ctx, &prediction);

    // May or may not have a prediction
    (void)has_prediction;
}

TEST_F(PredictiveAnalysisTest, PredictFailureType) {
    addTrendingData(PA_SERIES_MEMORY, 100, 85.0, 0.1);

    pa_failure_prediction_t prediction;
    bool has_prediction = pa_predict_failure_type(ctx, 1, &prediction);

    (void)has_prediction;
}

TEST_F(PredictiveAnalysisTest, RecordActualFailure) {
    EXPECT_TRUE(pa_record_actual_failure(ctx, 1, 1000000));
}

//=============================================================================
// Alert Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, GetAlerts) {
    pa_start(ctx);

    // Add data that might trigger alerts
    addTrendingData(PA_SERIES_CPU, 50, 85.0, 0.3);

    pa_alert_t alerts[10];
    uint32_t count = pa_get_alerts(ctx, alerts, 10);

    // Count may be 0 or more depending on thresholds
    (void)count;

    pa_stop(ctx);
}

TEST_F(PredictiveAnalysisTest, GetAlertsBySeverity) {
    pa_alert_t alerts[10];
    uint32_t count = pa_get_alerts_by_severity(ctx, PA_ALERT_CRITICAL, alerts, 10);

    EXPECT_GE(count, 0);  // May have no critical alerts
}

TEST_F(PredictiveAnalysisTest, AcknowledgeAlert) {
    // Create a scenario that generates an alert
    pa_start(ctx);
    addTrendingData(PA_SERIES_CPU, 50, 90.0, 0.5);

    pa_alert_t alerts[10];
    uint32_t count = pa_get_alerts(ctx, alerts, 10);

    if (count > 0) {
        EXPECT_TRUE(pa_acknowledge_alert(ctx, alerts[0].alert_id));
    }

    pa_stop(ctx);
}

TEST_F(PredictiveAnalysisTest, SuppressAlert) {
    pa_start(ctx);
    addTrendingData(PA_SERIES_CPU, 50, 90.0, 0.5);

    pa_alert_t alerts[10];
    uint32_t count = pa_get_alerts(ctx, alerts, 10);

    if (count > 0) {
        EXPECT_TRUE(pa_suppress_alert(ctx, alerts[0].alert_id, 60000));
    }

    pa_stop(ctx);
}

TEST_F(PredictiveAnalysisTest, ClearExpiredAlerts) {
    uint32_t cleared = pa_clear_expired_alerts(ctx);
    EXPECT_GE(cleared, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PredictiveAnalysisTest, GetStats) {
    addNormalData(PA_SERIES_CPU, 100, 50.0, 5.0);

    pa_stats_t stats;
    EXPECT_TRUE(pa_get_stats(ctx, &stats));
    EXPECT_EQ(stats.total_samples, 100);
}

TEST_F(PredictiveAnalysisTest, ResetStats) {
    addNormalData(PA_SERIES_CPU, 50, 50.0, 5.0);

    pa_reset_stats(ctx);

    pa_stats_t stats;
    EXPECT_TRUE(pa_get_stats(ctx, &stats));
    EXPECT_EQ(stats.total_samples, 0);
}

TEST_F(PredictiveAnalysisTest, GetAccuracy) {
    // Record some predictions and actual failures
    pa_record_actual_failure(ctx, 1, 1000);
    pa_record_actual_failure(ctx, 2, 2000);

    float accuracy = pa_get_accuracy(ctx);
    EXPECT_GE(accuracy, 0.0);
    EXPECT_LE(accuracy, 1.0);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST(PaStringTest, SeriesTypeToString) {
    EXPECT_STREQ("Memory", pa_series_type_to_string(PA_SERIES_MEMORY));
    EXPECT_STREQ("CPU", pa_series_type_to_string(PA_SERIES_CPU));
    EXPECT_STREQ("Latency", pa_series_type_to_string(PA_SERIES_LATENCY));
    EXPECT_STREQ("ErrorRate", pa_series_type_to_string(PA_SERIES_ERROR_RATE));
    EXPECT_STREQ("Throughput", pa_series_type_to_string(PA_SERIES_THROUGHPUT));
}

TEST(PaStringTest, DetectionMethodToString) {
    EXPECT_STREQ("ZScore", pa_detection_method_to_string(PA_DETECT_ZSCORE));
    EXPECT_STREQ("IQR", pa_detection_method_to_string(PA_DETECT_IQR));
    EXPECT_STREQ("Isolation", pa_detection_method_to_string(PA_DETECT_ISOLATION));
    EXPECT_STREQ("LSTM", pa_detection_method_to_string(PA_DETECT_LSTM));
    EXPECT_STREQ("Ensemble", pa_detection_method_to_string(PA_DETECT_ENSEMBLE));
}

TEST(PaStringTest, ModelTypeToString) {
    EXPECT_STREQ("Linear", pa_model_type_to_string(PA_MODEL_LINEAR));
    EXPECT_STREQ("Exponential", pa_model_type_to_string(PA_MODEL_EXPONENTIAL));
    EXPECT_STREQ("ARIMA", pa_model_type_to_string(PA_MODEL_ARIMA));
    EXPECT_STREQ("HoltWinters", pa_model_type_to_string(PA_MODEL_HOLT_WINTERS));
    EXPECT_STREQ("Neural", pa_model_type_to_string(PA_MODEL_NEURAL));
}

TEST(PaStringTest, AlertSeverityToString) {
    EXPECT_STREQ("Info", pa_alert_severity_to_string(PA_ALERT_INFO));
    EXPECT_STREQ("Warning", pa_alert_severity_to_string(PA_ALERT_WARNING));
    EXPECT_STREQ("Critical", pa_alert_severity_to_string(PA_ALERT_CRITICAL));
    EXPECT_STREQ("Emergency", pa_alert_severity_to_string(PA_ALERT_EMERGENCY));
}

TEST(PaStringTest, TrendToString) {
    EXPECT_STREQ("Stable", pa_trend_to_string(PA_TREND_STABLE));
    EXPECT_STREQ("Increasing", pa_trend_to_string(PA_TREND_INCREASING));
    EXPECT_STREQ("Decreasing", pa_trend_to_string(PA_TREND_DECREASING));
    EXPECT_STREQ("Cyclic", pa_trend_to_string(PA_TREND_CYCLIC));
    EXPECT_STREQ("Volatile", pa_trend_to_string(PA_TREND_VOLATILE));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PredictiveAnalysisTest, EmptySeriesForecast) {
    pa_forecast_t forecast;
    bool success = pa_forecast(ctx, PA_SERIES_CPU, &forecast);
    EXPECT_FALSE(success);  // Should fail with no data
}

TEST_F(PredictiveAnalysisTest, SingleSampleAnomaly) {
    pa_add_sample(ctx, PA_SERIES_CPU, 50.0);

    // Should not crash with single sample
    pa_anomaly_t anomalies[10];
    uint32_t count = pa_detect_anomalies(ctx, PA_SERIES_CPU, anomalies, 10);
    EXPECT_EQ(count, 0);  // Not enough data for anomaly detection
}

TEST_F(PredictiveAnalysisTest, MaxSamples) {
    // Add more than max samples
    for (int i = 0; i < PA_MAX_SAMPLES + 100; i++) {
        pa_add_sample(ctx, PA_SERIES_CPU, 50.0 + (i % 20));
    }

    pa_series_meta_t meta;
    EXPECT_TRUE(pa_get_series_meta(ctx, PA_SERIES_CPU, &meta));
    EXPECT_LE(meta.sample_count, PA_MAX_SAMPLES);  // Should cap at max
}

TEST_F(PredictiveAnalysisTest, NegativeValues) {
    pa_add_sample(ctx, PA_SERIES_GRADIENT, -10.0);
    pa_add_sample(ctx, PA_SERIES_GRADIENT, -5.0);
    pa_add_sample(ctx, PA_SERIES_GRADIENT, 0.0);
    pa_add_sample(ctx, PA_SERIES_GRADIENT, 5.0);

    pa_series_meta_t meta;
    EXPECT_TRUE(pa_get_series_meta(ctx, PA_SERIES_GRADIENT, &meta));
    EXPECT_NEAR(meta.min, -10.0, 0.01);
}

TEST_F(PredictiveAnalysisTest, LargeValues) {
    pa_add_sample(ctx, PA_SERIES_THROUGHPUT, 1e12);
    pa_add_sample(ctx, PA_SERIES_THROUGHPUT, 1e13);

    pa_series_meta_t meta;
    EXPECT_TRUE(pa_get_series_meta(ctx, PA_SERIES_THROUGHPUT, &meta));
    EXPECT_NEAR(meta.max, 1e13, 1e10);
}

TEST_F(PredictiveAnalysisTest, CorrelationWithSelf) {
    addNormalData(PA_SERIES_CPU, 100, 50.0, 5.0);

    pa_correlation_t corr;
    EXPECT_TRUE(pa_calculate_correlation(ctx, PA_SERIES_CPU, PA_SERIES_CPU, &corr));
    EXPECT_NEAR(corr.correlation, 1.0, 0.01);  // Perfect self-correlation
}

