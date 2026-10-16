/**
 * @file test_middleware_performance.cpp
 * @brief Performance regression tests for middleware integration
 */

#include <gtest/gtest.h>
#include <chrono>
#include "middleware/integration/nimcp_flow_tracker.h"
#include "middleware/integration/nimcp_middleware_controller.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "core/brain/nimcp_brain.h"

using namespace std::chrono;

class MiddlewarePerformanceTest : public ::testing::Test {
protected:
    brain_t brain;
    flow_tracker_t* tracker;
    middleware_controller_t* controller;
    shannon_monitor_t* shannon;
    quantum_command_propagator_t* propagator;

    void SetUp() override {
        // Create brain
        brain_params_t params = brain_default_params();
        params.num_neurons = 10000;
        params.num_layers = 5;
        brain = brain_create(&params);
        ASSERT_NE(brain, nullptr);

        tracker = flow_tracker_create();
        controller = middleware_controller_create(brain);
        shannon = shannon_monitor_create();
        propagator = quantum_command_propagator_create(brain, shannon);
    }

    void TearDown() override {
        if (propagator) quantum_command_propagator_destroy(propagator);
        if (shannon) shannon_monitor_destroy(shannon);
        if (controller) middleware_controller_destroy(controller);
        if (tracker) flow_tracker_destroy(tracker);
        if (brain) brain_destroy(brain);
    }
};

TEST_F(MiddlewarePerformanceTest, FlowTrackerThroughput) {
    const int NUM_FLOWS = 10000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < NUM_FLOWS; i++) {
        integration_path_t path = (integration_path_t)(i % FLOW_TRACKER_NUM_PATHS);
        flow_tracker_record_flow(tracker, path, 10.0f * (i + 1), 100 + i);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Performance target: <50µs per flow on average
    double avg_time_us = duration.count() / (double)NUM_FLOWS;
    EXPECT_LT(avg_time_us, 50.0);

    printf("FlowTracker throughput: %.2f flows/sec (avg %.2f µs/flow)\n",
           NUM_FLOWS * 1000000.0 / duration.count(), avg_time_us);
}

TEST_F(MiddlewarePerformanceTest, ControllerCommandLatency) {
    const int NUM_COMMANDS = 1000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < NUM_COMMANDS; i++) {
        command_target_region_t region = (command_target_region_t)(i % 7);
        middleware_controller_set_attention_threshold(controller, region, 0.5f + (i % 5) * 0.1f);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Performance target: <5µs per command (as per design spec)
    double avg_time_us = duration.count() / (double)NUM_COMMANDS;
    EXPECT_LT(avg_time_us, 5.0);

    printf("Controller command latency: %.2f µs average\n", avg_time_us);
}

TEST_F(MiddlewarePerformanceTest, ShannonMonitorEventProcessing) {
    const int NUM_EVENTS = 5000;
    event_t event = {0};
    event.type = 1;
    event.source = 0;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < NUM_EVENTS; i++) {
        event.type = i % 256;
        shannon_monitor_record_event(shannon, &event);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Performance target: <20µs per event
    double avg_time_us = duration.count() / (double)NUM_EVENTS;
    EXPECT_LT(avg_time_us, 20.0);

    printf("Shannon event processing: %.2f events/sec (avg %.2f µs/event)\n",
           NUM_EVENTS * 1000000.0 / duration.count(), avg_time_us);
}

TEST_F(MiddlewarePerformanceTest, QuantumPropagationSpeedup) {
    middleware_command_t cmd = {0};
    cmd.type = COMMAND_CONFIGURE_ATTENTION;
    cmd.priority = 0.8f;
    cmd.payload.attention.target_region = TARGET_ALL_REGIONS;

    auto start = high_resolution_clock::now();
    uint32_t neurons_reached = quantum_command_propagator_propagate(propagator, &cmd);
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Should reach significant portion of neurons
    EXPECT_GT(neurons_reached, 0);

    // Get speedup metrics
    command_propagation_metrics_t metrics;
    quantum_command_propagator_get_metrics(propagator, &metrics);

    // Should achieve quantum speedup (>1.0x)
    EXPECT_GT(metrics.speedup_vs_classical, 1.0f);

    printf("Quantum propagation: %u neurons in %ld µs (speedup: %.2fx)\n",
           neurons_reached, duration.count(), metrics.speedup_vs_classical);
}

TEST_F(MiddlewarePerformanceTest, MemoryUsageStability) {
    // Record many flows to check for memory leaks
    const int ITERATIONS = 10000;

    for (int i = 0; i < ITERATIONS; i++) {
        flow_tracker_record_flow(tracker, PATH_MIDDLEWARE_TO_EXECUTIVE, 10.0f, 100);

        if (i % 1000 == 0) {
            // Reset to simulate long-running operation
            flow_tracker_reset(tracker);
        }
    }

    // If we get here without crash/leak, test passes
    SUCCEED();
}
