/**
 * @file test_plasticity_anomaly_integration.cpp
 * @brief Integration tests for Plasticity Anomaly Detection
 * @date 2026-01-20
 *
 * Tests the plasticity anomaly detection module with health monitoring integration.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_plasticity_anomaly_detection.h"
}

class PlasticityAnomalyIntegrationTest : public ::testing::Test {
protected:
    plasticity_anomaly_detector_t* detector = nullptr;

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

TEST_F(PlasticityAnomalyIntegrationTest, DefaultConfigSetsReasonableValues) {
    plasticity_anomaly_config_t config;
    int result = plasticity_anomaly_default_config(&config);
    EXPECT_EQ(result, 0);

    // Verify defaults are reasonable
    EXPECT_GE(config.sensitivity, 0.0f);
    EXPECT_LE(config.sensitivity, 1.0f);
}

TEST_F(PlasticityAnomalyIntegrationTest, CreateWithDefaultConfig) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    EXPECT_NE(detector, nullptr);
}

/* ============================================================================
 * Weight Analysis Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, AnalyzeHealthyWeights) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Load default weight rules
    plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);

    // Normal weight distribution
    float weights[] = {0.5f, 0.3f, 0.7f, 0.1f, 0.9f, 0.4f};
    int anomalies = plasticity_anomaly_analyze_weights(detector, weights, 6, "test_weights");
    EXPECT_EQ(anomalies, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, AnalyzeWeightsWithNaN) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Load default weight rules
    plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);

    // Weights with NaN
    float weights[] = {0.5f, NAN, 0.7f};
    int anomalies = plasticity_anomaly_analyze_weights(detector, weights, 3, "nan_weights");
    EXPECT_GT(anomalies, 0);
}

/* ============================================================================
 * Timing Analysis Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, AnalyzeNormalTiming) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Load default timing rules
    plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_TIMING);

    // Normal timing (within STDP window)
    float pre[] = {1.0f, 5.0f, 10.0f};
    float post[] = {2.0f, 6.0f, 11.0f};
    int anomalies = plasticity_anomaly_analyze_timing(detector, pre, post, 3);
    EXPECT_GE(anomalies, 0);
}

/* ============================================================================
 * Rule Management Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, LoadDefaultWeightRules) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_GE(result, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, LoadDefaultTimingRules) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_TIMING);
    EXPECT_GE(result, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, LoadDefaultBCMRules) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_BCM);
    EXPECT_GE(result, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, LoadAllDefaultRules) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_load_all_default_rules(detector);
    EXPECT_GE(result, 0);
}

/* ============================================================================
 * Metric Submission Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, SubmitMetricSucceeds) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    int result = plasticity_anomaly_submit_metric(detector, "weight_mean", 0.5f,
        PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, SubmitMultipleMetrics) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Submit multiple metrics
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "metric_%d", i);
        int result = plasticity_anomaly_submit_metric(detector, name, 0.5f + (i * 0.01f),
            PLASTICITY_CATEGORY_WEIGHT);
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Detection Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, DetectWithNoRules) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Detect without any rules loaded
    int anomalies = plasticity_anomaly_detect(detector);
    EXPECT_EQ(anomalies, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, DetectAfterSubmittingMetrics) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Load rules
    plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);

    // Submit metrics
    plasticity_anomaly_submit_metric(detector, "weight_mean", 0.5f, PLASTICITY_CATEGORY_WEIGHT);

    // Detect
    int anomalies = plasticity_anomaly_detect(detector);
    EXPECT_GE(anomalies, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, GetStatsSucceeds) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    plasticity_detection_stats_t stats;
    int result = plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, ResetStatsSucceeds) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Generate some activity
    plasticity_anomaly_load_all_default_rules(detector);
    plasticity_anomaly_submit_metric(detector, "test", 0.5f, PLASTICITY_CATEGORY_WEIGHT);
    plasticity_anomaly_detect(detector);

    // Reset
    plasticity_anomaly_reset_stats(detector);

    plasticity_detection_stats_t stats;
    plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(stats.anomalies_detected, 0u);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, ConcurrentMetricSubmission) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    const int num_threads = 4;
    const int metrics_per_thread = 25;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < metrics_per_thread; i++) {
                char name[64];
                snprintf(name, sizeof(name), "thread_%d_metric_%d", t, i);
                plasticity_anomaly_submit_metric(detector, name, 0.5f + (t * 0.01f),
                    PLASTICITY_CATEGORY_WEIGHT);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Should complete without crashes
    plasticity_detection_stats_t stats;
    int result = plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(PlasticityAnomalyIntegrationTest, ConcurrentWeightAnalysis) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    plasticity_anomaly_load_default_rules(detector, PLASTICITY_CATEGORY_WEIGHT);

    const int num_threads = 4;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 20; i++) {
                float weights[10];
                for (int w = 0; w < 10; w++) {
                    weights[w] = 0.5f + (t * 0.01f) + (w * 0.02f);
                    // Inject occasional bad values
                    if (t == 0 && i % 5 == 0 && w == 3) {
                        weights[w] = NAN;
                    }
                }
                char name[32];
                snprintf(name, sizeof(name), "weights_%d_%d", t, i);
                plasticity_anomaly_analyze_weights(detector, weights, 10, name);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Should complete without crashes
    plasticity_detection_stats_t stats;
    int result = plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, CategoryNameReturnsValidString) {
    const char* name = plasticity_anomaly_category_name(PLASTICITY_CATEGORY_WEIGHT);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(PlasticityAnomalyIntegrationTest, AllCategoryNamesValid) {
    plasticity_anomaly_category_t categories[] = {
        PLASTICITY_CATEGORY_WEIGHT,
        PLASTICITY_CATEGORY_TIMING,
        PLASTICITY_CATEGORY_BCM,
        PLASTICITY_CATEGORY_HOMEOSTATIC,
        PLASTICITY_CATEGORY_LEARNING
    };

    for (auto cat : categories) {
        const char* name = plasticity_anomaly_category_name(cat);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

/* ============================================================================
 * Full Pipeline Test
 * ============================================================================ */

TEST_F(PlasticityAnomalyIntegrationTest, FullDetectionPipeline) {
    plasticity_anomaly_config_t config;
    plasticity_anomaly_default_config(&config);

    detector = plasticity_anomaly_create(&config, nullptr);
    ASSERT_NE(detector, nullptr);

    // Load all rules
    plasticity_anomaly_load_all_default_rules(detector);

    // Simulate a learning session with monitoring
    for (int epoch = 0; epoch < 5; epoch++) {
        // Analyze weights
        float weights[10];
        for (int i = 0; i < 10; i++) {
            weights[i] = 0.5f + (i * 0.05f);
            // Inject anomaly in epoch 3
            if (epoch == 3 && i == 5) {
                weights[i] = NAN;
            }
        }
        char name[32];
        snprintf(name, sizeof(name), "epoch_%d_weights", epoch);
        plasticity_anomaly_analyze_weights(detector, weights, 10, name);

        // Analyze timing
        float pre[] = {1.0f, 2.0f, 3.0f};
        float post[] = {1.5f, 2.5f, 3.5f};
        plasticity_anomaly_analyze_timing(detector, pre, post, 3);

        // Submit metrics
        plasticity_anomaly_submit_metric(detector, "learning_rate", 0.01f,
            PLASTICITY_CATEGORY_LEARNING);

        // Run detection
        plasticity_anomaly_detect(detector);
    }

    // Verify stats
    plasticity_detection_stats_t stats;
    plasticity_anomaly_get_stats(detector, &stats);
    EXPECT_GT(stats.anomalies_detected, 0u);  // Should have detected the NaN
}
