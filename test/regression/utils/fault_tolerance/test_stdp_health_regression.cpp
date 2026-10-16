/**
 * @file test_stdp_health_regression.cpp
 * @brief Regression tests for STDP Health Metrics API stability
 * @date 2026-01-20
 *
 * These tests verify that the STDP health metrics API remains stable
 * and that changes don't break existing functionality.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_stdp_health_metrics.h"
}

class StdpHealthRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================================
 * API Signature Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, DefaultConfigFunctionSignature) {
    stdp_health_config_t config;
    int result = stdp_health_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(StdpHealthRegressionTest, CreateFunctionSignature) {
    // Verify create accepts config and bio_async pointers
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    EXPECT_NE(metrics, nullptr);
    if (metrics) {
        stdp_health_destroy(metrics);
    }
}

TEST_F(StdpHealthRegressionTest, DestroyFunctionSignature) {
    stdp_health_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Config Structure Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, ConfigStructureHasRequiredFields) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    // Verify monitoring flags exist
    (void)config.enable_weight_monitoring;
    (void)config.enable_timing_monitoring;
    (void)config.enable_lr_monitoring;
    (void)config.enable_trace_monitoring;
    (void)config.enable_bcm_monitoring;
    (void)config.enable_homeostatic_monitoring;

    // Verify other fields
    EXPECT_GT(config.check_interval_ms, 0u);
    (void)config.verbose_logging;
}

TEST_F(StdpHealthRegressionTest, ThresholdStructuresExist) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    // Weight thresholds
    EXPECT_GT(config.weight_thresholds.max_weight_value, 0.0f);
    EXPECT_LT(config.weight_thresholds.min_weight_value, 0.0f);
    EXPECT_GT(config.weight_thresholds.max_weight_delta, 0.0f);
    EXPECT_GE(config.weight_thresholds.saturation_threshold, 0.0f);

    // Timing thresholds
    EXPECT_GT(config.timing_thresholds.max_timing_window_ms, 0.0f);
    EXPECT_GT(config.timing_thresholds.min_timing_window_ms, 0.0f);
    EXPECT_GT(config.timing_thresholds.max_timing_jitter_ms, 0.0f);

    // Learning rate thresholds
    EXPECT_GT(config.lr_thresholds.max_learning_rate, 0.0f);
    EXPECT_GT(config.lr_thresholds.min_learning_rate, 0.0f);
    EXPECT_GT(config.lr_thresholds.max_lr_change_rate, 0.0f);
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, AnomalyTypeEnumValues) {
    // Verify anomaly type enum values are stable
    EXPECT_EQ(STDP_ANOMALY_NONE, 0);

    // Weight anomalies (0x100 range)
    EXPECT_EQ(STDP_ANOMALY_WEIGHT_DIVERGENCE, 0x100);
    EXPECT_EQ(STDP_ANOMALY_WEIGHT_NAN, 0x101);
    EXPECT_EQ(STDP_ANOMALY_WEIGHT_SATURATION, 0x102);
    EXPECT_EQ(STDP_ANOMALY_WEIGHT_OSCILLATION, 0x103);

    // Timing anomalies (0x110 range)
    EXPECT_EQ(STDP_ANOMALY_TIMING_JITTER, 0x110);
    EXPECT_EQ(STDP_ANOMALY_TIMING_WINDOW_EXCEEDED, 0x111);
    EXPECT_EQ(STDP_ANOMALY_TIMING_CAUSALITY_VIOLATION, 0x112);

    // Learning rate anomalies (0x120 range)
    EXPECT_EQ(STDP_ANOMALY_LR_EXPLOSION, 0x120);
    EXPECT_EQ(STDP_ANOMALY_LR_VANISHING, 0x121);
    EXPECT_EQ(STDP_ANOMALY_LR_INSTABILITY, 0x122);
}

TEST_F(StdpHealthRegressionTest, SeverityEnumValues) {
    EXPECT_EQ(STDP_SEVERITY_INFO, 0);
    EXPECT_EQ(STDP_SEVERITY_WARNING, 1);
    EXPECT_EQ(STDP_SEVERITY_ERROR, 2);
    EXPECT_EQ(STDP_SEVERITY_CRITICAL, 3);
}

/* ============================================================================
 * Report Structure Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, ReportStructureFields) {
    stdp_anomaly_report_t report = {};

    // Verify all expected fields are accessible
    report.type = STDP_ANOMALY_WEIGHT_DIVERGENCE;
    report.severity = STDP_SEVERITY_WARNING;
    report.timestamp_us = 1000;
    report.context_id = 1;
    report.affected_count = 10;
    report.value = 0.5f;
    report.threshold = 1.0f;

    EXPECT_EQ(report.type, STDP_ANOMALY_WEIGHT_DIVERGENCE);
    EXPECT_EQ(report.severity, STDP_SEVERITY_WARNING);
}

/* ============================================================================
 * Stats Structure Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, StatsStructureFields) {
    stdp_health_stats_t stats = {};

    // Verify all expected fields are accessible
    stats.total_anomalies = 10;
    stats.weight_anomalies = 5;
    stats.timing_anomalies = 3;
    stats.lr_anomalies = 2;
    stats.trace_anomalies = 1;
    stats.bcm_anomalies = 0;
    stats.homeostatic_anomalies = 1;
    stats.checks_performed = 100;
    stats.overall_health_score = 0.85f;

    EXPECT_EQ(stats.total_anomalies, 10u);
    EXPECT_EQ(stats.checks_performed, 100u);
    EXPECT_FLOAT_EQ(stats.overall_health_score, 0.85f);
}

/* ============================================================================
 * Utility Function Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, AnomalyTypeNameFunction) {
    // Verify type name function returns expected strings for common types
    EXPECT_NE(stdp_anomaly_type_name(STDP_ANOMALY_WEIGHT_DIVERGENCE), nullptr);
    EXPECT_STRNE(stdp_anomaly_type_name(STDP_ANOMALY_WEIGHT_DIVERGENCE), "unknown");

    EXPECT_NE(stdp_anomaly_type_name(STDP_ANOMALY_TIMING_JITTER), nullptr);
    EXPECT_STRNE(stdp_anomaly_type_name(STDP_ANOMALY_TIMING_JITTER), "unknown");

    EXPECT_NE(stdp_anomaly_type_name(STDP_ANOMALY_LR_EXPLOSION), nullptr);
    EXPECT_STRNE(stdp_anomaly_type_name(STDP_ANOMALY_LR_EXPLOSION), "unknown");
}

TEST_F(StdpHealthRegressionTest, SeverityNameFunction) {
    EXPECT_STREQ(stdp_anomaly_severity_name(STDP_SEVERITY_INFO), "info");
    EXPECT_STREQ(stdp_anomaly_severity_name(STDP_SEVERITY_WARNING), "warning");
    EXPECT_STREQ(stdp_anomaly_severity_name(STDP_SEVERITY_ERROR), "error");
    EXPECT_STREQ(stdp_anomaly_severity_name(STDP_SEVERITY_CRITICAL), "critical");
}

TEST_F(StdpHealthRegressionTest, VersionFunction) {
    const char* version = stdp_health_metrics_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);

    // Version should contain a dot
    EXPECT_NE(strchr(version, '.'), nullptr);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, NullHandlingConsistency) {
    // Config functions
    EXPECT_EQ(stdp_health_default_config(nullptr), -1);

    // Registration functions
    EXPECT_EQ(stdp_health_register_stdp(nullptr, nullptr, "test"), -1);
    EXPECT_EQ(stdp_health_register_bcm(nullptr, nullptr, "test"), -1);
    EXPECT_EQ(stdp_health_register_coordinator(nullptr, nullptr), -1);
    EXPECT_EQ(stdp_health_unregister(nullptr, 0), -1);

    // Check functions
    EXPECT_EQ(stdp_health_check(nullptr), -1);
    EXPECT_EQ(stdp_health_check_weights(nullptr, nullptr, 0, "test"), -1);
    EXPECT_EQ(stdp_health_check_timing(nullptr, nullptr, nullptr, 0), -1);
    EXPECT_EQ(stdp_health_check_learning_rate(nullptr, 0.01f, "test"), -1);
    EXPECT_EQ(stdp_health_check_traces(nullptr, nullptr, 0), -1);

    // Callback functions
    EXPECT_EQ(stdp_health_set_anomaly_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(stdp_health_set_check_callback(nullptr, nullptr, nullptr), -1);

    // Stats functions
    stdp_health_stats_t stats;
    EXPECT_EQ(stdp_health_get_stats(nullptr, &stats), -1);

    // Query functions
    EXPECT_LT(stdp_health_get_score(nullptr), 0.0f);
    EXPECT_FALSE(stdp_health_is_healthy(nullptr));

    // Bio-async functions
    EXPECT_EQ(stdp_health_connect_bio_async(nullptr, nullptr), -1);
    EXPECT_EQ(stdp_health_broadcast_status(nullptr), -1);
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, DefaultConfigValues) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    // All monitoring should be enabled by default
    EXPECT_TRUE(config.enable_weight_monitoring);
    EXPECT_TRUE(config.enable_timing_monitoring);
    EXPECT_TRUE(config.enable_lr_monitoring);
    EXPECT_TRUE(config.enable_trace_monitoring);
    EXPECT_TRUE(config.enable_bcm_monitoring);
    EXPECT_TRUE(config.enable_homeostatic_monitoring);
}

TEST_F(StdpHealthRegressionTest, InitialHealthScore) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    // Initial health should be 1.0 (perfect health)
    float score = stdp_health_get_score(metrics);
    EXPECT_FLOAT_EQ(score, 1.0f);

    EXPECT_TRUE(stdp_health_is_healthy(metrics));

    stdp_health_destroy(metrics);
}

/* ============================================================================
 * Constant Value Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, MaxContextsConstant) {
    // STDP_HEALTH_MAX_CONTEXTS should be defined and reasonable
    EXPECT_GE(STDP_HEALTH_MAX_CONTEXTS, 8);
    EXPECT_LE(STDP_HEALTH_MAX_CONTEXTS, 256);
}

/* ============================================================================
 * Boundary Condition Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, InvalidSeverityHandling) {
    const char* name = stdp_anomaly_severity_name((stdp_anomaly_severity_t)100);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(StdpHealthRegressionTest, UnregisterInvalidId) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    // Invalid IDs should fail gracefully
    EXPECT_EQ(stdp_health_unregister(metrics, -1), -1);
    EXPECT_EQ(stdp_health_unregister(metrics, STDP_HEALTH_MAX_CONTEXTS), -1);
    EXPECT_EQ(stdp_health_unregister(metrics, 1000), -1);

    stdp_health_destroy(metrics);
}

TEST_F(StdpHealthRegressionTest, EmptyWeightsArrayHandling) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float weights[] = {0.5f};
    EXPECT_EQ(stdp_health_check_weights(metrics, weights, 0, "empty"), -1);

    stdp_health_destroy(metrics);
}

TEST_F(StdpHealthRegressionTest, NullWeightsArrayHandling) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    EXPECT_EQ(stdp_health_check_weights(metrics, nullptr, 5, "null"), -1);

    stdp_health_destroy(metrics);
}

/* ============================================================================
 * Functional Regression Tests
 * ============================================================================ */

TEST_F(StdpHealthRegressionTest, HealthyWeightsNoAnomalies) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float weights[] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f};
    int anomalies = stdp_health_check_weights(metrics, weights, 5, "healthy");
    EXPECT_EQ(anomalies, 0);

    stdp_health_destroy(metrics);
}

TEST_F(StdpHealthRegressionTest, NaNWeightsDetected) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    float weights[] = {0.5f, NAN, 0.7f};
    int anomalies = stdp_health_check_weights(metrics, weights, 3, "nan");
    EXPECT_GT(anomalies, 0);

    stdp_health_destroy(metrics);
}

TEST_F(StdpHealthRegressionTest, ValidLearningRateNoAnomaly) {
    stdp_health_metrics_t* metrics = stdp_health_create(nullptr, nullptr);
    ASSERT_NE(metrics, nullptr);

    int anomaly = stdp_health_check_learning_rate(metrics, 0.01f, "valid_lr");
    EXPECT_EQ(anomaly, 0);

    stdp_health_destroy(metrics);
}

TEST_F(StdpHealthRegressionTest, ExcessiveLearningRateDetected) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);
    config.lr_thresholds.max_learning_rate = 0.1f;

    stdp_health_metrics_t* metrics = stdp_health_create(&config, nullptr);
    ASSERT_NE(metrics, nullptr);

    int anomaly = stdp_health_check_learning_rate(metrics, 1.0f, "excessive_lr");
    EXPECT_NE(anomaly, 0);

    stdp_health_destroy(metrics);
}

