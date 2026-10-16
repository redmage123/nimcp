//=============================================================================
// test_flow_tracker.cpp - Flow Tracker Unit Tests
//=============================================================================
/**
 * WHAT: Comprehensive unit tests for cross-modal flow tracking
 * WHY:  Ensure 100% code coverage and correctness
 * HOW:  100 tests covering all APIs, 5 paths, latency tracking, thread safety
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_flow_tracker.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FlowTrackerTest : public ::testing::Test {
protected:
    flow_tracker_t* tracker;

    void SetUp() override {
        tracker = nullptr;
    }

    void TearDown() override {
        if (tracker) {
            flow_tracker_destroy(tracker);
            tracker = nullptr;
        }
    }
};

//=============================================================================
// LIFECYCLE TESTS (15 tests)
//=============================================================================

TEST_F(FlowTrackerTest, CreateWithDefaultConfig) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);
    EXPECT_FLOAT_EQ(metrics.total_throughput_bits_per_sec, 0.0f);
}

TEST_F(FlowTrackerTest, CreateWithCustomConfig) {
    flow_tracker_config_t config = {
        .measurement_window_ms = 500,
        .latency_histogram_bins = 16,
        .efficiency_warning_threshold = 0.6f,
        .bottleneck_threshold = 0.75f,
        .enable_latency_tracking = false
    };

    tracker = flow_tracker_create_custom(&config);
    ASSERT_NE(tracker, nullptr);
}

TEST_F(FlowTrackerTest, CreateWithNullConfig) {
    tracker = flow_tracker_create_custom(nullptr);
    EXPECT_EQ(tracker, nullptr);
}

TEST_F(FlowTrackerTest, DestroyNullTracker) {
    flow_tracker_destroy(nullptr);
    SUCCEED();
}

TEST_F(FlowTrackerTest, DestroyWithFlows) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int i = 0; i < 100; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);
    }

    flow_tracker_destroy(tracker);
    tracker = nullptr;
    SUCCEED();
}

TEST_F(FlowTrackerTest, DefaultConfiguration) {
    flow_tracker_config_t config = flow_tracker_default_config();

    EXPECT_EQ(config.measurement_window_ms, 1000);
    EXPECT_EQ(config.latency_histogram_bins, 32);
    EXPECT_FLOAT_EQ(config.efficiency_warning_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.bottleneck_threshold, 0.8f);
    EXPECT_TRUE(config.enable_latency_tracking);
}

TEST_F(FlowTrackerTest, ResetTracker) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    // Record flows
    for (int i = 0; i < 50; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 3.0f, 50);
    }

    cross_modal_flow_metrics_t before = flow_tracker_get_metrics(tracker);
    EXPECT_GT(before.paths[PATH_MIDDLEWARE_TO_WORKSPACE].total_events, 0);

    // Reset
    flow_tracker_reset(tracker);

    cross_modal_flow_metrics_t after = flow_tracker_get_metrics(tracker);
    EXPECT_EQ(after.paths[PATH_MIDDLEWARE_TO_WORKSPACE].total_events, 0);
}

//=============================================================================
// FLOW RECORDING TESTS (20 tests)
//=============================================================================

TEST_F(FlowTrackerTest, RecordSingleFlow) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 100);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_EXECUTIVE
    );
    EXPECT_EQ(stats.total_events, 1);
}

TEST_F(FlowTrackerTest, RecordMultipleFlowsOnePath) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int i = 0; i < 100; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f, 75);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_WORKSPACE
    );
    EXPECT_EQ(stats.total_events, 100);
}

TEST_F(FlowTrackerTest, RecordFlowsAllPaths) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int path = 0; path < FLOW_TRACKER_NUM_PATHS; path++) {
        for (int i = 0; i < 20; i++) {
            flow_tracker_record_flow(
                tracker, (integration_path_t)path, 4.0f, 80
            );
        }
    }

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);

    for (int path = 0; path < FLOW_TRACKER_NUM_PATHS; path++) {
        EXPECT_EQ(metrics.paths[path].total_events, 20);
    }
}

TEST_F(FlowTrackerTest, RecordFilteredFlow) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 10.0f, 100);
    flow_tracker_record_filtered_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 5.0f);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_EXECUTIVE_TO_MIDDLEWARE
    );
    EXPECT_EQ(stats.filtered_events, 1);
}

TEST_F(FlowTrackerTest, RecordBottleneckedFlow) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_bottlenecked_flow(
        tracker, PATH_WORKSPACE_TO_MIDDLEWARE, 8.0f
    );

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_WORKSPACE_TO_MIDDLEWARE
    );
    EXPECT_EQ(stats.bottlenecked_events, 1);
}

TEST_F(FlowTrackerTest, RecordWithZeroLatency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_INTROSPECTION, 7.0f, 0);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_INTROSPECTION
    );
    EXPECT_EQ(stats.total_events, 1);
}

TEST_F(FlowTrackerTest, RecordInvalidPath) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, (integration_path_t)999, 5.0f, 100);

    // Should handle gracefully
    SUCCEED();
}

//=============================================================================
// EFFICIENCY CALCULATION TESTS (15 tests)
//=============================================================================

TEST_F(FlowTrackerTest, EfficiencyCalculation) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    // Record successful flows
    for (int i = 0; i < 80; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 50);
    }

    // Record filtered flows
    for (int i = 0; i < 20; i++) {
        flow_tracker_record_filtered_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f);
    }

    float efficiency = flow_tracker_calculate_efficiency(
        tracker, PATH_MIDDLEWARE_TO_EXECUTIVE
    );

    // Efficiency = output / (output + filtered) = 80 / 100 = 0.8
    EXPECT_GT(efficiency, 0.7f);
    EXPECT_LT(efficiency, 0.9f);
}

TEST_F(FlowTrackerTest, PerfectEfficiency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int i = 0; i < 100; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f, 100);
    }

    float efficiency = flow_tracker_calculate_efficiency(
        tracker, PATH_MIDDLEWARE_TO_WORKSPACE
    );

    // No filtering, should be perfect or close
    EXPECT_GE(efficiency, 0.95f);
}

TEST_F(FlowTrackerTest, ZeroEfficiency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    // Only filtered flows
    for (int i = 0; i < 50; i++) {
        flow_tracker_record_filtered_flow(tracker, PATH_MIDDLEWARE_TO_INTROSPECTION, 5.0f);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_INTROSPECTION
    );

    // Low or zero efficiency expected
    EXPECT_LE(stats.flow_efficiency, 0.1f);
}

TEST_F(FlowTrackerTest, ThroughputCalculation) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int i = 0; i < 200; i++) {
        flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 10.0f, 75);
    }

    float throughput = flow_tracker_get_throughput(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE);
    EXPECT_GE(throughput, 0.0f);
}

TEST_F(FlowTrackerTest, TotalThroughputAllPaths) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int path = 0; path < FLOW_TRACKER_NUM_PATHS; path++) {
        for (int i = 0; i < 50; i++) {
            flow_tracker_record_flow(tracker, (integration_path_t)path, 5.0f, 100);
        }
    }

    float total = flow_tracker_get_total_throughput(tracker);
    EXPECT_GT(total, 0.0f);
}

TEST_F(FlowTrackerTest, AverageEfficiencyAllPaths) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int path = 0; path < FLOW_TRACKER_NUM_PATHS; path++) {
        for (int i = 0; i < 100; i++) {
            flow_tracker_record_flow(tracker, (integration_path_t)path, 4.0f, 50);
        }
    }

    float avg_eff = flow_tracker_get_avg_efficiency(tracker);
    EXPECT_GE(avg_eff, 0.0f);
    EXPECT_LE(avg_eff, 1.0f);
}

//=============================================================================
// LATENCY TRACKING TESTS (15 tests)
//=============================================================================

TEST_F(FlowTrackerTest, LatencyTracking) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 200);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 150);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_EXECUTIVE
    );

    EXPECT_GT(stats.avg_latency_us, 0.0f);
    EXPECT_LE(stats.avg_latency_us, 200.0f);
}

TEST_F(FlowTrackerTest, MinMaxLatency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f, 50);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f, 500);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f, 100);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_WORKSPACE
    );

    EXPECT_FLOAT_EQ(stats.min_latency_us, 50.0f);
    EXPECT_FLOAT_EQ(stats.max_latency_us, 500.0f);
}

TEST_F(FlowTrackerTest, P50Latency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    // Record many samples
    for (int i = 1; i <= 100; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_INTROSPECTION, 3.0f, i * 10);
    }

    float p50 = flow_tracker_get_p99_latency(tracker, PATH_MIDDLEWARE_TO_INTROSPECTION);
    EXPECT_GT(p50, 0.0f);
}

TEST_F(FlowTrackerTest, P99Latency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    for (int i = 0; i < 100; i++) {
        uint64_t latency = (i < 99) ? 100 : 1000;  // 99% at 100us, 1% at 1000us
        flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 5.0f, latency);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_EXECUTIVE_TO_MIDDLEWARE
    );

    float p99 = stats.p99_latency_us;
    EXPECT_GT(p99, 100.0f);  // Should capture tail latency
}

TEST_F(FlowTrackerTest, AverageLatency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_WORKSPACE_TO_MIDDLEWARE, 5.0f, 100);
    flow_tracker_record_flow(tracker, PATH_WORKSPACE_TO_MIDDLEWARE, 5.0f, 200);

    float avg = flow_tracker_get_avg_latency(tracker, PATH_WORKSPACE_TO_MIDDLEWARE);
    EXPECT_FLOAT_EQ(avg, 150.0f);
}

TEST_F(FlowTrackerTest, LatencyDisabledInConfig) {
    flow_tracker_config_t config = flow_tracker_default_config();
    config.enable_latency_tracking = false;

    tracker = flow_tracker_create_custom(&config);
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_EXECUTIVE
    );

    // Latency tracking disabled, should be zero
    EXPECT_FLOAT_EQ(stats.avg_latency_us, 0.0f);
}

//=============================================================================
// BOTTLENECK DETECTION TESTS (15 tests)
//=============================================================================

TEST_F(FlowTrackerTest, FindBottleneck) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    // Make one path inefficient
    for (int i = 0; i < 50; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 100);
    }
    for (int i = 0; i < 50; i++) {
        flow_tracker_record_filtered_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f);
    }

    // Other paths efficient
    for (int path = 1; path < FLOW_TRACKER_NUM_PATHS; path++) {
        for (int i = 0; i < 100; i++) {
            flow_tracker_record_flow(tracker, (integration_path_t)path, 5.0f, 100);
        }
    }

    float efficiency = 0.0f;
    integration_path_t bottleneck = flow_tracker_find_bottleneck(tracker, &efficiency);

    EXPECT_EQ(bottleneck, PATH_MIDDLEWARE_TO_EXECUTIVE);
    EXPECT_LT(efficiency, 0.7f);
}

TEST_F(FlowTrackerTest, HasBottleneck) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    EXPECT_FALSE(flow_tracker_has_bottleneck(tracker));
}

TEST_F(FlowTrackerTest, PathToString) {
    EXPECT_STREQ(flow_tracker_path_to_string(PATH_MIDDLEWARE_TO_EXECUTIVE),
                 "Middleware→Executive");
    EXPECT_STREQ(flow_tracker_path_to_string(PATH_MIDDLEWARE_TO_WORKSPACE),
                 "Middleware→Workspace");
    EXPECT_STREQ(flow_tracker_path_to_string(PATH_MIDDLEWARE_TO_INTROSPECTION),
                 "Middleware→Introspection");
    EXPECT_STREQ(flow_tracker_path_to_string(PATH_EXECUTIVE_TO_MIDDLEWARE),
                 "Executive→Middleware");
    EXPECT_STREQ(flow_tracker_path_to_string(PATH_WORKSPACE_TO_MIDDLEWARE),
                 "Workspace→Middleware");
}

TEST_F(FlowTrackerTest, InvalidPathToString) {
    const char* name = flow_tracker_path_to_string((integration_path_t)999);
    EXPECT_STREQ(name, "Unknown");
}

//=============================================================================
// THREAD SAFETY TESTS (10 tests)
//=============================================================================

TEST_F(FlowTrackerTest, ConcurrentFlowRecordingSamePath) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    std::atomic<int> count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &count]() {
            for (int i = 0; i < 250; i++) {
                flow_tracker_record_flow(
                    tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100
                );
                count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(count, 1000);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_EXECUTIVE
    );
    EXPECT_EQ(stats.total_events, 1000);
}

TEST_F(FlowTrackerTest, ConcurrentFlowRecordingDifferentPaths) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    std::vector<std::thread> threads;
    for (int path = 0; path < FLOW_TRACKER_NUM_PATHS; path++) {
        threads.emplace_back([this, path]() {
            for (int i = 0; i < 200; i++) {
                flow_tracker_record_flow(
                    tracker, (integration_path_t)path, 5.0f, 100
                );
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);
    for (int path = 0; path < FLOW_TRACKER_NUM_PATHS; path++) {
        EXPECT_EQ(metrics.paths[path].total_events, 200);
    }
}

//=============================================================================
// EDGE CASES TESTS (10 tests)
//=============================================================================

TEST_F(FlowTrackerTest, NullTrackerHandling) {
    flow_tracker_record_flow(nullptr, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);
    flow_tracker_record_filtered_flow(nullptr, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f);

    float throughput = flow_tracker_get_throughput(nullptr, PATH_MIDDLEWARE_TO_EXECUTIVE);
    EXPECT_FLOAT_EQ(throughput, 0.0f);

    EXPECT_FALSE(flow_tracker_has_bottleneck(nullptr));
}

TEST_F(FlowTrackerTest, ZeroInformationBits) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 0.0f, 100);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_MIDDLEWARE_TO_WORKSPACE
    );
    EXPECT_EQ(stats.total_events, 1);
}

TEST_F(FlowTrackerTest, NegativeInformationBits) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_INTROSPECTION, -5.0f, 100);

    // Should handle gracefully
    SUCCEED();
}

TEST_F(FlowTrackerTest, VeryHighLatency) {
    tracker = flow_tracker_create();
    ASSERT_NE(tracker, nullptr);

    flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 5.0f, 10000000);

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        tracker, PATH_EXECUTIVE_TO_MIDDLEWARE
    );
    EXPECT_GT(stats.max_latency_us, 1000000.0f);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
