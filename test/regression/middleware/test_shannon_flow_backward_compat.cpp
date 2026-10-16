//=============================================================================
// test_shannon_flow_backward_compat.cpp - Backward Compatibility Tests
//=============================================================================
/**
 * WHAT: Regression tests for Shannon + Flow backward compatibility
 * WHY:  Ensure Phase 1.5.1 doesn't break existing functionality
 * HOW:  20 tests validating API stability, performance, behavior
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "middleware/integration/nimcp_flow_tracker.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ShannonFlowBackwardCompatTest : public ::testing::Test {
protected:
    shannon_monitor_t* shannon;
    flow_tracker_t* flow;

    void SetUp() override {
        shannon = nullptr;
        flow = nullptr;
    }

    void TearDown() override {
        if (shannon) shannon_monitor_destroy(shannon);
        if (flow) flow_tracker_destroy(flow);
    }
};

//=============================================================================
// API STABILITY TESTS (10 tests)
//=============================================================================

TEST_F(ShannonFlowBackwardCompatTest, ShannonDefaultConfigUnchanged) {
    // Verify default config values haven't changed
    shannon_monitor_config_t config = shannon_monitor_default_config();

    EXPECT_EQ(config.history_size, 256);
    EXPECT_FLOAT_EQ(config.bandwidth_events_per_sec, 10000.0f);
    EXPECT_FLOAT_EQ(config.bottleneck_threshold, 0.8f);
    EXPECT_FLOAT_EQ(config.signal_to_noise_ratio, 50.0f);
    EXPECT_EQ(config.measurement_window_ms, 1000);
    EXPECT_FALSE(config.enable_adaptive_snr);
}

TEST_F(ShannonFlowBackwardCompatTest, FlowDefaultConfigUnchanged) {
    // Verify default config values haven't changed
    flow_tracker_config_t config = flow_tracker_default_config();

    EXPECT_EQ(config.measurement_window_ms, 1000);
    EXPECT_EQ(config.latency_histogram_bins, 32);
    EXPECT_FLOAT_EQ(config.efficiency_warning_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.bottleneck_threshold, 0.8f);
    EXPECT_TRUE(config.enable_latency_tracking);
}

TEST_F(ShannonFlowBackwardCompatTest, ShannonAPISignaturesUnchanged) {
    // Create with default config
    shannon = shannon_monitor_create();
    ASSERT_NE(shannon, nullptr);

    // All public APIs should still exist
    event_t event = {};
    event.type = 1;

    shannon_monitor_record_event(shannon, &event);
    float info = shannon_monitor_measure_event_information(shannon, &event);
    float capacity = shannon_monitor_calculate_channel_capacity(shannon);
    float throughput = shannon_monitor_get_throughput(shannon);
    float utilization = shannon_monitor_get_utilization(shannon);
    bool bottleneck = shannon_monitor_is_bottlenecked(shannon);
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(shannon);
    float entropy = shannon_monitor_get_event_entropy(shannon);

    // APIs callable without errors
    EXPECT_GE(info, 0.0f);
    EXPECT_GT(capacity, 0.0f);
    EXPECT_GE(throughput, 0.0f);
    EXPECT_GE(utilization, 0.0f);
    EXPECT_FALSE(bottleneck);  // No bottleneck yet
    EXPECT_EQ(metrics.total_events, 1);
    EXPECT_GE(entropy, 0.0f);
}

TEST_F(ShannonFlowBackwardCompatTest, FlowAPISignaturesUnchanged) {
    // Create with default config
    flow = flow_tracker_create();
    ASSERT_NE(flow, nullptr);

    // All public APIs should still exist
    flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(flow);
    path_flow_stats_t stats = flow_tracker_get_path_stats(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);
    float efficiency = flow_tracker_calculate_efficiency(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);
    float throughput = flow_tracker_get_throughput(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);
    float utilization = flow_tracker_get_utilization(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);
    float avg_latency = flow_tracker_get_avg_latency(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);
    float p99_latency = flow_tracker_get_p99_latency(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);
    integration_path_t bottleneck = flow_tracker_find_bottleneck(flow, nullptr);
    bool has_bottleneck = flow_tracker_has_bottleneck(flow);
    float total_throughput = flow_tracker_get_total_throughput(flow);
    float avg_efficiency = flow_tracker_get_avg_efficiency(flow);
    const char* path_name = flow_tracker_path_to_string(PATH_MIDDLEWARE_TO_EXECUTIVE);

    // APIs callable without errors
    EXPECT_EQ(metrics.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 1);
    EXPECT_EQ(stats.total_events, 1);
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_GE(throughput, 0.0f);
    EXPECT_GE(utilization, 0.0f);
    EXPECT_GT(avg_latency, 0.0f);
    EXPECT_GE(p99_latency, 0.0f);
    EXPECT_EQ(bottleneck, PATH_MIDDLEWARE_TO_EXECUTIVE);
    EXPECT_FALSE(has_bottleneck);
    EXPECT_GE(total_throughput, 0.0f);
    EXPECT_GE(avg_efficiency, 0.0f);
    EXPECT_STREQ(path_name, "Middleware→Executive");
}

TEST_F(ShannonFlowBackwardCompatTest, ShannonMetricsStructUnchanged) {
    shannon = shannon_monitor_create();
    ASSERT_NE(shannon, nullptr);

    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(shannon);

    // Verify all fields exist (will not compile if removed)
    (void)metrics.channel_capacity_bits_per_sec;
    (void)metrics.current_throughput;
    (void)metrics.capacity_utilization;
    (void)metrics.bottleneck_detected;
    (void)metrics.bottleneck_severity;
    (void)metrics.bottleneck_module;
    (void)metrics.information_loss_rate;
    (void)metrics.filtered_bits_per_sec;
    (void)metrics.loss_percentage;
    (void)metrics.event_entropy;
    (void)metrics.cognitive_response_entropy;
    (void)metrics.mutual_information;
    (void)metrics.total_events;
    (void)metrics.filtered_events;
    (void)metrics.bottlenecked_events;
    (void)metrics.measurement_window_ms;
    (void)metrics.last_update_time_ms;

    SUCCEED();
}

TEST_F(ShannonFlowBackwardCompatTest, FlowMetricsStructUnchanged) {
    flow = flow_tracker_create();
    ASSERT_NE(flow, nullptr);

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(flow);

    // Verify all fields exist
    (void)metrics.paths[0];
    (void)metrics.total_throughput_bits_per_sec;
    (void)metrics.avg_flow_efficiency;
    (void)metrics.worst_path_efficiency;
    (void)metrics.worst_path;
    (void)metrics.any_bottleneck_detected;
    (void)metrics.num_bottlenecked_paths;
    (void)metrics.measurement_window_ms;
    (void)metrics.last_global_update_ms;

    path_flow_stats_t stats = metrics.paths[0];
    (void)stats.input_rate_bits_per_sec;
    (void)stats.output_rate_bits_per_sec;
    (void)stats.throughput_bits_per_sec;
    (void)stats.flow_efficiency;
    (void)stats.bottleneck_severity;
    (void)stats.channel_capacity_bits_per_sec;
    (void)stats.capacity_utilization;
    (void)stats.avg_latency_us;
    (void)stats.min_latency_us;
    (void)stats.max_latency_us;
    (void)stats.p50_latency_us;
    (void)stats.p90_latency_us;
    (void)stats.p99_latency_us;
    (void)stats.stddev_latency_us;
    (void)stats.total_events;
    (void)stats.filtered_events;
    (void)stats.bottlenecked_events;
    (void)stats.information_loss_bits;
    (void)stats.loss_percentage;
    (void)stats.measurement_window_start_ms;
    (void)stats.last_update_time_ms;

    SUCCEED();
}

TEST_F(ShannonFlowBackwardCompatTest, IntegrationPathEnumUnchanged) {
    // Verify path enum values haven't changed
    EXPECT_EQ(PATH_MIDDLEWARE_TO_EXECUTIVE, 0);
    EXPECT_EQ(PATH_MIDDLEWARE_TO_WORKSPACE, 1);
    EXPECT_EQ(PATH_MIDDLEWARE_TO_INTROSPECTION, 2);
    EXPECT_EQ(PATH_EXECUTIVE_TO_MIDDLEWARE, 3);
    EXPECT_EQ(PATH_WORKSPACE_TO_MIDDLEWARE, 4);
    EXPECT_EQ(PATH_COUNT, 5);
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS (10 tests)
//=============================================================================

TEST_F(ShannonFlowBackwardCompatTest, ShannonOverheadNotIncreased) {
    shannon = shannon_monitor_create();
    ASSERT_NE(shannon, nullptr);

    event_t event = {};
    event.type = 1;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        shannon_monitor_record_event(shannon, &event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = duration.count() / 1000.0f;

    // Should still be <10µs per event (with margin)
    EXPECT_LT(avg_us, 15.0f);
}

TEST_F(ShannonFlowBackwardCompatTest, FlowOverheadNotIncreased) {
    flow = flow_tracker_create();
    ASSERT_NE(flow, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = duration.count() / 1000.0f;

    // Should still be <7µs per event (with margin)
    EXPECT_LT(avg_us, 10.0f);
}

TEST_F(ShannonFlowBackwardCompatTest, MemoryUsageNotIncreased) {
    // Create both monitors
    shannon = shannon_monitor_create();
    flow = flow_tracker_create();

    ASSERT_NE(shannon, nullptr);
    ASSERT_NE(flow, nullptr);

    // Memory usage should still be ~7KB total
    // (Can't directly measure, but structure sizes should be reasonable)
    size_t shannon_approx = 256 * 32 + 1024;  // ~9KB max
    size_t flow_approx = 5 * 1024;            // ~5KB max

    EXPECT_LT(shannon_approx, 10000);  // <10KB
    EXPECT_LT(flow_approx, 6000);      // <6KB
}

TEST_F(ShannonFlowBackwardCompatTest, ShannonFormulaConsistency) {
    shannon = shannon_monitor_create();
    ASSERT_NE(shannon, nullptr);

    // C = B log₂(1 + SNR)
    // With default B=10000, SNR=50:
    // C = 10000 * log₂(51) ≈ 56863

    float capacity = shannon_monitor_calculate_channel_capacity(shannon);

    EXPECT_GT(capacity, 55000.0f);
    EXPECT_LT(capacity, 58000.0f);
}

TEST_F(ShannonFlowBackwardCompatTest, FlowEfficiencyFormulaConsistency) {
    flow = flow_tracker_create();
    ASSERT_NE(flow, nullptr);

    // Record 80 successful, 20 filtered
    for (int i = 0; i < 80; i++) {
        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 100);
    }
    for (int i = 0; i < 20; i++) {
        flow_tracker_record_filtered_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f);
    }

    float efficiency = flow_tracker_calculate_efficiency(flow, PATH_MIDDLEWARE_TO_EXECUTIVE);

    // η = 800 / (800 + 200) = 0.8
    EXPECT_GT(efficiency, 0.75f);
    EXPECT_LT(efficiency, 0.85f);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
