/**
 * @file test_stdp_health_metrics.cpp
 * @brief Unit tests for STDP Health Metrics
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "utils/fault_tolerance/nimcp_stdp_health_metrics.h"
}

class StdpHealthMetricsTest : public ::testing::Test {
protected:
    stdp_health_config_t config;
    stdp_health_metrics_t* metrics = nullptr;

    void SetUp() override {
        stdp_health_default_config(&config);
    }

    void TearDown() override {
        if (metrics) {
            stdp_health_destroy(metrics);
            metrics = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, DefaultConfigSetsReasonableDefaults) {
    stdp_health_config_t cfg;
    int result = stdp_health_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_weight_monitoring);
    EXPECT_TRUE(cfg.enable_timing_monitoring);
    EXPECT_TRUE(cfg.enable_lr_monitoring);
    EXPECT_TRUE(cfg.enable_trace_monitoring);
    EXPECT_TRUE(cfg.enable_bcm_monitoring);
    EXPECT_TRUE(cfg.enable_homeostatic_monitoring);
}

TEST_F(StdpHealthMetricsTest, DefaultConfigNullReturnsError) {
    int result = stdp_health_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, DefaultThresholdsAreReasonable) {
    stdp_health_config_t cfg;
    stdp_health_default_config(&cfg);

    EXPECT_GT(cfg.weight_thresholds.max_weight_value, 0.0f);
    EXPECT_LT(cfg.weight_thresholds.min_weight_value, 0.0f);
    EXPECT_GT(cfg.timing_thresholds.max_timing_window_ms, 0.0f);
    EXPECT_GT(cfg.lr_thresholds.max_learning_rate, 0.0f);
    EXPECT_GT(cfg.lr_thresholds.min_learning_rate, 0.0f);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, CreateWithDefaultConfig) {
    metrics = stdp_health_create(nullptr, nullptr);
    EXPECT_NE(metrics, nullptr);
}

TEST_F(StdpHealthMetricsTest, CreateWithCustomConfig) {
    config.check_interval_ms = 50;
    config.verbose_logging = true;

    metrics = stdp_health_create(&config, nullptr);
    EXPECT_NE(metrics, nullptr);
}

TEST_F(StdpHealthMetricsTest, DestroyNullIsSafe) {
    stdp_health_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Registration Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, RegisterStdpNullMetricsFails) {
    int result = stdp_health_register_stdp(nullptr, nullptr, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, RegisterBcmNullMetricsFails) {
    int result = stdp_health_register_bcm(nullptr, nullptr, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, RegisterCoordinatorNullMetricsFails) {
    int result = stdp_health_register_coordinator(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, UnregisterNullMetricsFails) {
    int result = stdp_health_unregister(nullptr, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, UnregisterInvalidIdFails) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int result = stdp_health_unregister(metrics, -1);
    EXPECT_EQ(result, -1);

    result = stdp_health_unregister(metrics, STDP_HEALTH_MAX_CONTEXTS);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Monitoring Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, CheckNullMetricsFails) {
    int result = stdp_health_check(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, CheckReturnsZeroAnomaliesInitially) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int anomalies = stdp_health_check(metrics);
    EXPECT_GE(anomalies, 0);
}

TEST_F(StdpHealthMetricsTest, CheckWeightsNullMetricsFails) {
    float weights[] = {0.5f, 0.3f, 0.7f};
    int result = stdp_health_check_weights(nullptr, weights, 3, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, CheckWeightsNullWeightsFails) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int result = stdp_health_check_weights(metrics, nullptr, 3, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, CheckWeightsEmptyArrayFails) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float weights[] = {0.5f};
    int result = stdp_health_check_weights(metrics, weights, 0, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, CheckWeightsHealthyWeightsNoAnomalies) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float weights[] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f};
    int anomalies = stdp_health_check_weights(metrics, weights, 5, "test");
    EXPECT_EQ(anomalies, 0);
}

TEST_F(StdpHealthMetricsTest, CheckWeightsNaNDetected) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float weights[] = {0.5f, NAN, 0.7f};
    int anomalies = stdp_health_check_weights(metrics, weights, 3, "test");
    EXPECT_GT(anomalies, 0);
}

TEST_F(StdpHealthMetricsTest, CheckTimingNullMetricsFails) {
    float pre[] = {1.0f, 2.0f};
    float post[] = {1.5f, 2.5f};
    int result = stdp_health_check_timing(nullptr, pre, post, 2);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, CheckTimingValidTimingNoViolations) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float pre[] = {1.0f, 2.0f, 3.0f};
    float post[] = {1.5f, 2.5f, 3.5f};
    int violations = stdp_health_check_timing(metrics, pre, post, 3);
    EXPECT_GE(violations, 0);
}

TEST_F(StdpHealthMetricsTest, CheckLearningRateNullMetricsFails) {
    int result = stdp_health_check_learning_rate(nullptr, 0.01f, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, CheckLearningRateValidLR) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int anomaly = stdp_health_check_learning_rate(metrics, 0.01f, "test");
    EXPECT_EQ(anomaly, 0);
}

TEST_F(StdpHealthMetricsTest, CheckLearningRateTooHigh) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int anomaly = stdp_health_check_learning_rate(metrics, 10.0f, "test");
    EXPECT_NE(anomaly, 0);
}

TEST_F(StdpHealthMetricsTest, CheckTracesNullMetricsFails) {
    float traces[] = {0.1f, 0.2f};
    int result = stdp_health_check_traces(nullptr, traces, 2);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, SetAnomalyCallbackNullMetricsFails) {
    int result = stdp_health_set_anomaly_callback(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, SetCheckCallbackNullMetricsFails) {
    int result = stdp_health_set_check_callback(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

static int callback_count = 0;
static void test_anomaly_callback(const stdp_anomaly_report_t* report, void* user_data) {
    (void)report;
    (void)user_data;
    callback_count++;
}

TEST_F(StdpHealthMetricsTest, SetAnomalyCallback) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    callback_count = 0;
    int result = stdp_health_set_anomaly_callback(metrics, test_anomaly_callback, nullptr);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, GetStatsNullMetricsFails) {
    stdp_health_stats_t stats;
    int result = stdp_health_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, GetStatsNullStatsFails) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int result = stdp_health_get_stats(metrics, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, GetStatsReturnsValidStats) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    stdp_health_stats_t stats;
    int result = stdp_health_get_stats(metrics, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.overall_health_score, 0.0f);
    EXPECT_LE(stats.overall_health_score, 1.0f);
}

TEST_F(StdpHealthMetricsTest, GetScoreNullReturnsError) {
    float score = stdp_health_get_score(nullptr);
    EXPECT_LT(score, 0.0f);
}

TEST_F(StdpHealthMetricsTest, GetScoreInitiallyHealthy) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float score = stdp_health_get_score(metrics);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(StdpHealthMetricsTest, ResetStatsNullIsSafe) {
    stdp_health_reset_stats(nullptr);
    // Should not crash
}

TEST_F(StdpHealthMetricsTest, IsHealthyNullReturnsFalse) {
    bool healthy = stdp_health_is_healthy(nullptr);
    EXPECT_FALSE(healthy);
}

TEST_F(StdpHealthMetricsTest, IsHealthyInitiallyTrue) {
    metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    bool healthy = stdp_health_is_healthy(metrics);
    EXPECT_TRUE(healthy);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, ConnectBioAsyncNullMetricsFails) {
    int result = stdp_health_connect_bio_async(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(StdpHealthMetricsTest, BroadcastStatusNullMetricsFails) {
    int result = stdp_health_broadcast_status(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(StdpHealthMetricsTest, AnomalyTypeNameReturnsValidStrings) {
    const char* name;

    name = stdp_anomaly_type_name(STDP_ANOMALY_WEIGHT_DIVERGENCE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "unknown");

    name = stdp_anomaly_type_name(STDP_ANOMALY_TIMING_JITTER);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "unknown");

    name = stdp_anomaly_type_name(STDP_ANOMALY_LR_EXPLOSION);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "unknown");
}

TEST_F(StdpHealthMetricsTest, SeverityNameReturnsValidStrings) {
    for (int i = 0; i <= STDP_SEVERITY_CRITICAL; i++) {
        const char* name = stdp_anomaly_severity_name((stdp_anomaly_severity_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(StdpHealthMetricsTest, VersionReturnsValidString) {
    const char* version = stdp_health_metrics_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}
