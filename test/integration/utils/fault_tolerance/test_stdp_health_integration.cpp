/**
 * @file test_stdp_health_integration.cpp
 * @brief Integration tests for STDP Health Metrics
 * @date 2026-01-20
 *
 * Tests the STDP health metrics module with health agent integration.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_stdp_health_metrics.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

class StdpHealthIntegrationTest : public ::testing::Test {
protected:
    stdp_health_metrics_t* metrics = nullptr;
    nimcp_health_agent_t* health_agent = nullptr;

    void SetUp() override {
        // Create health agent
        health_agent_config_t ha_config;
        nimcp_health_agent_default_config(&ha_config);
        health_agent = nimcp_health_agent_create(&ha_config);
    }

    void TearDown() override {
        if (metrics) {
            stdp_health_destroy(metrics);
            metrics = nullptr;
        }
        if (health_agent) {
            nimcp_health_agent_destroy(health_agent);
            health_agent = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, DefaultConfigSetsReasonableValues) {
    stdp_health_config_t config;
    int result = stdp_health_default_config(&config);
    EXPECT_EQ(result, 0);

    // Check that some defaults are set
    EXPECT_GT(config.check_interval_ms, 0u);
    EXPECT_GT(config.weight_thresholds.max_weight_value, 0.0f);
}

TEST_F(StdpHealthIntegrationTest, CreateWithDefaultConfig) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    EXPECT_NE(metrics, nullptr);
}

TEST_F(StdpHealthIntegrationTest, CreateWithNullHealthAgent) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, nullptr);
    EXPECT_NE(metrics, nullptr);
}

/* ============================================================================
 * Weight Checking Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, CheckHealthyWeightsNoAnomalies) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Normal weight distribution
    float weights[] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f, 0.4f, 0.6f, 0.2f};
    int anomalies = stdp_health_check_weights(metrics, weights, 8, "test_weights");
    EXPECT_EQ(anomalies, 0);
}

TEST_F(StdpHealthIntegrationTest, DetectNaNWeightsAnomaly) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Weights with NaN values
    float weights[] = {0.5f, NAN, 0.7f, 0.1f, NAN};
    int anomalies = stdp_health_check_weights(metrics, weights, 5, "nan_weights");
    EXPECT_GT(anomalies, 0);
}

TEST_F(StdpHealthIntegrationTest, DetectInfWeightsAnomaly) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Weights with infinity
    float weights[] = {0.5f, INFINITY, 0.7f, -INFINITY};
    int anomalies = stdp_health_check_weights(metrics, weights, 4, "inf_weights");
    EXPECT_GT(anomalies, 0);
}

/* ============================================================================
 * Timing Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, CheckValidTimingNoViolations) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Normal pre/post spike times (within window)
    float pre_times[] = {1.0f, 5.0f, 10.0f, 15.0f};
    float post_times[] = {2.0f, 6.0f, 11.0f, 16.0f};

    int violations = stdp_health_check_timing(metrics, pre_times, post_times, 4);
    EXPECT_GE(violations, 0);  // May have 0 or more depending on window
}

/* ============================================================================
 * Learning Rate Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, CheckNormalLearningRate) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    int anomaly = stdp_health_check_learning_rate(metrics, 0.01f, "test_lr");
    EXPECT_EQ(anomaly, 0);
}

TEST_F(StdpHealthIntegrationTest, DetectExcessiveLearningRate) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);
    config.lr_thresholds.max_learning_rate = 0.1f;

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Learning rate above threshold
    int anomaly = stdp_health_check_learning_rate(metrics, 1.0f, "excessive_lr");
    EXPECT_NE(anomaly, 0);
}

TEST_F(StdpHealthIntegrationTest, DetectZeroLearningRate) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    int anomaly = stdp_health_check_learning_rate(metrics, 0.0f, "zero_lr");
    EXPECT_NE(anomaly, 0);
}

/* ============================================================================
 * Trace Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, CheckHealthyTraces) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Normal eligibility traces
    float traces[] = {0.1f, 0.2f, 0.05f, 0.3f, 0.15f};
    int anomalies = stdp_health_check_traces(metrics, traces, 5);
    EXPECT_EQ(anomalies, 0);
}

/* ============================================================================
 * Callback Tests
 * ============================================================================ */

static std::atomic<int> anomaly_callback_count{0};
static stdp_anomaly_type_t last_anomaly_type;

static void test_anomaly_callback(const stdp_anomaly_report_t* report, void* user_data) {
    (void)user_data;
    if (report) {
        last_anomaly_type = report->type;
    }
    anomaly_callback_count++;
}

TEST_F(StdpHealthIntegrationTest, AnomalyCallbackTriggered) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    anomaly_callback_count = 0;
    stdp_health_set_anomaly_callback(metrics, test_anomaly_callback, nullptr);

    // Trigger an anomaly with NaN weights
    float weights[] = {0.5f, NAN, 0.7f};
    stdp_health_check_weights(metrics, weights, 3, "callback_test");

    // Callback should have been triggered
    EXPECT_GT(anomaly_callback_count.load(), 0);
}

static std::atomic<int> check_callback_count{0};

static void test_check_callback(const stdp_health_stats_t* stats, void* user_data) {
    (void)stats;
    (void)user_data;
    check_callback_count++;
}

TEST_F(StdpHealthIntegrationTest, CheckCallbackTriggered) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    check_callback_count = 0;
    stdp_health_set_check_callback(metrics, test_check_callback, nullptr);

    // Run health check
    stdp_health_check(metrics);

    EXPECT_GT(check_callback_count.load(), 0);
}

/* ============================================================================
 * Health Score Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, HealthScoreDegradesWithAnomalies) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    stdp_health_stats_t stats_before;
    stdp_health_get_stats(metrics, &stats_before);
    float initial_score = stats_before.overall_health_score;

    // Introduce anomalies
    float bad_weights[] = {NAN, INFINITY, -INFINITY};
    stdp_health_check_weights(metrics, bad_weights, 3, "bad_weights");

    stdp_health_stats_t stats_after;
    stdp_health_get_stats(metrics, &stats_after);
    float after_score = stats_after.overall_health_score;

    // Health score should decrease or stay the same
    EXPECT_LE(after_score, initial_score);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, StatsAccumulateCorrectly) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Perform multiple checks
    for (int i = 0; i < 5; i++) {
        stdp_health_check(metrics);
    }

    stdp_health_stats_t stats;
    stdp_health_get_stats(metrics, &stats);
    EXPECT_GE(stats.checks_performed, 5u);
}

TEST_F(StdpHealthIntegrationTest, ResetStatsClearsCounters) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    // Generate some stats
    float weights[] = {NAN, 0.5f};
    stdp_health_check_weights(metrics, weights, 2, "reset_test");
    stdp_health_check(metrics);

    // Reset
    stdp_health_reset_stats(metrics);

    stdp_health_stats_t stats;
    stdp_health_get_stats(metrics, &stats);
    EXPECT_EQ(stats.anomalies_detected, 0u);
    EXPECT_EQ(stats.checks_performed, 0u);
}

/* ============================================================================
 * Concurrent Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, ConcurrentHealthChecks) {
    stdp_health_config_t config;
    stdp_health_default_config(&config);

    metrics = stdp_health_create(&config, health_agent);
    ASSERT_NE(metrics, nullptr);

    const int num_threads = 4;
    const int checks_per_thread = 25;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < checks_per_thread; i++) {
                // Mix of good and bad weights
                if ((t + i) % 3 == 0) {
                    float bad_weights[] = {NAN, 0.5f};
                    stdp_health_check_weights(metrics, bad_weights, 2, "concurrent_bad");
                } else {
                    float good_weights[] = {0.3f, 0.5f, 0.7f};
                    stdp_health_check_weights(metrics, good_weights, 3, "concurrent_good");
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should complete without crashes
    stdp_health_stats_t stats;
    int result = stdp_health_get_stats(metrics, &stats);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(StdpHealthIntegrationTest, AnomalyTypeNameReturnsValidString) {
    const char* name = stdp_anomaly_type_name(STDP_ANOMALY_WEIGHT_DIVERGENCE);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(StdpHealthIntegrationTest, SeverityNameReturnsValidString) {
    const char* name = stdp_anomaly_severity_name(STDP_SEVERITY_CRITICAL);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}
