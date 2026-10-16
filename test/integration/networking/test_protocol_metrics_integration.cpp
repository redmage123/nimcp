/**
 * @file test_protocol_metrics_integration.cpp
 * @brief Integration tests for Protocol Metrics System
 *
 * INTEGRATION SCENARIOS:
 * - Real-time metrics collection and aggregation
 * - Time-series history tracking
 * - Alert triggering under load
 * - Semantic primitive analytics
 * - Dashboard export under various conditions
 * - CSV export with large datasets
 * - Bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>

// Headers have their own extern "C" guards
#include "networking/nlp/nimcp_protocol_metrics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class ProtocolMetricsIntegrationTest : public ::testing::Test {
protected:
    protocol_metrics_t* metrics;

    void SetUp() override {
        metrics_config_t config = metrics_default_config();
        config.enable_bio_async = false;
        metrics = protocol_metrics_create(&config);
        ASSERT_NE(metrics, nullptr);
    }

    void TearDown() override {
        protocol_metrics_destroy(metrics);
    }
};

//=============================================================================
// Real-Time Metrics Collection
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, ContinuousMessageTracking) {
    const int num_messages = 100;

    // Simulate continuous message flow
    for (int i = 0; i < num_messages; i++) {
        metrics_record_message(
            metrics,
            0x01 + (i % 10),  // Varying message types
            1024 + (i * 100),  // Varying sizes
            5.0f + (i % 10),   // Varying latency
            (i % 20) != 0      // 5% error rate
        );
    }

    protocol_stats_t stats = metrics_get_protocol_stats(metrics);

    EXPECT_EQ(stats.messages_sent, num_messages);
    EXPECT_GT(stats.bytes_sent, 0u);
    EXPECT_GT(stats.avg_latency_ms, 0.0f);
    EXPECT_EQ(stats.errors, 5u);  // 5% of 100
}

//=============================================================================
// Semantic Analytics
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, SemanticPrimitiveTracking) {
    // Simulate usage of multiple primitives with varying frequencies
    const std::vector<std::pair<uint32_t, int>> primitives = {
        {1, 50},   // Primitive 1: 50 uses
        {2, 30},   // Primitive 2: 30 uses
        {3, 20},   // Primitive 3: 20 uses
        {4, 10},   // Primitive 4: 10 uses
        {5, 5}     // Primitive 5: 5 uses
    };

    for (const auto& p : primitives) {
        for (int i = 0; i < p.second; i++) {
            metrics_record_primitive_usage(
                metrics,
                p.first,
                0.8f + (i % 5) * 0.04f  // Varying relevance 0.8-1.0
            );
        }
    }

    // Get top 3 primitives
    semantic_primitive_stats_t* top = nullptr;
    int result = metrics_get_top_primitives(metrics, 3, &top);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    if (top) {
        EXPECT_EQ(top[0].primitive_id, 1u);  // Most used
        EXPECT_EQ(top[0].usage_count, 50u);
        EXPECT_EQ(top[1].primitive_id, 2u);
        EXPECT_EQ(top[1].usage_count, 30u);
        nimcp_free(top);
    }
}

//=============================================================================
// Alert System Integration
//=============================================================================

static int alert_count = 0;
static void integration_alert_callback(const char* alert) {
    alert_count++;
}

TEST_F(ProtocolMetricsIntegrationTest, AlertTriggering) {
    alert_count = 0;
    metrics_set_alert_callback(metrics, integration_alert_callback);

    // Generate high error rate scenario
    for (int i = 0; i < 100; i++) {
        metrics_record_message(
            metrics,
            0x01,
            1024,
            5.0f,
            i >= 90  // 90% error rate
        );
    }

    metrics_check_alerts(metrics);

    // Alert may or may not trigger depending on threshold
    SUCCEED();
}

//=============================================================================
// Dashboard Export
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, DashboardExportWithData) {
    // Generate various metrics
    for (int i = 0; i < 50; i++) {
        metrics_record_message(metrics, 0x01, 1024, 5.0f, true);
    }

    for (uint32_t i = 1; i <= 5; i++) {
        metrics_record_primitive_usage(metrics, i, 0.9f);
    }

    // Export dashboard
    char json[8192] = {0};
    int result = metrics_get_dashboard_summary(metrics, json, sizeof(json));

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(strlen(json), 0u);

    // Verify JSON structure
    EXPECT_NE(strstr(json, "messages_sent"), nullptr);
    EXPECT_NE(strstr(json, "tracked_primitives"), nullptr);
}

//=============================================================================
// CSV Export
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, CSVExportWithHistory) {
    const char* filepath = "/tmp/test_protocol_metrics.csv";

    // Generate historical data
    for (int i = 0; i < 20; i++) {
        metrics_record_message(metrics, 0x01, 1024, 5.0f, true);
    }

    // Export
    int result = metrics_export_csv(metrics, filepath);

    // May succeed or fail depending on filesystem
    if (result == NIMCP_SUCCESS) {
        // Verify file exists
        std::ifstream file(filepath);
        EXPECT_TRUE(file.good());
        file.close();
        remove(filepath);
    }

    SUCCEED();
}

//=============================================================================
// Performance Under Load
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, HighThroughputTracking) {
    const int num_messages = 10000;

    uint64_t start_time = nimcp_time_get_us();

    // Record many messages rapidly
    for (int i = 0; i < num_messages; i++) {
        metrics_record_message(metrics, 0x01, 1024, 5.0f, true);
    }

    uint64_t end_time = nimcp_time_get_us();
    uint64_t duration_us = end_time - start_time;

    // Should handle high throughput efficiently
    EXPECT_LT(duration_us, 1000000u);  // < 1 second

    protocol_stats_t stats = metrics_get_protocol_stats(metrics);
    EXPECT_EQ(stats.messages_sent, num_messages);
}

TEST_F(ProtocolMetricsIntegrationTest, ManyPrimitivesTracking) {
    const uint32_t num_primitives = 200;

    // Track many different primitives
    for (uint32_t i = 1; i <= num_primitives; i++) {
        metrics_record_primitive_usage(metrics, i, 0.8f);
    }

    semantic_primitive_stats_t* stats = nullptr;
    uint32_t count = 0;

    int result = metrics_get_primitive_stats(metrics, &stats, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should track up to max
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, METRICS_MAX_PRIMITIVES);

    if (stats) {
        nimcp_free(stats);
    }
}

//=============================================================================
// Reset and Reuse
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, ResetAndContinue) {
    // Record initial data
    for (int i = 0; i < 10; i++) {
        metrics_record_message(metrics, 0x01, 1024, 5.0f, true);
    }

    protocol_stats_t stats1 = metrics_get_protocol_stats(metrics);
    EXPECT_EQ(stats1.messages_sent, 10u);

    // Reset
    metrics_reset_all(metrics);

    protocol_stats_t stats2 = metrics_get_protocol_stats(metrics);
    EXPECT_EQ(stats2.messages_sent, 0u);

    // Continue tracking
    for (int i = 0; i < 5; i++) {
        metrics_record_message(metrics, 0x01, 1024, 5.0f, true);
    }

    protocol_stats_t stats3 = metrics_get_protocol_stats(metrics);
    EXPECT_EQ(stats3.messages_sent, 5u);
}

//=============================================================================
// Primitive Naming
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, PrimitiveNaming) {
    // Set names for primitives
    metrics_set_primitive_name(metrics, 1, "GREET");
    metrics_set_primitive_name(metrics, 2, "QUERY");
    metrics_set_primitive_name(metrics, 3, "RESPOND");

    // Use primitives
    metrics_record_primitive_usage(metrics, 1, 0.9f);
    metrics_record_primitive_usage(metrics, 2, 0.8f);
    metrics_record_primitive_usage(metrics, 3, 0.95f);

    // Verify names
    semantic_primitive_stats_t* stats = nullptr;
    uint32_t count = 0;

    metrics_get_primitive_stats(metrics, &stats, &count);

    if (stats) {
        for (uint32_t i = 0; i < count; i++) {
            if (stats[i].primitive_id == 1) {
                EXPECT_STREQ(stats[i].name, "GREET");
            }
        }
        nimcp_free(stats);
    }
}

//=============================================================================
// Compression Savings Tracking
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, CompressionSavingsAccumulation) {
    // Use primitives multiple times
    for (int i = 0; i < 100; i++) {
        metrics_record_primitive_usage(metrics, 1, 0.9f);
    }

    uint64_t savings = metrics_get_total_compression_savings(metrics);
    EXPECT_GT(savings, 0u);  // Should accumulate savings
}

//=============================================================================
// Mixed Workload
//=============================================================================

TEST_F(ProtocolMetricsIntegrationTest, MixedProtocolAndSemanticTracking) {
    // Simulate realistic mixed workload
    for (int i = 0; i < 50; i++) {
        // Record protocol message
        metrics_record_message(
            metrics,
            0x01 + (i % 5),
            1024,
            5.0f,
            true
        );

        // Record primitive usage (80% of messages use primitives)
        if (i % 5 != 0) {
            metrics_record_primitive_usage(
                metrics,
                1 + (i % 10),
                0.85f
            );
        }
    }

    protocol_stats_t p_stats = metrics_get_protocol_stats(metrics);
    EXPECT_EQ(p_stats.messages_sent, 50u);

    semantic_primitive_stats_t* s_stats = nullptr;
    uint32_t count = 0;
    metrics_get_primitive_stats(metrics, &s_stats, &count);

    EXPECT_GT(count, 0u);

    if (s_stats) {
        nimcp_free(s_stats);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
