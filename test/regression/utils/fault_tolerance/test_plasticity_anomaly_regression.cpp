/**
 * @file test_plasticity_anomaly_regression.cpp
 * @brief Regression tests for Plasticity Anomaly Detection API stability
 * @date 2026-01-20
 *
 * These tests verify that the plasticity anomaly detection API remains stable
 * and that changes don't break existing functionality.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "utils/fault_tolerance/nimcp_plasticity_anomaly_detection.h"
}

class PlasticityAnomalyRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/* ============================================================================
 * API Signature Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, DefaultConfigFunctionSignature) {
    plasticity_anomaly_config_t config;
    int result = plasticity_anomaly_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityAnomalyRegressionTest, CreateFunctionSignature) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    EXPECT_NE(detector, nullptr);
    if (detector) {
        plasticity_anomaly_destroy(detector);
    }
}

TEST_F(PlasticityAnomalyRegressionTest, DestroyFunctionSignature) {
    plasticity_anomaly_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Config Structure Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, ConfigStructureHasRequiredFields) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    // Verify expected fields exist
    (void)config.enabled;
    (void)config.detect_weight_anomalies;
    (void)config.detect_timing_anomalies;
    (void)config.detect_bcm_anomalies;
    (void)config.detect_homeostatic_anomalies;
    (void)config.detect_learning_anomalies;
    (void)config.sensitivity;
    (void)config.check_interval_ms;
    (void)config.verbose_logging;
}

TEST_F(PlasticityAnomalyRegressionTest, SensitivityInValidRange) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    EXPECT_GE(config.sensitivity, 0.0f);
    EXPECT_LE(config.sensitivity, 1.0f);
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, CategoryEnumValues) {
    EXPECT_EQ(PLASTICITY_CATEGORY_WEIGHT, 0);
    EXPECT_EQ(PLASTICITY_CATEGORY_TIMING, 1);
    EXPECT_EQ(PLASTICITY_CATEGORY_BCM, 2);
    EXPECT_EQ(PLASTICITY_CATEGORY_HOMEOSTATIC, 3);
    EXPECT_EQ(PLASTICITY_CATEGORY_LEARNING, 4);
    EXPECT_EQ(PLASTICITY_CATEGORY_COUNT, 5);
}

TEST_F(PlasticityAnomalyRegressionTest, AnomalyTypeEnumValues) {
    EXPECT_EQ(PLASTICITY_ANOMALY_NONE, 0);

    // Weight anomalies (0x200 range)
    EXPECT_EQ(PLASTICITY_ANOMALY_WEIGHT_EXPLOSION, 0x200);
    EXPECT_EQ(PLASTICITY_ANOMALY_WEIGHT_VANISHING, 0x201);
    EXPECT_EQ(PLASTICITY_ANOMALY_WEIGHT_NAN, 0x202);
    EXPECT_EQ(PLASTICITY_ANOMALY_WEIGHT_SATURATION, 0x203);

    // Timing anomalies (0x210 range)
    EXPECT_EQ(PLASTICITY_ANOMALY_TIMING_VIOLATION, 0x210);
    EXPECT_EQ(PLASTICITY_ANOMALY_TIMING_DRIFT, 0x211);

    // BCM anomalies (0x220 range)
    EXPECT_EQ(PLASTICITY_ANOMALY_BCM_THRESHOLD_DRIFT, 0x220);
    EXPECT_EQ(PLASTICITY_ANOMALY_BCM_INSTABILITY, 0x221);

    // Homeostatic anomalies (0x230 range)
    EXPECT_EQ(PLASTICITY_ANOMALY_HOMEOSTATIC_FAILURE, 0x230);
    EXPECT_EQ(PLASTICITY_ANOMALY_ACTIVITY_IMBALANCE, 0x231);

    // Learning anomalies (0x240 range)
    EXPECT_EQ(PLASTICITY_ANOMALY_LEARNING_STALLED, 0x240);
    EXPECT_EQ(PLASTICITY_ANOMALY_CONVERGENCE_FAILURE, 0x241);
}

TEST_F(PlasticityAnomalyRegressionTest, SeverityEnumValues) {
    EXPECT_EQ(PLASTICITY_SEVERITY_INFO, 0);
    EXPECT_EQ(PLASTICITY_SEVERITY_WARNING, 1);
    EXPECT_EQ(PLASTICITY_SEVERITY_ERROR, 2);
    EXPECT_EQ(PLASTICITY_SEVERITY_CRITICAL, 3);
}

TEST_F(PlasticityAnomalyRegressionTest, RuleTypeEnumValues) {
    EXPECT_EQ(PLASTICITY_RULE_THRESHOLD, 0);
    EXPECT_EQ(PLASTICITY_RULE_TREND, 1);
    EXPECT_EQ(PLASTICITY_RULE_RATE_OF_CHANGE, 2);
    EXPECT_EQ(PLASTICITY_RULE_STATISTICAL, 3);
    EXPECT_EQ(PLASTICITY_RULE_CORRELATION, 4);
    EXPECT_EQ(PLASTICITY_RULE_PATTERN, 5);
}

TEST_F(PlasticityAnomalyRegressionTest, ActionEnumValues) {
    EXPECT_EQ(PLASTICITY_ACTION_NONE, 0);
    EXPECT_EQ(PLASTICITY_ACTION_LOG, 1);
    EXPECT_EQ(PLASTICITY_ACTION_ALERT, 2);
    EXPECT_EQ(PLASTICITY_ACTION_REDUCE_LR, 3);
    EXPECT_EQ(PLASTICITY_ACTION_PAUSE_LEARNING, 4);
    EXPECT_EQ(PLASTICITY_ACTION_RESET_WEIGHTS, 5);
    EXPECT_EQ(PLASTICITY_ACTION_QUARANTINE, 6);
    EXPECT_EQ(PLASTICITY_ACTION_NOTIFY_HEALTH, 7);
}

/* ============================================================================
 * Rule Structure Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, RuleStructureFields) {
    plasticity_detection_rule_t rule = {};

    // Verify all expected fields are accessible
    rule.rule_id = 1;
    rule.enabled = true;
    rule.type = PLASTICITY_RULE_THRESHOLD;
    rule.anomaly = PLASTICITY_ANOMALY_WEIGHT_EXPLOSION;
    rule.category = PLASTICITY_CATEGORY_WEIGHT;
    rule.severity = PLASTICITY_SEVERITY_ERROR;
    rule.primary_action = PLASTICITY_ACTION_ALERT;
    rule.secondary_action = PLASTICITY_ACTION_REDUCE_LR;
    snprintf(rule.metric_name, sizeof(rule.metric_name), "test_metric");
    snprintf(rule.description, sizeof(rule.description), "Test rule");

    EXPECT_EQ(rule.rule_id, 1);
    EXPECT_TRUE(rule.enabled);
    EXPECT_EQ(rule.type, PLASTICITY_RULE_THRESHOLD);
}

TEST_F(PlasticityAnomalyRegressionTest, RuleParamsUnionAccessible) {
    plasticity_detection_rule_t rule = {};

    // Threshold params
    rule.params.threshold.max_value = 10.0f;
    rule.params.threshold.min_value = -10.0f;
    EXPECT_FLOAT_EQ(rule.params.threshold.max_value, 10.0f);

    // Trend params (reset union)
    memset(&rule.params, 0, sizeof(rule.params));
    rule.params.trend.direction = 1;
    rule.params.trend.duration_ms = 1000;
    EXPECT_EQ(rule.params.trend.direction, 1);

    // Rate params
    memset(&rule.params, 0, sizeof(rule.params));
    rule.params.rate.max_rate = 0.5f;
    rule.params.rate.window_ms = 100;
    EXPECT_FLOAT_EQ(rule.params.rate.max_rate, 0.5f);

    // Statistical params
    memset(&rule.params, 0, sizeof(rule.params));
    rule.params.statistical.sigma_threshold = 3.0f;
    rule.params.statistical.min_samples = 10;
    EXPECT_FLOAT_EQ(rule.params.statistical.sigma_threshold, 3.0f);
}

/* ============================================================================
 * Report Structure Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, ReportStructureFields) {
    plasticity_anomaly_report_t report = {};

    report.anomaly = PLASTICITY_ANOMALY_WEIGHT_EXPLOSION;
    report.category = PLASTICITY_CATEGORY_WEIGHT;
    report.severity = PLASTICITY_SEVERITY_CRITICAL;
    report.rule_id = 100;
    report.timestamp_us = 1000000;
    report.confidence = 0.95f;
    report.value = 50.0f;
    report.threshold = 10.0f;
    snprintf(report.source, sizeof(report.source), "test_source");

    EXPECT_EQ(report.anomaly, PLASTICITY_ANOMALY_WEIGHT_EXPLOSION);
    EXPECT_FLOAT_EQ(report.confidence, 0.95f);
}

/* ============================================================================
 * Stats Structure Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, StatsStructureFields) {
    plasticity_detection_stats_t stats = {};

    stats.checks_performed = 100;
    stats.total_anomalies = 10;
    stats.weight_anomalies = 5;
    stats.timing_anomalies = 2;
    stats.bcm_anomalies = 1;
    stats.homeostatic_anomalies = 1;
    stats.learning_anomalies = 1;
    stats.current_health_score = 0.85f;
    stats.rules_triggered = 10;

    EXPECT_EQ(stats.checks_performed, 100u);
    EXPECT_EQ(stats.total_anomalies, 10u);
    EXPECT_FLOAT_EQ(stats.current_health_score, 0.85f);
}

/* ============================================================================
 * Utility Function Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, AnomalyTypeNameFunction) {
    EXPECT_STREQ(plasticity_anomaly_type_name(PLASTICITY_ANOMALY_NONE), "none");
    EXPECT_STREQ(plasticity_anomaly_type_name(PLASTICITY_ANOMALY_WEIGHT_EXPLOSION),
                 "weight_explosion");
    EXPECT_STREQ(plasticity_anomaly_type_name(PLASTICITY_ANOMALY_BCM_THRESHOLD_DRIFT),
                 "bcm_threshold_drift");
    EXPECT_STREQ(plasticity_anomaly_type_name(PLASTICITY_ANOMALY_LEARNING_STALLED),
                 "learning_stalled");
}

TEST_F(PlasticityAnomalyRegressionTest, CategoryNameFunction) {
    for (int i = 0; i < PLASTICITY_CATEGORY_COUNT; i++) {
        const char* name = plasticity_anomaly_category_name((plasticity_anomaly_category_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(PlasticityAnomalyRegressionTest, SeverityNameFunction) {
    EXPECT_STREQ(plasticity_anomaly_severity_name(PLASTICITY_SEVERITY_INFO), "info");
    EXPECT_STREQ(plasticity_anomaly_severity_name(PLASTICITY_SEVERITY_WARNING), "warning");
    EXPECT_STREQ(plasticity_anomaly_severity_name(PLASTICITY_SEVERITY_ERROR), "error");
    EXPECT_STREQ(plasticity_anomaly_severity_name(PLASTICITY_SEVERITY_CRITICAL), "critical");
}

TEST_F(PlasticityAnomalyRegressionTest, ActionNameFunction) {
    for (int i = 0; i <= PLASTICITY_ACTION_NOTIFY_HEALTH; i++) {
        const char* name = plasticity_anomaly_action_name((plasticity_response_action_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(PlasticityAnomalyRegressionTest, VersionFunction) {
    const char* version = plasticity_anomaly_detection_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);

    // Version should contain a dot
    EXPECT_NE(strchr(version, '.'), nullptr);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, NullHandlingConsistency) {
    // Config functions
    EXPECT_EQ(plasticity_anomaly_default_config(nullptr), -1);

    // Rule functions
    EXPECT_EQ(plasticity_anomaly_add_rule(nullptr, nullptr), -1);
    EXPECT_EQ(plasticity_anomaly_remove_rule(nullptr, 1), -1);
    EXPECT_EQ(plasticity_anomaly_set_rule_enabled(nullptr, 1, true), -1);
    EXPECT_EQ(plasticity_anomaly_load_default_rules(nullptr, PLASTICITY_CATEGORY_WEIGHT), -1);
    EXPECT_EQ(plasticity_anomaly_load_all_default_rules(nullptr), -1);

    // Detection functions
    EXPECT_EQ(plasticity_anomaly_detect(nullptr), -1);
    EXPECT_EQ(plasticity_anomaly_submit_metric(nullptr, "test", 1.0f, PLASTICITY_CATEGORY_WEIGHT), -1);
    EXPECT_EQ(plasticity_anomaly_analyze_weights(nullptr, nullptr, 0, "test"), -1);
    EXPECT_EQ(plasticity_anomaly_analyze_timing(nullptr, nullptr, nullptr, 0), -1);

    // Callback functions
    EXPECT_EQ(plasticity_anomaly_set_callback(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(plasticity_anomaly_set_health_callback(nullptr, nullptr, nullptr), -1);

    // Query functions
    plasticity_detection_stats_t stats;
    EXPECT_EQ(plasticity_anomaly_get_stats(nullptr, &stats), -1);
    EXPECT_LT(plasticity_anomaly_get_health(nullptr), 0.0f);
    EXPECT_EQ(plasticity_anomaly_get_reports(nullptr, nullptr, 0), -1);

    // Bio-async functions
    EXPECT_EQ(plasticity_anomaly_connect_bio_async(nullptr, nullptr), -1);
    EXPECT_EQ(plasticity_anomaly_broadcast(nullptr), -1);
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, DefaultConfigValues) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    // Should be enabled by default
    EXPECT_TRUE(config.enabled);

    // All detection types should be enabled
    EXPECT_TRUE(config.detect_weight_anomalies);
    EXPECT_TRUE(config.detect_timing_anomalies);
    EXPECT_TRUE(config.detect_bcm_anomalies);
    EXPECT_TRUE(config.detect_homeostatic_anomalies);
    EXPECT_TRUE(config.detect_learning_anomalies);

    // Check interval should be positive
    EXPECT_GT(config.check_interval_ms, 0u);
}

TEST_F(PlasticityAnomalyRegressionTest, InitialHealthScore) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    // Initial health should be 1.0 (perfect)
    float health = plasticity_anomaly_get_health(detector);
    EXPECT_FLOAT_EQ(health, 1.0f);

    plasticity_anomaly_destroy(detector);
}

TEST_F(PlasticityAnomalyRegressionTest, DefaultRulesLoaded) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int weight_rules = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_GT(weight_rules, 0);

    int bcm_rules = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_BCM);
    EXPECT_GT(bcm_rules, 0);

    plasticity_anomaly_destroy(detector);
}

/* ============================================================================
 * Boundary Condition Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, InvalidSeverityHandling) {
    const char* name = plasticity_anomaly_severity_name((plasticity_severity_t)100);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(PlasticityAnomalyRegressionTest, InvalidCategoryHandling) {
    const char* name = plasticity_anomaly_category_name((plasticity_anomaly_category_t)100);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(PlasticityAnomalyRegressionTest, InvalidActionHandling) {
    const char* name = plasticity_anomaly_action_name((plasticity_response_action_t)100);
    EXPECT_STREQ(name, "unknown");
}

TEST_F(PlasticityAnomalyRegressionTest, NullMetricNameHandling) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_submit_metric(detector, nullptr, 1.0f, PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_EQ(result, -1);

    plasticity_anomaly_destroy(detector);
}

/* ============================================================================
 * Functional Regression Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyRegressionTest, HealthyWeightsNoAnomalies) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    float weights[] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f};
    int anomalies = plasticity_anomaly_analyze_weights(detector, weights, 5, "healthy");
    EXPECT_EQ(anomalies, 0);

    plasticity_anomaly_destroy(detector);
}

TEST_F(PlasticityAnomalyRegressionTest, NaNWeightsDetected) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    float weights[] = {0.5f, NAN, 0.7f};
    int anomalies = plasticity_anomaly_analyze_weights(detector, weights, 3, "nan");
    EXPECT_GT(anomalies, 0);

    plasticity_anomaly_destroy(detector);
}

TEST_F(PlasticityAnomalyRegressionTest, DetectNoRulesReturnsZero) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    // Don't load any rules
    int anomalies = plasticity_anomaly_detect(detector);
    EXPECT_EQ(anomalies, 0);

    plasticity_anomaly_destroy(detector);
}

TEST_F(PlasticityAnomalyRegressionTest, AddRuleReturnsId) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    plasticity_detection_rule_t rule = {};
    rule.rule_id = 12345;
    rule.enabled = true;
    rule.type = PLASTICITY_RULE_THRESHOLD;
    rule.anomaly = PLASTICITY_ANOMALY_WEIGHT_EXPLOSION;
    rule.category = PLASTICITY_CATEGORY_WEIGHT;
    rule.severity = PLASTICITY_SEVERITY_ERROR;

    int id = plasticity_anomaly_add_rule(detector, &rule);
    EXPECT_EQ(id, 12345);

    plasticity_anomaly_destroy(detector);
}

TEST_F(PlasticityAnomalyRegressionTest, StatsResetClearsAll) {
    plasticity_anomaly_detector_t* detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    // Generate some stats
    float weights[] = {NAN};
    plasticity_anomaly_analyze_weights(detector, weights, 1, "reset_test");
    plasticity_anomaly_detect(detector);

    // Reset
    plasticity_anomaly_reset_stats(detector);

    plasticity_detection_stats_t stats;
    plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(stats.total_anomalies, 0u);
    EXPECT_EQ(stats.checks_performed, 0u);

    plasticity_anomaly_destroy(detector);
}

