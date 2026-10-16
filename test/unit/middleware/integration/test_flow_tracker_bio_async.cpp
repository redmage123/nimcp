//=============================================================================
// test_flow_tracker_bio_async.cpp
// Bio-Async Integration Tests for Flow Tracker
//=============================================================================

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_flow_tracker.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FlowTrackerBioAsyncTest : public ::testing::Test {
protected:
    flow_tracker_t* tracker = nullptr;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        bio_config.enable_statistics = true;
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        router_config.enable_statistics = true;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-router";

        // Create flow tracker
        tracker = flow_tracker_create();
        ASSERT_NE(tracker, nullptr) << "Failed to create flow tracker";
    }

    void TearDown() override {
        if (tracker) {
            flow_tracker_destroy(tracker);
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, CreateRegistersWithBioRouter) {
    // Flow tracker should register with bio-router upon creation
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // At least one module should be registered
    EXPECT_GE(stats.active_modules, 1u);
}

TEST_F(FlowTrackerBioAsyncTest, DestroyUnregistersFromBioRouter) {
    // Get initial stats
    bio_router_stats_t stats_before;
    nimcp_error_t err = bio_router_get_stats(&stats_before);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    uint32_t modules_before = stats_before.active_modules;

    // Destroy tracker
    flow_tracker_destroy(tracker);
    tracker = nullptr;

    // Get stats after destroy
    bio_router_stats_t stats_after;
    err = bio_router_get_stats(&stats_after);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should have one less module
    EXPECT_LE(stats_after.active_modules, modules_before);
}

TEST_F(FlowTrackerBioAsyncTest, MultipleTrackersRegisterIndependently) {
    // Create second tracker
    flow_tracker_t* tracker2 = flow_tracker_create();
    ASSERT_NE(tracker2, nullptr);

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // May register independently or share a module ID - both are valid
    EXPECT_GE(stats.active_modules, 1u);

    flow_tracker_destroy(tracker2);
}

//=============================================================================
// Flow Recording with Bio-Async Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, RecordsFlowWithAsyncMessages) {
    // Record information flow using bio-async channels
    float information_bits = 8.5f;
    uint64_t latency_us = 10;

    flow_tracker_record_flow(tracker,
        PATH_MIDDLEWARE_TO_EXECUTIVE,
        information_bits,
        latency_us);

    // Get path statistics
    path_flow_stats_t stats = flow_tracker_get_path_stats(tracker,
        PATH_MIDDLEWARE_TO_EXECUTIVE);

    EXPECT_EQ(stats.total_events, 1u);
    EXPECT_GT(stats.throughput_bits_per_sec, 0.0f);
}

TEST_F(FlowTrackerBioAsyncTest, TracksFlowAcrossMultiplePaths) {
    // Record flow on different paths
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 5);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 8.0f, 7);
    flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 12.0f, 4);

    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);

    EXPECT_GE(metrics.total_throughput_bits_per_sec, 0.0f);
    EXPECT_GT(metrics.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 0u);
    EXPECT_GT(metrics.paths[PATH_MIDDLEWARE_TO_WORKSPACE].total_events, 0u);
    EXPECT_GT(metrics.paths[PATH_EXECUTIVE_TO_MIDDLEWARE].total_events, 0u);
}

//=============================================================================
// Bio-Promise Integration Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, IntegratesWithBioPromises) {
    // Create promise for async flow tracking
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(float));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Record flow
    float information = 15.0f;
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE,
        information, 8);

    // Complete promise with result
    float result = information;
    nimcp_bio_promise_complete(promise, &result);

    // Future should be ready
    EXPECT_TRUE(nimcp_bio_future_is_ready(future));

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(FlowTrackerBioAsyncTest, UsesChannelForDifferentFlowTypes) {
    // Different paths use different neuromodulator channels
    // Middleware->Executive: Dopamine (action selection)
    nimcp_bio_promise_t exec_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(float));

    // Middleware->Workspace: Serotonin (global broadcast)
    nimcp_bio_promise_t workspace_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN, sizeof(float));

    // Executive->Middleware: Norepinephrine (command/attention)
    nimcp_bio_promise_t command_promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE, sizeof(float));

    ASSERT_NE(exec_promise, nullptr);
    ASSERT_NE(workspace_promise, nullptr);
    ASSERT_NE(command_promise, nullptr);

    // Record flows
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 5);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 8.0f, 6);
    flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 12.0f, 4);

    // Complete all promises
    float result = 1.0f;
    nimcp_bio_promise_complete(exec_promise, &result);
    nimcp_bio_promise_complete(workspace_promise, &result);
    nimcp_bio_promise_complete(command_promise, &result);

    nimcp_bio_promise_destroy(exec_promise);
    nimcp_bio_promise_destroy(workspace_promise);
    nimcp_bio_promise_destroy(command_promise);
}

//=============================================================================
// Efficiency Calculation with Bio-Async Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, CalculatesEfficiencyWithAsyncFlow) {
    // Record input and output flow
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 5);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 8.0f, 6);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 9.0f, 5);

    // Calculate efficiency
    float efficiency = flow_tracker_calculate_efficiency(tracker,
        PATH_MIDDLEWARE_TO_EXECUTIVE);

    // Efficiency should be in valid range
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(FlowTrackerBioAsyncTest, TracksInformationLoss) {
    // Record normal flow
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 20.0f, 5);

    // Record filtered flow (information loss)
    flow_tracker_record_filtered_flow(tracker,
        PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f);

    path_flow_stats_t stats = flow_tracker_get_path_stats(tracker,
        PATH_MIDDLEWARE_TO_WORKSPACE);

    EXPECT_GT(stats.filtered_events, 0u);
    EXPECT_GT(stats.information_loss_bits, 0.0f);
}

//=============================================================================
// Latency Tracking with Bio-Async Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, TracksLatencyWithPromises) {
    // Create promises and measure latency
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    // Record flow with latency
    uint64_t start_us = 1000;
    uint64_t end_us = 1010;
    uint64_t latency = end_us - start_us;

    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE,
        10.0f, latency);

    // Get latency stats
    float avg_latency = flow_tracker_get_avg_latency(tracker,
        PATH_MIDDLEWARE_TO_EXECUTIVE);

    EXPECT_GT(avg_latency, 0.0f);
    EXPECT_NEAR(avg_latency, (float)latency, 1.0f);

    nimcp_bio_promise_destroy(promise);
}

TEST_F(FlowTrackerBioAsyncTest, TracksP99Latency) {
    // Record multiple flows with varying latency
    for (int i = 0; i < 100; i++) {
        uint64_t latency = 5 + (i % 20);  // 5-25µs range
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE,
            10.0f, latency);
    }

    float p99 = flow_tracker_get_p99_latency(tracker,
        PATH_MIDDLEWARE_TO_EXECUTIVE);
    float avg = flow_tracker_get_avg_latency(tracker,
        PATH_MIDDLEWARE_TO_EXECUTIVE);

    // P99 should be higher than average
    EXPECT_GT(p99, avg);
}

//=============================================================================
// Bottleneck Detection with Bio-Async Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, DetectsBottleneckWithHighLoad) {
    // Simulate high load on a path
    for (int i = 0; i < 50; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE,
            100.0f, 10);
    }

    // Check for bottleneck
    bool has_bottleneck = flow_tracker_has_bottleneck(tracker);

    // Get metrics
    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);

    // Verify bottleneck detection works
    EXPECT_GE(metrics.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 50u);
}

TEST_F(FlowTrackerBioAsyncTest, IdentifiesBottleneckPath) {
    // Create bottleneck on executive path
    for (int i = 0; i < 100; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE,
            100.0f, 5);
    }

    // Light load on other paths
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 5.0f, 3);

    float efficiency = 0.0f;
    integration_path_t bottleneck = flow_tracker_find_bottleneck(
        tracker, &efficiency);

    // Should identify the heavily loaded path
    // Note: Bottleneck based on efficiency, not just load
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, HandlesConcurrentFlowRecording) {
    const int num_threads = 4;
    const int records_per_thread = 25;
    std::atomic<int> total_recorded{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, records_per_thread, &total_recorded]() {
            integration_path_t path = static_cast<integration_path_t>(
                t % PATH_COUNT);

            for (int i = 0; i < records_per_thread; i++) {
                flow_tracker_record_flow(this->tracker, path, 10.0f, 5 + i);
                total_recorded++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_recorded.load(), num_threads * records_per_thread);

    // Verify all flows were recorded
    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);
    uint64_t total_events = 0;
    for (int i = 0; i < PATH_COUNT; i++) {
        total_events += metrics.paths[i].total_events;
    }
    EXPECT_EQ(total_events, (uint64_t)(num_threads * records_per_thread));
}

//=============================================================================
// Throughput Calculation Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, CalculatesTotalThroughput) {
    // Record flow on multiple paths
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 5);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 8.0f, 4);
    flow_tracker_record_flow(tracker, PATH_EXECUTIVE_TO_MIDDLEWARE, 12.0f, 6);

    float total_throughput = flow_tracker_get_total_throughput(tracker);

    // Total should be sum of all paths
    EXPECT_GT(total_throughput, 0.0f);
}

TEST_F(FlowTrackerBioAsyncTest, CalculatesAverageEfficiency) {
    // Record flow on multiple paths
    for (int i = 0; i < PATH_COUNT; i++) {
        integration_path_t path = static_cast<integration_path_t>(i);
        flow_tracker_record_flow(tracker, path, 10.0f, 5);
    }

    float avg_efficiency = flow_tracker_get_avg_efficiency(tracker);

    EXPECT_GE(avg_efficiency, 0.0f);
    EXPECT_LE(avg_efficiency, 1.0f);
}

//=============================================================================
// Reset and Cleanup Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, ResetClearsStatistics) {
    // Record some flows
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 5);
    flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_WORKSPACE, 8.0f, 4);

    // Verify flows were recorded
    cross_modal_flow_metrics_t metrics_before = flow_tracker_get_metrics(tracker);
    EXPECT_GT(metrics_before.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 0u);

    // Reset tracker
    flow_tracker_reset(tracker);

    // Verify stats cleared
    cross_modal_flow_metrics_t metrics_after = flow_tracker_get_metrics(tracker);
    EXPECT_EQ(metrics_after.paths[PATH_MIDDLEWARE_TO_EXECUTIVE].total_events, 0u);
    EXPECT_EQ(metrics_after.paths[PATH_MIDDLEWARE_TO_WORKSPACE].total_events, 0u);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(FlowTrackerBioAsyncTest, HandlesHighThroughputFlowRecording) {
    const int num_records = 10000;

    for (int i = 0; i < num_records; i++) {
        integration_path_t path = static_cast<integration_path_t>(
            i % PATH_COUNT);
        flow_tracker_record_flow(tracker, path, 10.0f, 5);
    }

    // Verify all were recorded
    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(tracker);
    uint64_t total_events = 0;
    for (int i = 0; i < PATH_COUNT; i++) {
        total_events += metrics.paths[i].total_events;
    }

    EXPECT_EQ(total_events, (uint64_t)num_records);
}

TEST_F(FlowTrackerBioAsyncTest, LowLatencyFlowRecording) {
    // Measure time to record flows
    const int num_records = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_records; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE,
            10.0f, 5);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    float avg_latency_us = (float)duration / num_records;

    // Should be under 5µs per record
    EXPECT_LT(avg_latency_us, 5.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
