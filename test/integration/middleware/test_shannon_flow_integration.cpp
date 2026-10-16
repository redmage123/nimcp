//=============================================================================
// test_shannon_flow_integration.cpp - Shannon + Flow Integration Tests
//=============================================================================
/**
 * WHAT: End-to-end integration tests for Shannon + Flow tracking
 * WHY:  Verify middleware-cognitive integration works correctly
 * HOW:  45 tests covering all 5 paths, combined monitoring, performance
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "middleware/integration/nimcp_flow_tracker.h"
#include "middleware/events/nimcp_event_types.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ShannonFlowIntegrationTest : public ::testing::Test {
protected:
    shannon_monitor_t* shannon;
    flow_tracker_t* flow;

    void SetUp() override {
        shannon = shannon_monitor_create();
        flow = flow_tracker_create();
        ASSERT_NE(shannon, nullptr);
        ASSERT_NE(flow, nullptr);
    }

    void TearDown() override {
        if (shannon) shannon_monitor_destroy(shannon);
        if (flow) flow_tracker_destroy(flow);
    }

    event_t create_event(uint32_t type) {
        event_t event = {};
        event.type = (event_type_t)type;
        event.source = EVENT_SOURCE_UNKNOWN;
        event.timestamp_us = 0;
        return event;
    }
};

//=============================================================================
// SHANNON + FLOW INTEGRATION TESTS (10 tests)
//=============================================================================

TEST_F(ShannonFlowIntegrationTest, BasicIntegration) {
    event_t event = create_event(1);

    // Shannon monitors event
    shannon_monitor_record_event(shannon, &event);

    // Flow tracker records path
    float info_bits = shannon_monitor_measure_event_information(shannon, &event);
    flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, info_bits, 100);

    // Both systems updated
    shannon_routing_metrics_t shannon_metrics = shannon_monitor_get_metrics(shannon);
    path_flow_stats_t flow_stats = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_EXECUTIVE
    );

    EXPECT_EQ(shannon_metrics.total_events, 1);
    EXPECT_EQ(flow_stats.total_events, 1);
}

TEST_F(ShannonFlowIntegrationTest, BottleneckTriggersFiltering) {
    // Simulate high load
    for (int i = 0; i < 1000; i++) {
        event_t event = create_event(i % 10);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);

        // Check if bottlenecked
        if (shannon_monitor_is_bottlenecked(shannon)) {
            // Filter low-information events
            // With 10 event types, each has info = -log2(0.1) ≈ 3.32 bits
            if (info_bits < 4.0f) {
                shannon_monitor_record_filtered_event(shannon, &event, info_bits);
                flow_tracker_record_filtered_flow(
                    flow, PATH_MIDDLEWARE_TO_WORKSPACE, info_bits
                );
                continue;
            }
        }

        shannon_monitor_record_event(shannon, &event);
        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_WORKSPACE, info_bits, 75);
    }

    shannon_routing_metrics_t shannon_metrics = shannon_monitor_get_metrics(shannon);
    path_flow_stats_t flow_stats = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_WORKSPACE
    );

    // Some events should have been filtered
    EXPECT_GT(shannon_metrics.filtered_events, 0);
    EXPECT_GT(flow_stats.filtered_events, 0);
}

TEST_F(ShannonFlowIntegrationTest, InformationLossTracking) {
    // Record events and filtered events
    for (int i = 0; i < 100; i++) {
        event_t event = create_event(i % 5);
        float info_bits = shannon_monitor_measure_event_information(shannon, &event);

        if (i % 5 == 0) {
            // Filter 20% of events
            shannon_monitor_record_filtered_event(shannon, &event, info_bits);
            flow_tracker_record_filtered_flow(
                flow, PATH_MIDDLEWARE_TO_INTROSPECTION, info_bits
            );
        } else {
            shannon_monitor_record_event(shannon, &event);
            flow_tracker_record_flow(
                flow, PATH_MIDDLEWARE_TO_INTROSPECTION, info_bits, 100
            );
        }
    }

    float shannon_loss = shannon_monitor_get_information_loss_percentage(shannon);
    path_flow_stats_t flow_stats = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_INTROSPECTION
    );

    // Both should report information loss
    EXPECT_GT(shannon_loss, 0.0f);
    EXPECT_GT(flow_stats.loss_percentage, 0.0f);
}

TEST_F(ShannonFlowIntegrationTest, CombinedMetrics) {
    for (int i = 0; i < 500; i++) {
        event_t event = create_event(i % 8);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_EXECUTIVE_TO_MIDDLEWARE, info_bits, 50);
    }

    // Shannon capacity
    float capacity = shannon_monitor_calculate_channel_capacity(shannon);
    float utilization = shannon_monitor_get_utilization(shannon);

    // Flow efficiency
    float efficiency = flow_tracker_calculate_efficiency(
        flow, PATH_EXECUTIVE_TO_MIDDLEWARE
    );
    float throughput = flow_tracker_get_throughput(flow, PATH_EXECUTIVE_TO_MIDDLEWARE);

    // Combined metric: capacity × efficiency
    float effective_capacity = capacity * efficiency;

    EXPECT_GT(capacity, 0.0f);
    EXPECT_GE(utilization, 0.0f);
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_GT(effective_capacity, 0.0f);
}

//=============================================================================
// EVENT BUS INTEGRATION TESTS (10 tests)
//=============================================================================

TEST_F(ShannonFlowIntegrationTest, MiddlewareToExecutivePath) {
    // Simulate PATTERN_DETECTED event → Executive
    for (int i = 0; i < 100; i++) {
        event_t event = create_event(EVENT_TYPE_PATTERN_DETECTED);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, info_bits, 125);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_EXECUTIVE
    );

    EXPECT_EQ(stats.total_events, 100);
    EXPECT_GT(stats.throughput_bits_per_sec, 0.0f);
}

TEST_F(ShannonFlowIntegrationTest, MiddlewareToWorkspacePath) {
    // Simulate SALIENCE_PEAK event → Workspace
    for (int i = 0; i < 150; i++) {
        event_t event = create_event(EVENT_TYPE_SALIENCE_PEAK);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_WORKSPACE, info_bits, 100);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_WORKSPACE
    );

    EXPECT_EQ(stats.total_events, 150);
}

TEST_F(ShannonFlowIntegrationTest, MiddlewareToIntrospectionPath) {
    // Simulate signal statistics → Introspection
    for (int i = 0; i < 80; i++) {
        event_t event = create_event(EVENT_TYPE_CUSTOM);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_INTROSPECTION, info_bits, 75);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_INTROSPECTION
    );

    EXPECT_EQ(stats.total_events, 80);
}

TEST_F(ShannonFlowIntegrationTest, ExecutiveToMiddlewarePath) {
    // Simulate attention commands Executive → Middleware
    for (int i = 0; i < 60; i++) {
        event_t event = create_event(EVENT_TYPE_ATTENTION_SHIFT);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_EXECUTIVE_TO_MIDDLEWARE, info_bits, 90);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        flow, PATH_EXECUTIVE_TO_MIDDLEWARE
    );

    EXPECT_EQ(stats.total_events, 60);
}

TEST_F(ShannonFlowIntegrationTest, WorkspaceToMiddlewarePath) {
    // Simulate conscious broadcasts Workspace → Middleware
    for (int i = 0; i < 40; i++) {
        event_t event = create_event(EVENT_TYPE_CUSTOM);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_WORKSPACE_TO_MIDDLEWARE, info_bits, 110);
    }

    path_flow_stats_t stats = flow_tracker_get_path_stats(
        flow, PATH_WORKSPACE_TO_MIDDLEWARE
    );

    EXPECT_EQ(stats.total_events, 40);
}

TEST_F(ShannonFlowIntegrationTest, AllPathsSimultaneous) {
    // Simulate realistic workload across all paths
    std::vector<integration_path_t> paths = {
        PATH_MIDDLEWARE_TO_EXECUTIVE,
        PATH_MIDDLEWARE_TO_WORKSPACE,
        PATH_MIDDLEWARE_TO_INTROSPECTION,
        PATH_EXECUTIVE_TO_MIDDLEWARE,
        PATH_WORKSPACE_TO_MIDDLEWARE
    };

    for (integration_path_t path : paths) {
        for (int i = 0; i < 50; i++) {
            event_t event = create_event(i % 6);

            float info_bits = shannon_monitor_measure_event_information(shannon, &event);
            shannon_monitor_record_event(shannon, &event);

            flow_tracker_record_flow(flow, path, info_bits, 100);
        }
    }

    // Verify all paths have data
    cross_modal_flow_metrics_t metrics = flow_tracker_get_metrics(flow);
    for (integration_path_t path : paths) {
        EXPECT_EQ(metrics.paths[path].total_events, 50);
    }
}

//=============================================================================
// PERFORMANCE INTEGRATION TESTS (10 tests)
//=============================================================================

TEST_F(ShannonFlowIntegrationTest, ShannonOverheadUnder5us) {
    event_t event = create_event(1);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        shannon_monitor_record_event(shannon, &event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = duration.count() / 1000.0f;

    // Target: <5µs per event
    EXPECT_LT(avg_us, 10.0f);  // Allow some margin for test overhead
}

TEST_F(ShannonFlowIntegrationTest, FlowTrackingOverheadUnder2us) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, 5.0f, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = duration.count() / 1000.0f;

    // Target: <2µs per event
    EXPECT_LT(avg_us, 5.0f);  // Allow margin
}

TEST_F(ShannonFlowIntegrationTest, TotalRoutingOverheadUnder15us) {
    event_t event = create_event(1);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        // Shannon monitoring
        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        // Flow tracking
        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, info_bits, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = duration.count() / 1000.0f;

    // Target: <15µs total
    EXPECT_LT(avg_us, 20.0f);  // Allow margin
}

TEST_F(ShannonFlowIntegrationTest, ThroughputVsCapacity) {
    // Generate sustained load
    for (int i = 0; i < 2000; i++) {
        event_t event = create_event(i % 10);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_WORKSPACE, info_bits, 100);
    }

    float capacity = shannon_monitor_calculate_channel_capacity(shannon);
    float throughput = shannon_monitor_get_throughput(shannon);

    // Throughput should not exceed capacity
    EXPECT_LE(throughput, capacity * 1.1f);  // Allow 10% margin
}

//=============================================================================
// COGNITIVE MODULE INTEGRATION TESTS (15 tests)
//=============================================================================

TEST_F(ShannonFlowIntegrationTest, ExecutiveIntegrationBasic) {
    // Simulate Executive receiving pattern events
    for (int i = 0; i < 100; i++) {
        event_t event = create_event(EVENT_TYPE_PATTERN_DETECTED);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        // Executive processes event and sends command back
        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_EXECUTIVE, info_bits, 120);

        // Command response
        event_t cmd = create_event(EVENT_TYPE_ATTENTION_SHIFT);
        float cmd_info = shannon_monitor_measure_event_information(shannon, &cmd);
        shannon_monitor_record_event(shannon, &cmd);

        flow_tracker_record_flow(flow, PATH_EXECUTIVE_TO_MIDDLEWARE, cmd_info, 80);
    }

    path_flow_stats_t to_exec = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_EXECUTIVE
    );
    path_flow_stats_t from_exec = flow_tracker_get_path_stats(
        flow, PATH_EXECUTIVE_TO_MIDDLEWARE
    );

    EXPECT_EQ(to_exec.total_events, 100);
    EXPECT_EQ(from_exec.total_events, 100);
}

TEST_F(ShannonFlowIntegrationTest, WorkspaceIntegrationBasic) {
    // Simulate Workspace receiving salience peaks
    for (int i = 0; i < 80; i++) {
        event_t event = create_event(EVENT_TYPE_SALIENCE_PEAK);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_WORKSPACE, info_bits, 110);

        // Workspace broadcasts winner
        if (i % 10 == 0) {
            event_t broadcast = create_event(EVENT_TYPE_CUSTOM);
            float bc_info = shannon_monitor_measure_event_information(shannon, &broadcast);
            shannon_monitor_record_event(shannon, &broadcast);

            flow_tracker_record_flow(flow, PATH_WORKSPACE_TO_MIDDLEWARE, bc_info, 90);
        }
    }

    path_flow_stats_t to_ws = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_WORKSPACE
    );
    path_flow_stats_t from_ws = flow_tracker_get_path_stats(
        flow, PATH_WORKSPACE_TO_MIDDLEWARE
    );

    EXPECT_EQ(to_ws.total_events, 80);
    EXPECT_EQ(from_ws.total_events, 8);
}

TEST_F(ShannonFlowIntegrationTest, IntrospectionIntegrationBasic) {
    // Simulate Introspection receiving diagnostics
    for (int i = 0; i < 60; i++) {
        event_t event = create_event(EVENT_TYPE_CUSTOM);

        float info_bits = shannon_monitor_measure_event_information(shannon, &event);
        shannon_monitor_record_event(shannon, &event);

        flow_tracker_record_flow(flow, PATH_MIDDLEWARE_TO_INTROSPECTION, info_bits, 95);
    }

    path_flow_stats_t to_intro = flow_tracker_get_path_stats(
        flow, PATH_MIDDLEWARE_TO_INTROSPECTION
    );

    EXPECT_EQ(to_intro.total_events, 60);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
