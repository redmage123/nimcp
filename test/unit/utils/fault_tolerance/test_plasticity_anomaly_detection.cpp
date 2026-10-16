/**
 * @file test_plasticity_anomaly_detection.cpp
 * @brief Unit tests for Plasticity Anomaly Detection
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "utils/fault_tolerance/nimcp_plasticity_anomaly_detection.h"
}

class PlasticityAnomalyDetectionTest : public ::testing::Test {
protected:
    plasticity_anomaly_config_t config;
    plasticity_anomaly_detector_t* detector = nullptr;

    void SetUp() override {
        plasticity_anomaly_default_config(&config);
    }

    void TearDown() override {
        if (detector) {
            plasticity_anomaly_destroy(detector);
            detector = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, DefaultConfigSetsReasonableDefaults) {
    plasticity_anomaly_config_t cfg;
    int result = plasticity_anomaly_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enabled);
    EXPECT_TRUE(cfg.detect_weight_anomalies);
    EXPECT_TRUE(cfg.detect_timing_anomalies);
    EXPECT_TRUE(cfg.detect_bcm_anomalies);
    EXPECT_TRUE(cfg.detect_homeostatic_anomalies);
    EXPECT_TRUE(cfg.detect_learning_anomalies);
    EXPECT_GT(cfg.check_interval_ms, 0u);
}

TEST_F(PlasticityAnomalyDetectionTest, DefaultConfigNullReturnsError) {
    int result = plasticity_anomaly_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, SensitivityInValidRange) {
    plasticity_anomaly_config_t cfg;
    plasticity_anomaly_default_config(&cfg);

    EXPECT_GE(cfg.sensitivity, 0.0f);
    EXPECT_LE(cfg.sensitivity, 1.0f);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, CreateWithDefaultConfig) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    EXPECT_NE(detector, nullptr);
}

TEST_F(PlasticityAnomalyDetectionTest, CreateWithCustomConfig) {
    config.sensitivity = 0.9f;
    config.verbose_logging = true;

    detector = plasticity_anomaly_create(&config, nullptr);
    EXPECT_NE(detector, nullptr);
}

TEST_F(PlasticityAnomalyDetectionTest, DestroyNullIsSafe) {
    plasticity_anomaly_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Rule Management Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, AddRuleNullDetectorFails) {
    plasticity_detection_rule_t rule = {};
    int result = plasticity_anomaly_add_rule(nullptr, &rule);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, AddRuleNullRuleFails) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_add_rule(detector, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, AddRuleSucceeds) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    plasticity_detection_rule_t rule = {};
    rule.rule_id = 100;
    rule.enabled = true;
    rule.type = PLASTICITY_RULE_THRESHOLD;
    rule.anomaly = PLASTICITY_ANOMALY_WEIGHT_EXPLOSION;
    rule.category = PLASTICITY_CATEGORY_WEIGHT;
    rule.severity = PLASTICITY_SEVERITY_ERROR;

    int result = plasticity_anomaly_add_rule(detector, &rule);
    EXPECT_EQ(result, 100);
}

TEST_F(PlasticityAnomalyDetectionTest, RemoveRuleNullDetectorFails) {
    int result = plasticity_anomaly_remove_rule(nullptr, 1);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, SetRuleEnabledNullDetectorFails) {
    int result = plasticity_anomaly_set_rule_enabled(nullptr, 1, false);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, LoadDefaultRulesWeight) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int loaded = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_GT(loaded, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, LoadDefaultRulesBCM) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int loaded = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_BCM);
    EXPECT_GT(loaded, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, LoadAllDefaultRules) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int loaded = plasticity_anomaly_load_all_default_rules(detector);
    EXPECT_GT(loaded, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, LoadDefaultRulesNullDetectorFails) {
    int loaded = plasticity_anomaly_load_default_rules(nullptr, PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_EQ(loaded, -1);
}

/* ============================================================================
 * Detection Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, DetectNullDetectorFails) {
    int result = plasticity_anomaly_detect(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, DetectNoRulesReturnsZero) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int anomalies = plasticity_anomaly_detect(detector);
    EXPECT_EQ(anomalies, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, SubmitMetricNullDetectorFails) {
    int result = plasticity_anomaly_submit_metric(nullptr, "test", 1.0f,
        PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, SubmitMetricNullNameFails) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_submit_metric(detector, nullptr, 1.0f,
        PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, SubmitMetricSucceeds) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_submit_metric(detector, "test_weight", 0.5f,
        PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, AnalyzeWeightsNullDetectorFails) {
    float weights[] = {0.5f, 0.3f};
    int result = plasticity_anomaly_analyze_weights(nullptr, weights, 2, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, AnalyzeWeightsNullWeightsFails) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_analyze_weights(detector, nullptr, 2, "test");
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, AnalyzeWeightsHealthyWeights) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    float weights[] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f};
    int anomalies = plasticity_anomaly_analyze_weights(detector, weights, 5, "test");
    EXPECT_EQ(anomalies, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, AnalyzeWeightsNaNDetected) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    float weights[] = {0.5f, NAN, 0.7f};
    int anomalies = plasticity_anomaly_analyze_weights(detector, weights, 3, "test");
    EXPECT_GT(anomalies, 0);
}

TEST_F(PlasticityAnomalyDetectionTest, AnalyzeTimingNullDetectorFails) {
    float pre[] = {1.0f, 2.0f};
    float post[] = {1.5f, 2.5f};
    int result = plasticity_anomaly_analyze_timing(nullptr, pre, post, 2);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, SetCallbackNullDetectorFails) {
    int result = plasticity_anomaly_set_callback(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, SetHealthCallbackNullDetectorFails) {
    int result = plasticity_anomaly_set_health_callback(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

static int anomaly_callback_count = 0;
static void test_anomaly_cb(const plasticity_anomaly_report_t* report, void* user_data) {
    (void)report;
    (void)user_data;
    anomaly_callback_count++;
}

TEST_F(PlasticityAnomalyDetectionTest, SetCallback) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    anomaly_callback_count = 0;
    int result = plasticity_anomaly_set_callback(detector, test_anomaly_cb, nullptr);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, GetStatsNullDetectorFails) {
    plasticity_detection_stats_t stats;
    int result = plasticity_anomaly_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, GetStatsNullStatsFails) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_get_stats(detector, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, GetStatsReturnsValidStats) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    plasticity_detection_stats_t stats;
    int result = plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.current_health_score, 0.0f);
    EXPECT_LE(stats.current_health_score, 1.0f);
}

TEST_F(PlasticityAnomalyDetectionTest, GetHealthNullReturnsError) {
    float health = plasticity_anomaly_get_health(nullptr);
    EXPECT_LT(health, 0.0f);
}

TEST_F(PlasticityAnomalyDetectionTest, GetHealthInitiallyOne) {
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    float health = plasticity_anomaly_get_health(detector);
    EXPECT_FLOAT_EQ(health, 1.0f);
}

TEST_F(PlasticityAnomalyDetectionTest, GetReportsNullDetectorFails) {
    plasticity_anomaly_report_t reports[10];
    int result = plasticity_anomaly_get_reports(nullptr, reports, 10);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, ResetStatsNullIsSafe) {
    plasticity_anomaly_reset_stats(nullptr);
    // Should not crash
}

TEST_F(PlasticityAnomalyDetectionTest, ClearHistoryNullIsSafe) {
    plasticity_anomaly_clear_history(nullptr);
    // Should not crash
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, ConnectBioAsyncNullDetectorFails) {
    int result = plasticity_anomaly_connect_bio_async(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PlasticityAnomalyDetectionTest, BroadcastNullDetectorFails) {
    int result = plasticity_anomaly_broadcast(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, AnomalyTypeNameReturnsValidStrings) {
    const char* name;

    name = plasticity_anomaly_type_name(PLASTICITY_ANOMALY_NONE);
    EXPECT_STREQ(name, "none");

    name = plasticity_anomaly_type_name(PLASTICITY_ANOMALY_WEIGHT_EXPLOSION);
    EXPECT_STREQ(name, "weight_explosion");

    name = plasticity_anomaly_type_name(PLASTICITY_ANOMALY_BCM_THRESHOLD_DRIFT);
    EXPECT_STREQ(name, "bcm_threshold_drift");

    name = plasticity_anomaly_type_name(PLASTICITY_ANOMALY_LEARNING_STALLED);
    EXPECT_STREQ(name, "learning_stalled");
}

TEST_F(PlasticityAnomalyDetectionTest, CategoryNameReturnsValidStrings) {
    for (int i = 0; i < PLASTICITY_CATEGORY_COUNT; i++) {
        const char* name = plasticity_anomaly_category_name((plasticity_anomaly_category_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(PlasticityAnomalyDetectionTest, SeverityNameReturnsValidStrings) {
    const char* names[] = {"info", "warning", "error", "critical"};

    for (int i = 0; i <= PLASTICITY_SEVERITY_CRITICAL; i++) {
        const char* name = plasticity_anomaly_severity_name((plasticity_severity_t)i);
        EXPECT_STREQ(name, names[i]);
    }
}

TEST_F(PlasticityAnomalyDetectionTest, ActionNameReturnsValidStrings) {
    for (int i = 0; i <= PLASTICITY_ACTION_NOTIFY_HEALTH; i++) {
        const char* name = plasticity_anomaly_action_name((plasticity_response_action_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(PlasticityAnomalyDetectionTest, VersionReturnsValidString) {
    const char* version = plasticity_anomaly_detection_version();
    EXPECT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

/* ============================================================================
 * Integration Tests (within unit test scope)
 * ============================================================================ */

TEST_F(PlasticityAnomalyDetectionTest, FullWorkflow) {
    // Create detector
    detector = plasticity_anomaly_create(nullptr, nullptr);
    ASSERT_NE(detector, nullptr);

    // Load default rules
    int loaded = plasticity_anomaly_load_all_default_rules(detector);
    EXPECT_GT(loaded, 0);

    // Submit some metrics
    plasticity_anomaly_submit_metric(detector, "weight_mean", 0.5f, PLASTICITY_CATEGORY_WEIGHT);
    plasticity_anomaly_submit_metric(detector, "activity", 0.3f, PLASTICITY_CATEGORY_HOMEOSTATIC);

    // Run detection
    int anomalies = plasticity_anomaly_detect(detector);
    EXPECT_GE(anomalies, 0);

    // Check health
    float health = plasticity_anomaly_get_health(detector);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);

    // Get stats
    plasticity_detection_stats_t stats;
    int result = plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.checks_performed, 0u);
}
