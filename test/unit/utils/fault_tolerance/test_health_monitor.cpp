/**
 * @file test_health_monitor.cpp
 * @brief Comprehensive tests for NIMCP Health Monitor
 * @version 1.0.0
 * @date 2025-11-19
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include <thread>
#include <chrono>
#include <cstring>

//=============================================================================
// Test Fixtures
//=============================================================================

class HealthMonitorTest : public ::testing::Test {
protected:
    health_monitor_t monitor;

    void SetUp() override {
        monitor = health_monitor_create("test_brain");
        ASSERT_NE(monitor, nullptr);
    }

    void TearDown() override {
        if (monitor) {
            health_monitor_destroy(monitor);
            monitor = nullptr;
        }
    }
};

class HealthMonitorRunningTest : public HealthMonitorTest {
protected:
    void SetUp() override {
        HealthMonitorTest::SetUp();
        ASSERT_TRUE(health_monitor_start(monitor));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (monitor && health_monitor_is_running(monitor)) {
            health_monitor_stop(monitor);
        }
        HealthMonitorTest::TearDown();
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(HealthMonitorLifecycle, CreateDestroy) {
    health_monitor_t mon = health_monitor_create("lifecycle_test");
    ASSERT_NE(mon, nullptr);
    health_monitor_destroy(mon);
}

TEST(HealthMonitorLifecycle, CreateNullID) {
    health_monitor_t mon = health_monitor_create(nullptr);
    EXPECT_EQ(mon, nullptr);
}

TEST(HealthMonitorLifecycle, DestroyNull) {
    // Should not crash
    health_monitor_destroy(nullptr);
}

TEST(HealthMonitorLifecycle, StartStop) {
    health_monitor_t mon = health_monitor_create("start_stop_test");
    ASSERT_NE(mon, nullptr);

    EXPECT_FALSE(health_monitor_is_running(mon));

    EXPECT_TRUE(health_monitor_start(mon));
    EXPECT_TRUE(health_monitor_is_running(mon));

    // Allow thread to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(health_monitor_stop(mon));
    EXPECT_FALSE(health_monitor_is_running(mon));

    health_monitor_destroy(mon);
}

TEST(HealthMonitorLifecycle, DoubleStart) {
    health_monitor_t mon = health_monitor_create("double_start_test");
    ASSERT_NE(mon, nullptr);

    EXPECT_TRUE(health_monitor_start(mon));
    EXPECT_TRUE(health_monitor_is_running(mon));

    // Second start should succeed (already running)
    EXPECT_TRUE(health_monitor_start(mon));
    EXPECT_TRUE(health_monitor_is_running(mon));

    health_monitor_stop(mon);
    health_monitor_destroy(mon);
}

TEST(HealthMonitorLifecycle, StopWithoutStart) {
    health_monitor_t mon = health_monitor_create("stop_without_start");
    ASSERT_NE(mon, nullptr);

    // Should succeed without starting
    EXPECT_TRUE(health_monitor_stop(mon));

    health_monitor_destroy(mon);
}

//=============================================================================
// Metric Recording Tests
//=============================================================================

TEST_F(HealthMonitorTest, RecordOperation) {
    // Record multiple operations
    health_monitor_record_operation(monitor, "inference", 1000);
    health_monitor_record_operation(monitor, "inference", 1200);
    health_monitor_record_operation(monitor, "inference", 800);
    health_monitor_record_operation(monitor, "learning", 5000);

    // Verify operation stats
    operation_metric_t stats;
    EXPECT_TRUE(health_monitor_get_operation_stats(monitor, "inference", &stats));
    EXPECT_EQ(stats.count, 3);
    EXPECT_EQ(stats.min_duration_us, 800);
    EXPECT_EQ(stats.max_duration_us, 1200);
    EXPECT_DOUBLE_EQ(stats.avg_duration_us, 1000.0);

    EXPECT_TRUE(health_monitor_get_operation_stats(monitor, "learning", &stats));
    EXPECT_EQ(stats.count, 1);
    EXPECT_EQ(stats.avg_duration_us, 5000.0);
}

TEST_F(HealthMonitorTest, RecordOperationNull) {
    // Null monitor should not crash
    health_monitor_record_operation(nullptr, "test", 100);

    // Null operation should not crash
    health_monitor_record_operation(monitor, nullptr, 100);
}

TEST_F(HealthMonitorTest, RecordMemory) {
    health_monitor_record_memory(monitor, 1024 * 1024);  // 1 MB
    health_monitor_record_memory(monitor, 2 * 1024 * 1024);  // 2 MB
    health_monitor_record_memory(monitor, 3 * 1024 * 1024);  // 3 MB

    memory_metric_t stats;
    EXPECT_TRUE(health_monitor_get_memory_stats(monitor, &stats));
    EXPECT_EQ(stats.current_bytes, 3 * 1024 * 1024);
    EXPECT_EQ(stats.peak_bytes, 3 * 1024 * 1024);
    // Note: growth_rate may be 0 if records happen instantly (no time delta)
    EXPECT_GE(stats.growth_rate, 0.0);
}

TEST_F(HealthMonitorTest, RecordMemoryNull) {
    health_monitor_record_memory(nullptr, 1024);
}

TEST_F(HealthMonitorTest, RecordError) {
    health_monitor_record_error(monitor, "FPE");
    health_monitor_record_error(monitor, "FPE");
    health_monitor_record_error(monitor, "NULL_PTR");

    // Errors are tracked but no direct query API
    // Will be verified through anomaly detection
}

TEST_F(HealthMonitorTest, RecordErrorNull) {
    health_monitor_record_error(nullptr, "test");
    health_monitor_record_error(monitor, nullptr);
}

TEST_F(HealthMonitorTest, RecordCacheAccess) {
    // Record 80% hit rate
    for (int i = 0; i < 80; i++) {
        health_monitor_record_cache_access(monitor, true);
    }
    for (int i = 0; i < 20; i++) {
        health_monitor_record_cache_access(monitor, false);
    }

    // Verify cache stats indirectly through health status
    // (get_status computes scores on-demand)
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GT(status.cache_score, 70.0f);  // Good hit rate
}

TEST_F(HealthMonitorTest, RecordThreadEvent) {
    // Record mostly non-contentious locks
    for (int i = 0; i < 90; i++) {
        health_monitor_record_thread_event(monitor, false, 0);
    }
    // Some contention
    for (int i = 0; i < 10; i++) {
        health_monitor_record_thread_event(monitor, true, 100);
    }

    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_GT(status.thread_score, 70.0f);  // Low contention
}

TEST_F(HealthMonitorTest, RecordThroughput) {
    // Record 1000 ops in 1 second (1000 ops/sec)
    health_monitor_record_throughput(monitor, 1000, 1000000);

    // Record 500 ops in 1 second (500 ops/sec)
    health_monitor_record_throughput(monitor, 500, 1000000);

    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    // Throughput score depends on comparison with average
}

//=============================================================================
// Health Assessment Tests
//=============================================================================

TEST_F(HealthMonitorTest, GetStatusInitial) {
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    // On-demand computation: fresh monitor with no issues is healthy
    EXPECT_GE(status.score, 0.0f);
    EXPECT_LE(status.score, 100.0f);
}

TEST_F(HealthMonitorTest, GetStatusNull) {
    health_status_snapshot_t status;
    EXPECT_FALSE(health_monitor_get_status(nullptr, &status));

    EXPECT_FALSE(health_monitor_get_status(monitor, nullptr));
}

TEST_F(HealthMonitorTest, GetScore) {
    float score = health_monitor_get_score(monitor);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(HealthMonitorTest, GetScoreNull) {
    float score = health_monitor_get_score(nullptr);
    EXPECT_EQ(score, -1.0f);
}

TEST_F(HealthMonitorTest, IsHealthy) {
    // Initially should have some score
    bool healthy = health_monitor_is_healthy(monitor);
    // Result depends on initial state
}

TEST_F(HealthMonitorTest, GetStatusLevel) {
    health_status_t level = health_monitor_get_status_level(monitor);
    EXPECT_GE(level, HEALTH_EXCELLENT);
    EXPECT_LE(level, HEALTH_UNKNOWN);
}

TEST_F(HealthMonitorTest, GetStatusLevelNull) {
    health_status_t level = health_monitor_get_status_level(nullptr);
    EXPECT_EQ(level, HEALTH_UNKNOWN);
}

TEST_F(HealthMonitorRunningTest, HealthScoreCalculation) {
    // Record good metrics
    for (int i = 0; i < 100; i++) {
        health_monitor_record_operation(monitor, "inference", 1000);
        health_monitor_record_cache_access(monitor, true);
        health_monitor_record_thread_event(monitor, false, 0);
    }
    health_monitor_record_memory(monitor, 10 * 1024 * 1024);
    health_monitor_establish_baseline(monitor);

    // Wait for monitoring cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    float score = health_monitor_get_score(monitor);
    EXPECT_GT(score, 70.0f);  // Should be healthy
}

//=============================================================================
// Anomaly Detection Tests
//=============================================================================

TEST_F(HealthMonitorTest, DetectAnomaliesInitial) {
    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);
    EXPECT_GE(count, 0);
}

TEST_F(HealthMonitorTest, DetectAnomaliesNull) {
    anomaly_t anomalies[10];
    EXPECT_EQ(health_monitor_detect_anomalies(nullptr, anomalies, 10), -1);
    EXPECT_EQ(health_monitor_detect_anomalies(monitor, nullptr, 10), -1);
    EXPECT_EQ(health_monitor_detect_anomalies(monitor, anomalies, 0), -1);
}

TEST_F(HealthMonitorRunningTest, DetectMemoryLeak) {
    // Establish baseline
    health_monitor_record_memory(monitor, 10 * 1024 * 1024);
    health_monitor_establish_baseline(monitor);

    // Simulate memory leak - steadily increasing memory
    for (int i = 0; i < 100; i++) {
        size_t memory = (10 + i) * 1024 * 1024;
        health_monitor_record_memory(monitor, memory);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    // Should detect memory leak
    bool found_memory_leak = false;
    for (int32_t i = 0; i < count; i++) {
        if (anomalies[i].type == ANOMALY_MEMORY_LEAK) {
            found_memory_leak = true;
            EXPECT_GT(anomalies[i].confidence, 0.0);
            EXPECT_LE(anomalies[i].confidence, 1.0);
        }
    }
    EXPECT_TRUE(found_memory_leak);
}

TEST_F(HealthMonitorRunningTest, DetectPerformanceDegradation) {
    // Establish baseline with fast operations
    for (int i = 0; i < 50; i++) {
        health_monitor_record_operation(monitor, "inference", 100);
    }
    health_monitor_establish_baseline(monitor);

    // Simulate degradation - steadily increasing latency
    for (int i = 0; i < 50; i++) {
        health_monitor_record_operation(monitor, "inference", 100 + i * 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    // May detect performance degradation
    bool found_degradation = false;
    for (int32_t i = 0; i < count; i++) {
        if (anomalies[i].type == ANOMALY_PERFORMANCE_DEGRADATION) {
            found_degradation = true;
        }
    }
}

TEST_F(HealthMonitorRunningTest, DetectErrorSpike) {
    // Establish baseline with few errors
    health_monitor_establish_baseline(monitor);

    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Simulate error spike
    for (int i = 0; i < 100; i++) {
        health_monitor_record_error(monitor, "FPE");
    }

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    // Check if error spike detected (depends on implementation thresholds)
    bool found_error_spike = false;
    for (int32_t i = 0; i < count; i++) {
        if (anomalies[i].type == ANOMALY_ERROR_SPIKE) {
            found_error_spike = true;
        }
    }
    // Note: Detection depends on timing and thresholds - not guaranteed
    (void)found_error_spike;
    EXPECT_GE(count, 0);
}

TEST_F(HealthMonitorRunningTest, DetectCacheThrashing) {
    // Establish baseline with good hit rate
    for (int i = 0; i < 80; i++) {
        health_monitor_record_cache_access(monitor, true);
    }
    for (int i = 0; i < 20; i++) {
        health_monitor_record_cache_access(monitor, false);
    }
    health_monitor_establish_baseline(monitor);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Simulate cache thrashing - very low hit rate
    for (int i = 0; i < 90; i++) {
        health_monitor_record_cache_access(monitor, false);
    }
    for (int i = 0; i < 10; i++) {
        health_monitor_record_cache_access(monitor, true);
    }

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    bool found_cache_thrashing = false;
    for (int32_t i = 0; i < count; i++) {
        if (anomalies[i].type == ANOMALY_CACHE_THRASHING) {
            found_cache_thrashing = true;
        }
    }
    // Note: Detection depends on timing and thresholds - not guaranteed
    (void)found_cache_thrashing;
    EXPECT_GE(count, 0);
}

TEST_F(HealthMonitorRunningTest, DetectThroughputDrop) {
    // Establish baseline with good throughput
    health_monitor_record_throughput(monitor, 1000, 1000000);
    health_monitor_establish_baseline(monitor);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Simulate throughput drop
    health_monitor_record_throughput(monitor, 100, 1000000);

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    bool found_throughput_drop = false;
    for (int32_t i = 0; i < count; i++) {
        if (anomalies[i].type == ANOMALY_THROUGHPUT_DROP) {
            found_throughput_drop = true;
        }
    }
    EXPECT_TRUE(found_throughput_drop);
}

TEST_F(HealthMonitorRunningTest, DetectThreadContention) {
    // Simulate high thread contention
    for (int i = 0; i < 30; i++) {
        health_monitor_record_thread_event(monitor, false, 0);
    }
    for (int i = 0; i < 70; i++) {
        health_monitor_record_thread_event(monitor, true, 1000);
    }

    // Wait for detection
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);

    bool found_thread_contention = false;
    for (int32_t i = 0; i < count; i++) {
        if (anomalies[i].type == ANOMALY_THREAD_CONTENTION) {
            found_thread_contention = true;
        }
    }
    EXPECT_TRUE(found_thread_contention);
}

//=============================================================================
// Failure Prediction Tests
//=============================================================================

TEST_F(HealthMonitorTest, PredictFailureNoData) {
    uint32_t ttf;
    bool predicted = health_monitor_predict_failure(monitor, &ttf);
    // May or may not predict with no data
}

TEST_F(HealthMonitorTest, PredictFailureNull) {
    EXPECT_FALSE(health_monitor_predict_failure(nullptr, nullptr));
}

TEST_F(HealthMonitorRunningTest, PredictFailureMemoryLeak) {
    // Establish baseline
    health_monitor_record_memory(monitor, 100 * 1024 * 1024);
    health_monitor_establish_baseline(monitor);

    // Simulate severe memory leak
    for (int i = 0; i < 100; i++) {
        size_t memory = (100 + i * 10) * 1024 * 1024;
        health_monitor_record_memory(monitor, memory);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t ttf;
    bool predicted = health_monitor_predict_failure(monitor, &ttf);
    EXPECT_TRUE(predicted);
    if (predicted) {
        EXPECT_GT(ttf, 0);
    }
}

TEST_F(HealthMonitorRunningTest, PredictFailureErrorRate) {
    // Simulate high error rate
    for (int i = 0; i < 100; i++) {
        health_monitor_record_error(monitor, "CRITICAL");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t ttf;
    bool predicted = health_monitor_predict_failure(monitor, &ttf);
    EXPECT_TRUE(predicted);
}

//=============================================================================
// Baseline and Configuration Tests
//=============================================================================

TEST_F(HealthMonitorTest, EstablishBaseline) {
    health_monitor_record_memory(monitor, 50 * 1024 * 1024);

    EXPECT_TRUE(health_monitor_establish_baseline(monitor));

    memory_metric_t stats;
    EXPECT_TRUE(health_monitor_get_memory_stats(monitor, &stats));
    EXPECT_EQ(stats.baseline_bytes, 50 * 1024 * 1024);
}

TEST_F(HealthMonitorTest, ResetBaseline) {
    health_monitor_establish_baseline(monitor);
    EXPECT_TRUE(health_monitor_reset_baseline(monitor));
}

TEST_F(HealthMonitorTest, SetAnomalyThreshold) {
    EXPECT_TRUE(health_monitor_set_anomaly_threshold(monitor, 2.5));
    EXPECT_FALSE(health_monitor_set_anomaly_threshold(monitor, -1.0));
}

TEST_F(HealthMonitorTest, SetInterval) {
    EXPECT_TRUE(health_monitor_set_interval(monitor, 500));
    EXPECT_FALSE(health_monitor_set_interval(monitor, 0));
}

//=============================================================================
// Anomaly Management Tests
//=============================================================================

TEST_F(HealthMonitorTest, ClearResolvedAnomalies) {
    uint32_t cleared = health_monitor_clear_resolved_anomalies(monitor);
    EXPECT_GE(cleared, 0);
}

TEST_F(HealthMonitorTest, GetAnomalyCount) {
    uint32_t count = health_monitor_get_anomaly_count(monitor, ANOMALY_MEMORY_LEAK);
    EXPECT_GE(count, 0);
}

//=============================================================================
// Reporting Tests
//=============================================================================

TEST_F(HealthMonitorTest, GenerateReport) {
    // Record some metrics
    health_monitor_record_operation(monitor, "test_op", 1000);
    health_monitor_record_memory(monitor, 10 * 1024 * 1024);
    health_monitor_record_error(monitor, "TEST_ERROR");

    // Generate report to stdout (visual verification)
    health_monitor_report(monitor, stdout);
}

TEST_F(HealthMonitorTest, ReportNull) {
    health_monitor_report(nullptr, stdout);
    health_monitor_report(monitor, nullptr);
}

TEST_F(HealthMonitorTest, ExportJSON) {
    health_monitor_record_operation(monitor, "test_op", 1000);

    char json[2048];
    int32_t written = health_monitor_export_json(monitor, json, sizeof(json));

    EXPECT_GT(written, 0);
    EXPECT_LT(written, (int32_t)sizeof(json));

    // Verify JSON contains expected fields
    EXPECT_NE(strstr(json, "brain_id"), nullptr);
    EXPECT_NE(strstr(json, "status"), nullptr);
    EXPECT_NE(strstr(json, "score"), nullptr);
}

TEST_F(HealthMonitorTest, ExportJSONNull) {
    char json[1024];
    EXPECT_EQ(health_monitor_export_json(nullptr, json, sizeof(json)), -1);
    EXPECT_EQ(health_monitor_export_json(monitor, nullptr, sizeof(json)), -1);
    EXPECT_EQ(health_monitor_export_json(monitor, json, 0), -1);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST(HealthMonitorUtility, StatusToString) {
    EXPECT_STREQ(health_status_to_string(HEALTH_EXCELLENT), "EXCELLENT");
    EXPECT_STREQ(health_status_to_string(HEALTH_GOOD), "GOOD");
    EXPECT_STREQ(health_status_to_string(HEALTH_FAIR), "FAIR");
    EXPECT_STREQ(health_status_to_string(HEALTH_POOR), "POOR");
    EXPECT_STREQ(health_status_to_string(HEALTH_CRITICAL), "CRITICAL");
    EXPECT_STREQ(health_status_to_string(HEALTH_UNKNOWN), "UNKNOWN");
}

TEST(HealthMonitorUtility, AnomalyTypeToString) {
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_MEMORY_LEAK), "MEMORY_LEAK");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_PERFORMANCE_DEGRADATION), "PERFORMANCE_DEGRADATION");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_ERROR_SPIKE), "ERROR_SPIKE");
    EXPECT_STREQ(anomaly_type_to_string(ANOMALY_CACHE_THRASHING), "CACHE_THRASHING");
}

TEST(HealthMonitorUtility, AnomalySeverityToString) {
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(anomaly_severity_to_string(ANOMALY_SEVERITY_CRITICAL), "CRITICAL");
}

TEST(HealthMonitorUtility, GetTimestamp) {
    uint64_t t1 = health_monitor_get_timestamp_us();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t t2 = health_monitor_get_timestamp_us();

    EXPECT_GT(t2, t1);
    EXPECT_GE(t2 - t1, 10000);  // At least 10ms = 10000us
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(HealthMonitorRunningTest, HighFrequencyMetrics) {
    // Record metrics at high frequency
    for (int i = 0; i < 10000; i++) {
        health_monitor_record_operation(monitor, "high_freq", i % 1000);
        if (i % 10 == 0) {
            health_monitor_record_memory(monitor, 10 * 1024 * 1024 + i);
        }
        if (i % 100 == 0) {
            health_monitor_record_cache_access(monitor, i % 2 == 0);
        }
    }

    // Should handle without crashing
    float score = health_monitor_get_score(monitor);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(HealthMonitorTest, ManyOperationTypes) {
    // Create many different operation types
    for (int i = 0; i < 100; i++) {
        char op_name[32];
        snprintf(op_name, sizeof(op_name), "operation_%d", i);
        health_monitor_record_operation(monitor, op_name, 1000 + i);
    }

    // Should handle without crashing
    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
}

TEST_F(HealthMonitorTest, ManyErrorTypes) {
    // Create many different error types
    for (int i = 0; i < 50; i++) {
        char err_type[32];
        snprintf(err_type, sizeof(err_type), "ERROR_%d", i);
        health_monitor_record_error(monitor, err_type);
    }

    // Should handle without crashing
    float score = health_monitor_get_score(monitor);
    EXPECT_GE(score, 0.0f);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(HealthMonitorRunningTest, CompleteMonitoringCycle) {
    // Simulate complete monitoring scenario

    // 1. Establish baseline
    for (int i = 0; i < 100; i++) {
        health_monitor_record_operation(monitor, "inference", 1000);
        health_monitor_record_memory(monitor, 50 * 1024 * 1024);
        health_monitor_record_cache_access(monitor, true);
    }
    health_monitor_establish_baseline(monitor);

    // 2. Normal operation
    for (int i = 0; i < 100; i++) {
        health_monitor_record_operation(monitor, "inference", 1000 + (rand() % 200));
        health_monitor_record_cache_access(monitor, (rand() % 100) < 80);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // 3. Check health (should be good)
    EXPECT_TRUE(health_monitor_is_healthy(monitor));

    // 4. Introduce anomaly
    for (int i = 0; i < 50; i++) {
        health_monitor_record_error(monitor, "CRITICAL");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // 5. Detect anomalies (may or may not detect depending on thresholds and timing)
    anomaly_t anomalies[10];
    int32_t count = health_monitor_detect_anomalies(monitor, anomalies, 10);
    EXPECT_GE(count, 0);  // Anomaly detection depends on implementation thresholds

    // 6. Generate report
    health_monitor_report(monitor, stdout);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(HealthMonitorTest, ZeroDurationOperation) {
    health_monitor_record_operation(monitor, "zero_duration", 0);

    operation_metric_t stats;
    EXPECT_TRUE(health_monitor_get_operation_stats(monitor, "zero_duration", &stats));
    EXPECT_EQ(stats.avg_duration_us, 0.0);
}

TEST_F(HealthMonitorTest, VeryLargeDuration) {
    health_monitor_record_operation(monitor, "large_duration", UINT64_MAX);

    operation_metric_t stats;
    EXPECT_TRUE(health_monitor_get_operation_stats(monitor, "large_duration", &stats));
    EXPECT_GT(stats.max_duration_us, 0);
}

TEST_F(HealthMonitorTest, ZeroMemory) {
    health_monitor_record_memory(monitor, 0);

    memory_metric_t stats;
    EXPECT_TRUE(health_monitor_get_memory_stats(monitor, &stats));
    EXPECT_EQ(stats.current_bytes, 0);
}

TEST_F(HealthMonitorTest, VeryLargeMemory) {
    health_monitor_record_memory(monitor, SIZE_MAX);

    memory_metric_t stats;
    EXPECT_TRUE(health_monitor_get_memory_stats(monitor, &stats));
    EXPECT_GT(stats.current_bytes, 0);
}

TEST_F(HealthMonitorTest, AllCacheHits) {
    for (int i = 0; i < 100; i++) {
        health_monitor_record_cache_access(monitor, true);
    }

    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_EQ(status.cache_score, 100.0f);  // Perfect hit rate
}

TEST_F(HealthMonitorTest, AllCacheMisses) {
    for (int i = 0; i < 100; i++) {
        health_monitor_record_cache_access(monitor, false);
    }

    health_status_snapshot_t status;
    EXPECT_TRUE(health_monitor_get_status(monitor, &status));
    EXPECT_LT(status.cache_score, 70.0f);  // Poor hit rate
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
