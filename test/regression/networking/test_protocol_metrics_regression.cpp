/**
 * @file test_protocol_metrics_regression.cpp
 * @brief Regression tests for Protocol Metrics System
 *
 * REGRESSION FOCUS:
 * - Memory leak prevention
 * - Performance degradation detection
 * - Data accuracy over time
 * - API stability
 * - Edge case handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_protocol_metrics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class ProtocolMetricsRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Memory Leak Regression Tests
//=============================================================================

TEST_F(ProtocolMetricsRegressionTest, NoLeaksOnRepeatedCreationDestruction) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;

    for (int iteration = 0; iteration < 100; iteration++) {
        auto* pm = protocol_metrics_create(&config);
        ASSERT_NE(pm, nullptr);

        // Use metrics
        for (int i = 0; i < 10; i++) {
            metrics_record_message(pm, 0x01, 1024, 5.0f, true);
            metrics_record_primitive_usage(pm, i, 0.8f);
        }

        protocol_metrics_destroy(pm);
    }

    SUCCEED();
}

TEST_F(ProtocolMetricsRegressionTest, NoLeaksOnStatsQueries) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    for (int iteration = 0; iteration < 100; iteration++) {
        metrics_record_primitive_usage(pm, iteration % 10, 0.8f);

        semantic_primitive_stats_t* stats = nullptr;
        uint32_t count = 0;

        int result = metrics_get_primitive_stats(pm, &stats, &count);
        if (result == NIMCP_SUCCESS && stats) {
            nimcp_free(stats);
        }
    }

    protocol_metrics_destroy(pm);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(ProtocolMetricsRegressionTest, MessageRecordingPerformance) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    const int num_messages = 10000;
    uint64_t start_time = nimcp_time_get_us();

    for (int i = 0; i < num_messages; i++) {
        metrics_record_message(pm, 0x01, 1024, 5.0f, true);
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_us = end_time - start_time;

    // Should handle high throughput (baseline: < 1 second for 10k messages)
    EXPECT_LT(duration_us, 1000000u);

    protocol_metrics_destroy(pm);
}

TEST_F(ProtocolMetricsRegressionTest, PrimitiveTrackingPerformance) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    const int num_primitives = 100;
    uint64_t start_time = nimcp_time_get_us();

    for (int i = 0; i < num_primitives; i++) {
        metrics_record_primitive_usage(pm, i, 0.8f);
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_us = end_time - start_time;

    // Should be fast (baseline: < 10ms)
    EXPECT_LT(duration_us, 10000u);

    protocol_metrics_destroy(pm);
}

//=============================================================================
// Data Accuracy Regression Tests
//=============================================================================

TEST_F(ProtocolMetricsRegressionTest, MessageCountAccuracy) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    const int expected_count = 100;

    for (int i = 0; i < expected_count; i++) {
        metrics_record_message(pm, 0x01, 1024, 5.0f, true);
    }

    protocol_stats_t stats = metrics_get_protocol_stats(pm);
    EXPECT_EQ(stats.messages_sent, expected_count);

    protocol_metrics_destroy(pm);
}

TEST_F(ProtocolMetricsRegressionTest, LatencyAverageAccuracy) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    // Record messages with known latencies
    metrics_record_message(pm, 0x01, 1024, 10.0f, true);
    metrics_record_message(pm, 0x01, 1024, 20.0f, true);
    metrics_record_message(pm, 0x01, 1024, 30.0f, true);

    protocol_stats_t stats = metrics_get_protocol_stats(pm);

    // Average should be 20.0
    EXPECT_FLOAT_EQ(stats.avg_latency_ms, 20.0f);

    protocol_metrics_destroy(pm);
}

TEST_F(ProtocolMetricsRegressionTest, ErrorCountAccuracy) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    const int total = 100;
    const int errors = 10;

    for (int i = 0; i < total; i++) {
        metrics_record_message(pm, 0x01, 1024, 5.0f, i >= errors);
    }

    protocol_stats_t stats = metrics_get_protocol_stats(pm);
    EXPECT_EQ(stats.errors, errors);

    protocol_metrics_destroy(pm);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(ProtocolMetricsRegressionTest, NullParameterHandling) {
    // All null parameter cases should fail gracefully
    // protocol_metrics_destroy returns void - just verify it doesn't crash
    protocol_metrics_destroy(nullptr);
    EXPECT_NE(metrics_record_message(nullptr, 0x01, 1024, 5.0f, true), NIMCP_SUCCESS);
    EXPECT_NE(metrics_record_primitive_usage(nullptr, 1, 0.8f), NIMCP_SUCCESS);

    protocol_stats_t stats = metrics_get_protocol_stats(nullptr);
    EXPECT_EQ(stats.messages_sent, 0u);

    EXPECT_EQ(metrics_get_uptime_ms(nullptr), 0u);
}

TEST_F(ProtocolMetricsRegressionTest, ZeroAndNegativeValues) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    // Should handle edge case values gracefully
    metrics_record_message(pm, 0, 0, 0.0f, true);
    metrics_record_primitive_usage(pm, 0, 0.0f);

    SUCCEED();

    protocol_metrics_destroy(pm);
}

TEST_F(ProtocolMetricsRegressionTest, MaxPrimitivesLimit) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    // Try to track more than max
    for (uint32_t i = 0; i < METRICS_MAX_PRIMITIVES + 10; i++) {
        metrics_record_primitive_usage(pm, i, 0.8f);
    }

    semantic_primitive_stats_t* stats = nullptr;
    uint32_t count = 0;

    metrics_get_primitive_stats(pm, &stats, &count);

    // Should not exceed max
    EXPECT_LE(count, METRICS_MAX_PRIMITIVES);

    if (stats) {
        nimcp_free(stats);
    }

    protocol_metrics_destroy(pm);
}

//=============================================================================
// API Stability Regression Tests
//=============================================================================

TEST_F(ProtocolMetricsRegressionTest, DefaultConfigStability) {
    metrics_config_t config1 = metrics_default_config();
    metrics_config_t config2 = metrics_default_config();

    // Default config should be consistent
    EXPECT_EQ(config1.metrics_window_ms, config2.metrics_window_ms);
    EXPECT_EQ(config1.history_depth, config2.history_depth);
    EXPECT_FLOAT_EQ(config1.alert_threshold, config2.alert_threshold);
}

TEST_F(ProtocolMetricsRegressionTest, StatsConsistency) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    // Record same data
    for (int i = 0; i < 10; i++) {
        metrics_record_message(pm, 0x01, 1024, 5.0f, true);
    }

    // Get stats multiple times - should be consistent
    protocol_stats_t stats1 = metrics_get_protocol_stats(pm);
    protocol_stats_t stats2 = metrics_get_protocol_stats(pm);

    EXPECT_EQ(stats1.messages_sent, stats2.messages_sent);
    EXPECT_FLOAT_EQ(stats1.avg_latency_ms, stats2.avg_latency_ms);

    protocol_metrics_destroy(pm);
}

TEST_F(ProtocolMetricsRegressionTest, ResetConsistency) {
    metrics_config_t config = metrics_default_config();
    config.enable_bio_async = false;
    auto* pm = protocol_metrics_create(&config);

    // Record, reset, check multiple times
    for (int iteration = 0; iteration < 10; iteration++) {
        metrics_record_message(pm, 0x01, 1024, 5.0f, true);

        protocol_stats_t stats_before = metrics_get_protocol_stats(pm);
        EXPECT_GT(stats_before.messages_sent, 0u);

        metrics_reset_all(pm);

        protocol_stats_t stats_after = metrics_get_protocol_stats(pm);
        EXPECT_EQ(stats_after.messages_sent, 0u);
    }

    protocol_metrics_destroy(pm);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
